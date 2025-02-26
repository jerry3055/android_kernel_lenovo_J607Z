/*
 * ASIX AX88179/178A USB 3.0/2.0 to Gigabit Ethernet Devices
 *
 * Copyright (C) 2011-2013 ASIX
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (c) 2022    ASIX Electronic Corporation    All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "ax_main.h"
#include "ax88179_178a.h"

struct _ax_buikin_setting AX88179_BULKIN_SIZE[] = {
	{7, 0x70, 0,	0x0C, 0x0f},
	{7, 0x70, 0,	0x0C, 0x0f},
	{7, 0x20, 3,	0x16, 0xff},
	{7, 0xae, 7,	0x18, 0xff},
};
const struct ethtool_ops ax88179_ethtool_ops = {
	.get_drvinfo	= ax_get_drvinfo,
#if KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE
	.get_settings	= ax_get_settings,
	.set_settings	= ax_set_settings,
#else
	.get_link_ksettings = ax_get_link_ksettings,
	.set_link_ksettings = ax_set_link_ksettings,
#endif
	.get_link	= ethtool_op_get_link,
	.get_msglevel	= ax_get_msglevel,
	.set_msglevel	= ax_set_msglevel,
	.get_wol	= ax_get_wol,
	.set_wol	= ax_set_wol,
	.get_ts_info	= ethtool_op_get_ts_info,
	.get_strings	= ax_get_strings,
	.get_sset_count = ax_get_sset_count,
	.get_ethtool_stats = ax_get_ethtool_stats,
	.get_regs_len	= ax_get_regs_len,
	.get_regs	= ax_get_regs,
};

int ax88179_signature(struct ax_device *axdev, struct _ax_ioctl_command *info)
{
	strlcpy(info->sig, AX88179_SIGNATURE, sizeof(info->sig));
	return 0;
}

int ax88179_read_eeprom(struct ax_device *axdev, struct _ax_ioctl_command *info)
{
	u8 i;
	u16 tmp;
	u8 value;
	unsigned short *buf;

	if (info->buf != NULL) {
		buf = kmalloc_array(info->size, sizeof(unsigned short),
				    GFP_KERNEL);
		if (!buf) {
#if KERNEL_VERSION(2, 6, 34) <= LINUX_VERSION_CODE
			netdev_err(axdev->netdev,
				   "Cannot allocate memory for buffer");
#endif
			return -ENOMEM;
		}
	} else {
		netdev_info(axdev->netdev,
			    "The EEPROM buffer cannot be NULL. \r\n");
		return -EINVAL;
	}

	if (info->type == 0) {
		for (i = 0; i < info->size; i++) {

			if (ax_write_cmd(axdev, AX_ACCESS_MAC,
					      AX_SROM_ADDR, 1, 1, &i) < 0) {
				kfree(buf);
				return -EINVAL;
			}

			value = EEP_RD;
			if (ax_write_cmd(axdev, AX_ACCESS_MAC,
					      AX_SROM_CMD, 1, 1, &value) < 0) {
				kfree(buf);
				return -EINVAL;
			}

			do {
				ax_read_cmd(axdev, AX_ACCESS_MAC,
						 AX_SROM_CMD, 1, 1, &value, 0);
			} while (value & EEP_BUSY);

			if (ax_read_cmd(axdev, AX_ACCESS_MAC,
					     AX_SROM_DATA_LOW, 2, 2,
					     &tmp, 1) < 0) {
				kfree(buf);
				return -EINVAL;
			}

			*(buf + i) = be16_to_cpu(tmp);

			if (i == (info->size - 1))
				break;
		}
	} else {
		ret = __ax88179_write_cmd(dev, cmd, value, index,
					  size, data, 0);
	}

	return ret;
}

static void ax88179_status(struct usbnet *dev, struct urb *urb)
{
	struct ax88179_int_data *event;
	u32 link;

	if (urb->actual_length < 8)
		return;

	event = urb->transfer_buffer;
	le32_to_cpus((void *)&event->intdata1);

	link = (((__force u32)event->intdata1) & AX_INT_PPLS_LINK) >> 16;

	if (netif_carrier_ok(dev->net) != link) {
		usbnet_link_change(dev, link, 1);
		if (!link)
			netdev_info(dev->net, "ax88179 - Link status is: 0\n");
	}
}

static int ax88179_mdio_read(struct net_device *netdev, int phy_id, int loc)
{
	struct usbnet *dev = netdev_priv(netdev);
	u16 res;

	ax88179_read_cmd(dev, AX_ACCESS_PHY, phy_id, (__u16)loc, 2, &res);
	return res;
}

int ax88179_write_eeprom(struct ax_device *axdev,
			 struct _ax_ioctl_command *info)
{
	struct usbnet *dev = netdev_priv(netdev);
	u16 res = (u16) val;

	ax88179_write_cmd(dev, AX_ACCESS_PHY, phy_id, (__u16)loc, 2, &res);
}

static inline int ax88179_phy_mmd_indirect(struct usbnet *dev, u16 prtad,
					   u16 devad)
{
	u16 tmp16;
	int ret;

	tmp16 = devad;
	ret = ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				MII_MMD_CTRL, 2, &tmp16);

	tmp16 = prtad;
	ret = ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				MII_MMD_DATA, 2, &tmp16);

	tmp16 = devad | MII_MMD_CTRL_NOINCR;
	ret = ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				MII_MMD_CTRL, 2, &tmp16);

	return ret;
}

static int
ax88179_phy_read_mmd_indirect(struct usbnet *dev, u16 prtad, u16 devad)
{
	int ret;
	u16 tmp16;

	ax88179_phy_mmd_indirect(dev, prtad, devad);

	ret = ax88179_read_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
			       MII_MMD_DATA, 2, &tmp16);
	if (ret < 0)
		return ret;

	return tmp16;
}

static int
ax88179_phy_write_mmd_indirect(struct usbnet *dev, u16 prtad, u16 devad,
			       u16 data)
{
	int ret;

	ax88179_phy_mmd_indirect(dev, prtad, devad);

	ret = ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				MII_MMD_DATA, 2, &data);

	if (ret < 0)
		return ret;

	return 0;
}

static int ax88179_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct usbnet *dev = usb_get_intfdata(intf);
	u16 tmp16;
	u8 tmp8;

	usbnet_suspend(intf, message);

	/* Disable RX path */
	ax88179_read_cmd_nopm(dev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
			      2, 2, &tmp16);
	tmp16 &= ~AX_MEDIUM_RECEIVE_EN;
	ax88179_write_cmd_nopm(dev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
			       2, 2, &tmp16);

	/* Force bulk-in zero length */
	ax88179_read_cmd_nopm(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL,
			      2, 2, &tmp16);

	tmp16 |= AX_PHYPWR_RSTCTL_BZ | AX_PHYPWR_RSTCTL_IPRL;
	ax88179_write_cmd_nopm(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL,
			       2, 2, &tmp16);

	/* change clock */
	tmp8 = 0;
	ax88179_write_cmd_nopm(dev, AX_ACCESS_MAC, AX_CLK_SELECT, 1, 1, &tmp8);

	/* Configure RX control register => stop operation */
	tmp16 = AX_RX_CTL_STOP;
	ax88179_write_cmd_nopm(dev, AX_ACCESS_MAC, AX_RX_CTL, 2, 2, &tmp16);

	return 0;
}

/* This function is used to enable the autodetach function. */
/* This function is determined by offset 0x43 of EEPROM */
static int ax88179_auto_detach(struct usbnet *dev, int in_pm)
{
	u16 tmp16;
	u8 tmp8;
	int (*fnr)(struct usbnet *, u8, u16, u16, u16, void *);
	int (*fnw)(struct usbnet *, u8, u16, u16, u16, void *);

	if (!in_pm) {
		fnr = ax88179_read_cmd;
		fnw = ax88179_write_cmd;
	} else {
		fnr = ax88179_read_cmd_nopm;
		fnw = ax88179_write_cmd_nopm;
	}

	if (fnr(dev, AX_ACCESS_EEPROM, 0x43, 1, 2, &tmp16) < 0)
		return 0;

	if ((tmp16 == 0xFFFF) || (!(tmp16 & 0x0100)))
		return 0;

	/* Enable Auto Detach bit */
	tmp8 = 0;
	fnr(dev, AX_ACCESS_MAC, AX_CLK_SELECT, 1, 1, &tmp8);
	tmp8 |= AX_CLK_SELECT_ULR;
	fnw(dev, AX_ACCESS_MAC, AX_CLK_SELECT, 1, 1, &tmp8);

	fnr(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, &tmp16);
	tmp16 |= AX_PHYPWR_RSTCTL_AT;
	fnw(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, &tmp16);

	return 0;
}

static int ax88179_resume(struct usb_interface *intf)
{
	struct usbnet *dev = usb_get_intfdata(intf);
	u16 tmp16;
	u8 tmp8;

	usbnet_link_change(dev, 0, 0);

	/* Power up ethernet PHY */
	tmp16 = 0;
	ax88179_write_cmd_nopm(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL,
			       2, 2, &tmp16);
	udelay(1000);

	tmp16 = AX_PHYPWR_RSTCTL_IPRL;
	ax88179_write_cmd_nopm(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL,
			       2, 2, &tmp16);
	msleep(200);

	/* Ethernet PHY Auto Detach*/
	ax88179_auto_detach(dev, 1);

	/* Enable clock */
	ax88179_read_cmd_nopm(dev, AX_ACCESS_MAC,  AX_CLK_SELECT, 1, 1, &tmp8);
	tmp8 |= AX_CLK_SELECT_ACS | AX_CLK_SELECT_BCS;
	ax88179_write_cmd_nopm(dev, AX_ACCESS_MAC, AX_CLK_SELECT, 1, 1, &tmp8);
	msleep(100);

	/* Configure RX control register => start operation */
	tmp16 = AX_RX_CTL_DROPCRCERR | AX_RX_CTL_IPE | AX_RX_CTL_START |
		AX_RX_CTL_AP | AX_RX_CTL_AMALL | AX_RX_CTL_AB;
	ax88179_write_cmd_nopm(dev, AX_ACCESS_MAC, AX_RX_CTL, 2, 2, &tmp16);

	return usbnet_resume(intf);
}

static void
ax88179_get_wol(struct net_device *net, struct ethtool_wolinfo *wolinfo)
{
	struct usbnet *dev = netdev_priv(net);
	u8 opt;

	if (ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_MONITOR_MOD,
			     1, 1, &opt) < 0) {
		wolinfo->supported = 0;
		wolinfo->wolopts = 0;
		return;
	}

	wolinfo->supported = WAKE_PHY | WAKE_MAGIC;
	wolinfo->wolopts = 0;
	if (opt & AX_MONITOR_MODE_RWLC)
		wolinfo->wolopts |= WAKE_PHY;
	if (opt & AX_MONITOR_MODE_RWMP)
		wolinfo->wolopts |= WAKE_MAGIC;
}

static int
ax88179_set_wol(struct net_device *net, struct ethtool_wolinfo *wolinfo)
{
	struct usbnet *dev = netdev_priv(net);
	u8 opt = 0;

	if (wolinfo->wolopts & ~(WAKE_PHY | WAKE_MAGIC))
		return -EINVAL;

	if (wolinfo->wolopts & WAKE_PHY)
		opt |= AX_MONITOR_MODE_RWLC;
	if (wolinfo->wolopts & WAKE_MAGIC)
		opt |= AX_MONITOR_MODE_RWMP;

	if (ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_MONITOR_MOD,
			      1, 1, &opt) < 0)
		return -EINVAL;

	return 0;
}

static int ax88179_get_eeprom_len(struct net_device *net)
{
	return AX_EEPROM_LEN;
}

static int
ax88179_get_eeprom(struct net_device *net, struct ethtool_eeprom *eeprom,
		   u8 *data)
{
	struct usbnet *dev = netdev_priv(net);
	u16 *eeprom_buff;
	int first_word, last_word;
	int i, ret;

	if (eeprom->len == 0)
		return -EINVAL;

	eeprom->magic = AX88179_EEPROM_MAGIC;

	first_word = eeprom->offset >> 1;
	last_word = (eeprom->offset + eeprom->len - 1) >> 1;
	eeprom_buff = kmalloc_array(last_word - first_word + 1, sizeof(u16),
				    GFP_KERNEL);
	if (!eeprom_buff)
		return -ENOMEM;

	/* ax88179/178A returns 2 bytes from eeprom on read */
	for (i = first_word; i <= last_word; i++) {
		ret = __ax88179_read_cmd(dev, AX_ACCESS_EEPROM, i, 1, 2,
					 &eeprom_buff[i - first_word],
					 0);
		if (ret < 0) {
			kfree(eeprom_buff);
			return -EIO;
		}
	}

	memcpy(data, (u8 *)eeprom_buff + (eeprom->offset & 1), eeprom->len);
	kfree(eeprom_buff);
	return 0;
}

static int ax88179_get_link_ksettings(struct net_device *net,
				      struct ethtool_link_ksettings *cmd)
{
	struct usbnet *dev = netdev_priv(net);

	mii_ethtool_get_link_ksettings(&dev->mii, cmd);

	return 0;
}

static int ax88179_set_link_ksettings(struct net_device *net,
				      const struct ethtool_link_ksettings *cmd)
{
	struct usbnet *dev = netdev_priv(net);
	return mii_ethtool_set_link_ksettings(&dev->mii, cmd);
}

static int
ax88179_ethtool_get_eee(struct usbnet *dev, struct ethtool_eee *data)
{
	int val;

	/* Get Supported EEE */
	val = ax88179_phy_read_mmd_indirect(dev, MDIO_PCS_EEE_ABLE,
					    MDIO_MMD_PCS);
	if (val < 0)
		return val;
	data->supported = mmd_eee_cap_to_ethtool_sup_t(val);

	/* Get advertisement EEE */
	val = ax88179_phy_read_mmd_indirect(dev, MDIO_AN_EEE_ADV,
					    MDIO_MMD_AN);
	if (val < 0)
		return val;
	data->advertised = mmd_eee_adv_to_ethtool_adv_t(val);

	/* Get LP advertisement EEE */
	val = ax88179_phy_read_mmd_indirect(dev, MDIO_AN_EEE_LPABLE,
					    MDIO_MMD_AN);
	if (val < 0)
		return val;
	data->lp_advertised = mmd_eee_adv_to_ethtool_adv_t(val);

	return 0;
}

static int
ax88179_ethtool_set_eee(struct usbnet *dev, struct ethtool_eee *data)
{
	u16 tmp16 = ethtool_adv_to_mmd_eee_adv_t(data->advertised);

	return ax88179_phy_write_mmd_indirect(dev, MDIO_AN_EEE_ADV,
					      MDIO_MMD_AN, tmp16);
}

static int ax88179_chk_eee(struct usbnet *dev)
{
	struct ethtool_cmd ecmd = { .cmd = ETHTOOL_GSET };
	struct ax88179_data *priv = (struct ax88179_data *)dev->data;

	mii_ethtool_gset(&dev->mii, &ecmd);

	if (ecmd.duplex & DUPLEX_FULL) {
		int eee_lp, eee_cap, eee_adv;
		u32 lp, cap, adv, supported = 0;

		eee_cap = ax88179_phy_read_mmd_indirect(dev,
							MDIO_PCS_EEE_ABLE,
							MDIO_MMD_PCS);
		if (eee_cap < 0) {
			priv->eee_active = 0;
			return false;
		}

		cap = mmd_eee_cap_to_ethtool_sup_t(eee_cap);
		if (!cap) {
			priv->eee_active = 0;
			return false;
		}

		eee_lp = ax88179_phy_read_mmd_indirect(dev,
						       MDIO_AN_EEE_LPABLE,
						       MDIO_MMD_AN);
		if (eee_lp < 0) {
			priv->eee_active = 0;
			return false;
		}

		eee_adv = ax88179_phy_read_mmd_indirect(dev,
							MDIO_AN_EEE_ADV,
							MDIO_MMD_AN);

		if (eee_adv < 0) {
			priv->eee_active = 0;
			return false;
		}

		adv = mmd_eee_adv_to_ethtool_adv_t(eee_adv);
		lp = mmd_eee_adv_to_ethtool_adv_t(eee_lp);
		supported = (ecmd.speed == SPEED_1000) ?
			     SUPPORTED_1000baseT_Full :
			     SUPPORTED_100baseT_Full;

		if (!(lp & adv & supported)) {
			priv->eee_active = 0;
			return false;
		}

		priv->eee_active = 1;
		return true;
	}

	priv->eee_active = 0;
	return false;
}

static void ax88179_disable_eee(struct usbnet *dev)
{
	u16 tmp16;

	tmp16 = GMII_PHY_PGSEL_PAGE3;
	ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  GMII_PHY_PAGE_SELECT, 2, &tmp16);

	tmp16 = 0x3246;
	ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  MII_PHYADDR, 2, &tmp16);

	tmp16 = GMII_PHY_PGSEL_PAGE0;
	ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  GMII_PHY_PAGE_SELECT, 2, &tmp16);
}

static void ax88179_enable_eee(struct usbnet *dev)
{
	u16 tmp16;

	tmp16 = GMII_PHY_PGSEL_PAGE3;
	ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  GMII_PHY_PAGE_SELECT, 2, &tmp16);

	tmp16 = 0x3247;
	ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  MII_PHYADDR, 2, &tmp16);

	tmp16 = GMII_PHY_PGSEL_PAGE5;
	ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  GMII_PHY_PAGE_SELECT, 2, &tmp16);

	tmp16 = 0x0680;
	ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  MII_BMSR, 2, &tmp16);

	tmp16 = GMII_PHY_PGSEL_PAGE0;
	ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  GMII_PHY_PAGE_SELECT, 2, &tmp16);
}

static int ax88179_get_eee(struct net_device *net, struct ethtool_eee *edata)
{
	struct usbnet *dev = netdev_priv(net);
	struct ax88179_data *priv = (struct ax88179_data *)dev->data;

	edata->eee_enabled = priv->eee_enabled;
	edata->eee_active = priv->eee_active;

	return ax88179_ethtool_get_eee(dev, edata);
}

static int ax88179_set_eee(struct net_device *net, struct ethtool_eee *edata)
{
	struct usbnet *dev = netdev_priv(net);
	struct ax88179_data *priv = (struct ax88179_data *)dev->data;
	int ret = -EOPNOTSUPP;

	priv->eee_enabled = edata->eee_enabled;
	if (!priv->eee_enabled) {
		ax88179_disable_eee(dev);
	} else {
		priv->eee_enabled = ax88179_chk_eee(dev);
		if (!priv->eee_enabled)
			return -EOPNOTSUPP;

		ax88179_enable_eee(dev);
	}

	ret = ax88179_ethtool_set_eee(dev, edata);
	if (ret)
		return ret;

	mii_nway_restart(&dev->mii);

	usbnet_link_change(dev, 0, 0);

	return ret;
}

static int ax88179_ioctl(struct net_device *net, struct ifreq *rq, int cmd)
{
	struct usbnet *dev = netdev_priv(net);
	return generic_mii_ioctl(&dev->mii, if_mii(rq), cmd, NULL);
}

static const struct ethtool_ops ax88179_ethtool_ops = {
	.get_link		= ethtool_op_get_link,
	.get_msglevel		= usbnet_get_msglevel,
	.set_msglevel		= usbnet_set_msglevel,
	.get_wol		= ax88179_get_wol,
	.set_wol		= ax88179_set_wol,
	.get_eeprom_len		= ax88179_get_eeprom_len,
	.get_eeprom		= ax88179_get_eeprom,
	.get_eee		= ax88179_get_eee,
	.set_eee		= ax88179_set_eee,
	.nway_reset		= usbnet_nway_reset,
	.get_link_ksettings	= ax88179_get_link_ksettings,
	.set_link_ksettings	= ax88179_set_link_ksettings,
};

static void ax88179_set_multicast(struct net_device *net)
{
	struct usbnet *dev = netdev_priv(net);
	struct ax88179_data *data = (struct ax88179_data *)dev->data;
	u8 *m_filter = ((u8 *)dev->data) + 12;

	data->rxctl = (AX_RX_CTL_START | AX_RX_CTL_AB | AX_RX_CTL_IPE);

	if (net->flags & IFF_PROMISC) {
		axdev->rxctl |= AX_RX_CTL_PRO;
	} else if (net->flags & IFF_ALLMULTI
		   || mc_count > AX_MAX_MCAST) {
		axdev->rxctl |= AX_RX_CTL_AMALL;
	} else if (mc_count == 0) {
	} else {
		u32 crc_bits;
#if KERNEL_VERSION(2, 6, 35) > LINUX_VERSION_CODE
		struct dev_mc_list *mc_list = net->mc_list;
		int i = 0;

		memset(m_filter, 0, AX_MCAST_FILTER_SIZE);

		for (i = 0; i < net->mc_count; i++) {
			crc_bits = ether_crc(ETH_ALEN,
					     mc_list->dmi_addr) >> 26;
			*(m_filter + (crc_bits >> 3)) |=
				1 << (crc_bits & 7);
			mc_list = mc_list->next;
		}
#else
		struct netdev_hw_addr *ha = NULL;

		memset(m_filter, 0, AX_MCAST_FILTER_SIZE);
		netdev_for_each_mc_addr(ha, net) {
			crc_bits = ether_crc(ETH_ALEN, ha->addr) >> 26;
			*(m_filter + (crc_bits >> 3)) |=
				1 << (crc_bits & 7);
		}
#endif
		ax_write_cmd_async(axdev, AX_ACCESS_MAC,
					AX_MULTI_FILTER_ARRY,
					AX_MCAST_FILTER_SIZE,
					AX_MCAST_FILTER_SIZE, m_filter);

		axdev->rxctl |= AX_RX_CTL_AM;
	}

	ax_write_cmd_async(axdev, AX_ACCESS_MAC, AX_RX_CTL,
				2, 2, &axdev->rxctl);
}

int ax88179_set_mac_addr(struct net_device *netdev, void *p)
{
	struct ax_device *axdev = netdev_priv(netdev);
	struct sockaddr *addr = p;
	int ret;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	if (netif_running(netdev))
		return -EBUSY;

	memcpy(netdev->dev_addr, addr->sa_data, ETH_ALEN);

	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX_NODE_ID, ETH_ALEN,
			   ETH_ALEN, netdev->dev_addr);
	if (ret < 0)
		return ret;

	return 0;

}

static int ax88179_check_eeprom(struct ax_device *axdev)
{
	u8 i = 0;
	u8 buf[2];
	u8 eeprom[20];
	u16 csum = 0, delay = HZ / 10;

	for (i = 0 ; i < 6; i++) {
		buf[0] = i;
		if (ax_write_cmd(axdev, AX_ACCESS_MAC, AX_SROM_ADDR,
				      1, 1, buf) < 0)
			return -EINVAL;

		buf[0] = EEP_RD;
		if (ax_write_cmd(axdev, AX_ACCESS_MAC, AX_SROM_CMD,
				      1, 1, buf) < 0)
			return -EINVAL;

		do {
			ax_read_cmd(axdev, AX_ACCESS_MAC, AX_SROM_CMD,
					 1, 1, buf, 0);

			if (time_after(jiffies, (jiffies + delay)))
				return -EINVAL;
		} while (buf[0] & EEP_BUSY);

		ax_read_cmd(axdev, AX_ACCESS_MAC, AX_SROM_DATA_LOW,
				 2, 2, &eeprom[i * 2], 0);

		if ((i == 0) && (eeprom[0] == 0xFF))
			return -EINVAL;
	}

	csum = eeprom[6] + eeprom[7] + eeprom[8] + eeprom[9];
	csum = (csum >> 8) + (csum & 0xff);

	if ((csum + eeprom[10]) == 0xff)
		return 0;
	else
		return -EINVAL;

	return 0;
}

static int ax88179_check_efuse(struct ax_device *axdev, void *ledmode)
{
	u8	i = 0;
	u16	csum = 0;
	u8	efuse[64];

	if (ax_read_cmd(axdev, AX_ACCESS_EFUSE, 0, 64, 64, efuse, 0) < 0)
		return -EINVAL;

	if (efuse[0] == 0xFF)
		return -EINVAL;

	for (i = 0; i < 64; i++)
		csum = csum + efuse[i];

	while (csum > 255)
		csum = (csum & 0x00FF) + ((csum >> 8) & 0x00FF);

	if (csum == 0xFF) {
		memcpy((u8 *)ledmode, &efuse[51], 2);
		return 0;
	} else
		return -EINVAL;

	return 0;
}

static int ax88179_convert_old_led(struct ax_device *axdev, u8 efuse, void *ledvalue)
{
	u8 ledmode = 0;
	u16 reg16;
	u16 led = 0;

	/* loaded the old eFuse LED Mode */
	if (efuse) {
		if (ax_read_cmd(axdev, AX_ACCESS_EFUSE, 0x18,
				     1, 2, &reg16, 1) < 0)
			return -EINVAL;
		ledmode = (u8)(reg16 & 0xFF);
	} else { /* loaded the old EEprom LED Mode */
		if (ax_read_cmd(axdev, AX_ACCESS_EEPROM, 0x3C,
				     1, 2, &reg16, 1) < 0)
			return -EINVAL;
		ledmode = (u8) (reg16 >> 8);
	}
	netdev_dbg(axdev->netdev, "Old LED Mode = %02X\n", ledmode);

	switch (ledmode) {
	case 0xFF:
		led = LED0_ACTIVE | LED1_LINK_10 | LED1_LINK_100 |
		      LED1_LINK_1000 | LED2_ACTIVE | LED2_LINK_10 |
		      LED2_LINK_100 | LED2_LINK_1000 | LED_VALID;
		break;
	case 0xFE:
		led = LED0_ACTIVE | LED1_LINK_1000 | LED2_LINK_100 | LED_VALID;
		break;
	case 0xFD:
		led = LED0_ACTIVE | LED1_LINK_1000 | LED2_LINK_100 |
		      LED2_LINK_10 | LED_VALID;
		break;
	case 0xFC:
		led = LED0_ACTIVE | LED1_ACTIVE | LED1_LINK_1000 | LED2_ACTIVE |
		      LED2_LINK_100 | LED2_LINK_10 | LED_VALID;
		break;
	default:
		led = LED0_ACTIVE | LED1_LINK_10 | LED1_LINK_100 |
		      LED1_LINK_1000 | LED2_ACTIVE | LED2_LINK_10 |
		      LED2_LINK_100 | LED2_LINK_1000 | LED_VALID;
		break;
	}

	memcpy((u8 *)ledvalue, &led, 2);

	return 0;
}

static void ax88179_Gether_setting(struct ax_device *axdev)
{
	u16 reg16;

	reg16 = 0x03;
	ax_write_cmd(axdev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  31, 2, &reg16);
	reg16 = 0x3246;
	ax_write_cmd(axdev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  25, 2, &reg16);
	reg16 = 0;
	ax_write_cmd(axdev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  31, 2, &reg16);
}

static int ax88179_LED_setting(struct ax_device *axdev)
{
	u16 ledvalue = 0, delay = HZ / 10;
	u16 ledact, ledlink;
	u16 reg16;
	u8 value;

	ax_read_cmd(axdev, AX_ACCESS_MAC, GENERAL_STATUS, 1, 1, &value, 0);

	if (!(value & AX_SECLD)) {
		value = AX_GPIO_CTRL_GPIO3EN | AX_GPIO_CTRL_GPIO2EN |
			AX_GPIO_CTRL_GPIO1EN;
		if (ax_write_cmd(axdev, AX_ACCESS_MAC, AX_GPIO_CTRL,
				      1, 1, &value) < 0)
			return -EINVAL;
	}

	if (!ax88179_check_eeprom(axdev)) {
		value = 0x42;
		if (ax_write_cmd(axdev, AX_ACCESS_MAC, AX_SROM_ADDR,
				      1, 1, &value) < 0)
			return -EINVAL;

		value = EEP_RD;
		if (ax_write_cmd(axdev, AX_ACCESS_MAC, AX_SROM_CMD,
				      1, 1, &value) < 0)
			return -EINVAL;

		do {
			ax_read_cmd(axdev, AX_ACCESS_MAC, AX_SROM_CMD,
					 1, 1, &value, 0);

			ax_read_cmd(axdev, AX_ACCESS_MAC, AX_SROM_CMD,
					 1, 1, &value, 0);

			if (time_after(jiffies, (jiffies + delay)))
				return -EINVAL;
		} while (value & EEP_BUSY);

		ax_read_cmd(axdev, AX_ACCESS_MAC, AX_SROM_DATA_HIGH,
				 1, 1, &value, 0);
		ledvalue = (value << 8);
		ax_read_cmd(axdev, AX_ACCESS_MAC, AX_SROM_DATA_LOW,
				 1, 1, &value, 0);
		ledvalue |= value;

		if ((ledvalue == 0xFFFF) || ((ledvalue & LED_VALID) == 0))
			ax88179_convert_old_led(axdev, 0, &ledvalue);

	} else if (!ax88179_check_efuse(axdev, &ledvalue)) {
		if ((ledvalue == 0xFFFF) || ((ledvalue & LED_VALID) == 0))
			ax88179_convert_old_led(axdev, 0, &ledvalue);
	} else {
		ax88179_convert_old_led(axdev, 0, &ledvalue);
	}

	reg16 = GMII_PHY_PAGE_SELECT_EXT;
	ax_write_cmd(axdev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  GMII_PHY_PAGE_SELECT, 2, &reg16);

	reg16 = 0x2c;
	ax_write_cmd(axdev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  GMII_PHYPAGE, 2, &reg16);

	ax_read_cmd(axdev, AX_ACCESS_PHY, AX88179_PHY_ID,
			 GMII_LED_ACTIVE, 2, &ledact, 1);

	ax_read_cmd(axdev, AX_ACCESS_PHY, AX88179_PHY_ID,
			 GMII_LED_LINK, 2, &ledlink, 1);

	ledact &= GMII_LED_ACTIVE_MASK;
	ledlink &= GMII_LED_LINK_MASK;

	if (ledvalue & LED0_ACTIVE)
		ledact |= GMII_LED0_ACTIVE;
	if (ledvalue & LED1_ACTIVE)
		ledact |= GMII_LED1_ACTIVE;
	if (ledvalue & LED2_ACTIVE)
		ledact |= GMII_LED2_ACTIVE;

	if (ledvalue & LED0_LINK_10)
		ledlink |= GMII_LED0_LINK_10;
	if (ledvalue & LED1_LINK_10)
		ledlink |= GMII_LED1_LINK_10;
	if (ledvalue & LED2_LINK_10)
		ledlink |= GMII_LED2_LINK_10;

	if (ledvalue & LED0_LINK_100)
		ledlink |= GMII_LED0_LINK_100;
	if (ledvalue & LED1_LINK_100)
		ledlink |= GMII_LED1_LINK_100;
	if (ledvalue & LED2_LINK_100)
		ledlink |= GMII_LED2_LINK_100;

	if (ledvalue & LED0_LINK_1000)
		ledlink |= GMII_LED0_LINK_1000;
	if (ledvalue & LED1_LINK_1000)
		ledlink |= GMII_LED1_LINK_1000;
	if (ledvalue & LED2_LINK_1000)
		ledlink |= GMII_LED2_LINK_1000;

	ax_write_cmd(axdev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  GMII_LED_ACTIVE, 2, &ledact);

	ax_write_cmd(axdev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  GMII_LED_LINK, 2, &ledlink);

	reg16 = GMII_PHY_PAGE_SELECT_PAGE0;
	ax_write_cmd(axdev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  GMII_PHY_PAGE_SELECT, 2, &reg16);

	/* LED full duplex setting */
	reg16 = 0;
	if (ledvalue & LED0_FD)
		reg16 |= 0x01;
	else if ((ledvalue & LED0_USB3_MASK) == 0)
		reg16 |= 0x02;

	if (ledvalue & LED1_FD)
		reg16 |= 0x04;
	else if ((ledvalue & LED1_USB3_MASK) == 0)
		reg16 |= 0x08;

	if (ledvalue & LED2_FD) /* LED2_FD */
		reg16 |= 0x10;
	else if ((ledvalue & LED2_USB3_MASK) == 0) /* LED2_USB3 */
		reg16 |= 0x20;

	ax_write_cmd(axdev, AX_ACCESS_MAC, 0x73, 1, 1, &reg16);

	return 0;
}

static void ax88179_EEE_setting(struct ax_device *axdev)
{
	u16 reg16;
	/* Disable */
	reg16 = 0x07;
	ax_write_cmd(axdev, AX_ACCESS_PHY, AX88179_PHY_ID,
				GMII_PHY_MACR, 2, &reg16);
	reg16 = 0x3c;
	ax_write_cmd(axdev, AX_ACCESS_PHY, AX88179_PHY_ID,
				GMII_PHY_MAADR, 2, &reg16);
	reg16 = 0x4007;
	ax_write_cmd(axdev, AX_ACCESS_PHY, AX88179_PHY_ID,
				GMII_PHY_MACR, 2, &reg16);
	reg16 = 0x00;
	ax_write_cmd(axdev, AX_ACCESS_PHY, AX88179_PHY_ID,
				GMII_PHY_MAADR, 2, &reg16);
}

static int ax88179_AutoDetach(struct ax_device *axdev, int in_pm)
{
	u16 reg16;
	usb_read_function fnr;
	usb_write_function fnw;

	if (!in_pm) {
		fnr = ax_read_cmd;
		fnw = ax_write_cmd;
	} else {
		fnr = ax_read_cmd_nopm;
		fnw = ax_write_cmd_nopm;
	}

	if (fnr(axdev, AX_ACCESS_EEPROM, 0x43, 1, 2, &reg16, 1) < 0)
		return 0;

	if ((reg16 == 0xFFFF) || (!(reg16 & 0x0100)))
		return 0;

	reg16 = 0;
	fnr(axdev, AX_ACCESS_MAC, AX_CLK_SELECT, 1, 1, &reg16, 0);
	reg16 |= AX_CLK_SELECT_ULR;
	fnw(axdev, AX_ACCESS_MAC, AX_CLK_SELECT, 1, 1, &reg16);

	fnr(axdev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, &reg16, 1);
	reg16 |= AX_PHYPWR_RSTCTL_AUTODETACH;
	fnw(axdev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, &reg16);

	return 0;
}

static int ax88179_hw_init(struct ax_device *axdev)
{
	u32 reg32;
	u16 reg16;
	u8 reg8;
	u8 buf[6] = {0};

	reg32 = 0;
	ax_write_cmd(axdev, 0x81, 0x310, 0, 4, &reg32);

	reg16 = 0;
	ax_write_cmd(axdev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, &reg16);
	reg16 = AX_PHYPWR_RSTCTL_IPRL;
	ax_write_cmd(axdev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, &reg16);
	msleep(200);

	reg8 = AX_CLK_SELECT_ACS | AX_CLK_SELECT_BCS;
	ax_write_cmd(axdev, AX_ACCESS_MAC, AX_CLK_SELECT, 1, 1, &reg8);
	msleep(100);

	ax88179_AutoDetach(axdev, 0);

	memcpy(buf, &AX88179_BULKIN_SIZE[0], 5);
	ax_write_cmd(axdev, AX_ACCESS_MAC, AX_RX_BULKIN_QCTRL, 5, 5, buf);

	reg8 = 0x34;
	ax_write_cmd(axdev, AX_ACCESS_MAC, AX_PAUSE_WATERLVL_LOW,
			  1, 1, &reg8);

	reg8 = 0x52;
	ax_write_cmd(axdev, AX_ACCESS_MAC, AX_PAUSE_WATERLVL_HIGH,
			  1, 1, &reg8);

	ax_write_cmd(axdev, 0x91, 0, 0, 0, NULL);

	reg8 = AX_RXCOE_IP | AX_RXCOE_TCP | AX_RXCOE_UDP |
	       AX_RXCOE_TCPV6 | AX_RXCOE_UDPV6;
	ax_write_cmd(axdev, AX_ACCESS_MAC, AX_RXCOE_CTL, 1, 1, &reg8);

	reg8 = AX_TXCOE_IP | AX_TXCOE_TCP | AX_TXCOE_UDP |
	       AX_TXCOE_TCPV6 | AX_TXCOE_UDPV6;
	ax_write_cmd(axdev, AX_ACCESS_MAC, AX_TXCOE_CTL, 1, 1, &reg8);

	reg8 = AX_MONITOR_MODE_PMETYPE | AX_MONITOR_MODE_PMEPOL |
	       AX_MONITOR_MODE_RWLC | AX_MONITOR_MODE_RWMP;
	ax_write_cmd(axdev, AX_ACCESS_MAC, AX_MONITOR_MODE, 1, 1, &reg8);

	ax88179_LED_setting(axdev);

	ax88179_EEE_setting(axdev);

	ax88179_Gether_setting(axdev);

	ax_set_tx_qlen(axdev);

	mii_nway_restart(&axdev->mii);

	return 0;

}

static void ax88179_unbind(struct usbnet *dev, struct usb_interface *intf)
{
	u16 tmp16;

	/* Configure RX control register => stop operation */
	tmp16 = AX_RX_CTL_STOP;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_RX_CTL, 2, 2, &tmp16);

	tmp16 = 0;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_CLK_SELECT, 1, 1, &tmp16);

	/* Power down ethernet PHY */
	tmp16 = 0;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, &tmp16);
}

static void
ax88179_rx_checksum(struct sk_buff *skb, u32 *pkt_hdr)
{
	skb->ip_summed = CHECKSUM_NONE;

	/* checksum error bit is set */
	if ((*pkt_hdr & AX_RXHDR_L3CSUM_ERR) ||
	    (*pkt_hdr & AX_RXHDR_L4CSUM_ERR))
		return;

	/* It must be a TCP or UDP packet with a valid checksum */
	if (((*pkt_hdr & AX_RXHDR_L4_TYPE_MASK) == AX_RXHDR_L4_TYPE_TCP) ||
	    ((*pkt_hdr & AX_RXHDR_L4_TYPE_MASK) == AX_RXHDR_L4_TYPE_UDP))
		skb->ip_summed = CHECKSUM_UNNECESSARY;
}

static int ax88179_rx_fixup(struct usbnet *dev, struct sk_buff *skb)
{
	struct sk_buff *ax_skb;
	int pkt_cnt;
	u32 rx_hdr;
	u16 hdr_off;
	u32 *pkt_hdr;

	/* At the end of the SKB, there's a header telling us how many packets
	 * are bundled into this buffer and where we can find an array of
	 * per-packet metadata (which contains elements encoded into u16).
	 */

	/* SKB contents for current firmware:
	 *   <packet 1> <padding>
	 *   ...
	 *   <packet N> <padding>
	 *   <per-packet metadata entry 1> <dummy header>
	 *   ...
	 *   <per-packet metadata entry N> <dummy header>
	 *   <padding2> <rx_hdr>
	 *
	 * where:
	 *   <packet N> contains pkt_len bytes:
	 *		2 bytes of IP alignment pseudo header
	 *		packet received
	 *   <per-packet metadata entry N> contains 4 bytes:
	 *		pkt_len and fields AX_RXHDR_*
	 *   <padding>	0-7 bytes to terminate at
	 *		8 bytes boundary (64-bit).
	 *   <padding2> 4 bytes to make rx_hdr terminate at
	 *		8 bytes boundary (64-bit)
	 *   <dummy-header> contains 4 bytes:
	 *		pkt_len=0 and AX_RXHDR_DROP_ERR
	 *   <rx-hdr>	contains 4 bytes:
	 *		pkt_cnt and hdr_off (offset of
	 *		  <per-packet metadata entry 1>)
	 *
	 * pkt_cnt is number of entrys in the per-packet metadata.
	 * In current firmware there is 2 entrys per packet.
	 * The first points to the packet and the
	 *  second is a dummy header.
	 * This was done probably to align fields in 64-bit and
	 *  maintain compatibility with old firmware.
	 * This code assumes that <dummy header> and <padding2> are
	 *  optional.
	 */

	if (skb->len < 4)
		return 0;
	skb_trim(skb, skb->len - 4);
	memcpy(&rx_hdr, skb_tail_pointer(skb), 4);
	le32_to_cpus(&rx_hdr);
	pkt_cnt = (u16)rx_hdr;
	hdr_off = (u16)(rx_hdr >> 16);

	if (pkt_cnt == 0)
		return 0;

	/* Make sure that the bounds of the metadata array are inside the SKB
	 * (and in front of the counter at the end).
	 */
	if (pkt_cnt * 4 + hdr_off > skb->len)
		return 0;
	pkt_hdr = (u32 *)(skb->data + hdr_off);

	/* Packets must not overlap the metadata array */
	skb_trim(skb, hdr_off);

	for (; pkt_cnt > 0; pkt_cnt--, pkt_hdr++) {
		u16 pkt_len_plus_padd;
		u16 pkt_len;

		le32_to_cpus(pkt_hdr);
		pkt_len = (*pkt_hdr >> 16) & 0x1fff;
		pkt_len_plus_padd = (pkt_len + 7) & 0xfff8;

		/* Skip dummy header used for alignment
		 */
		if (pkt_len == 0)
			continue;

		if (pkt_len_plus_padd > skb->len)
			return 0;

		/* Check CRC or runt packet */
		if ((*pkt_hdr & (AX_RXHDR_CRC_ERR | AX_RXHDR_DROP_ERR)) ||
		    pkt_len < 2 + ETH_HLEN) {
			dev->net->stats.rx_errors++;
			skb_pull(skb, pkt_len_plus_padd);
			continue;
		}

		/* last packet */
		if (pkt_len_plus_padd == skb->len) {
			skb_trim(skb, pkt_len);

			/* Skip IP alignment pseudo header */
			skb_pull(skb, 2);

			ax88179_rx_checksum(skb, pkt_hdr);
			return 1;
		}

		ax_skb = netdev_alloc_skb_ip_align(dev->net, pkt_len);
		if (!ax_skb)
			return 0;
		skb_put(ax_skb, pkt_len);
		memcpy(ax_skb->data, skb->data + 2, pkt_len);

		ax88179_rx_checksum(ax_skb, pkt_hdr);
		usbnet_skb_return(dev, ax_skb);

		skb_pull(skb, pkt_len_plus_padd);
	}

	return 0;
}

static struct sk_buff *
ax88179_tx_fixup(struct usbnet *dev, struct sk_buff *skb, gfp_t flags)
{
	u8 reg8[5], link_sts;
	u16 mode, reg16, delay;
	u32 reg32;

	mode = AX_MEDIUM_TXFLOW_CTRLEN | AX_MEDIUM_RXFLOW_CTRLEN;

	ax_read_cmd_nopm(axdev, AX_ACCESS_MAC, PHYSICAL_LINK_STATUS,
			 1, 1, &link_sts, 0);
	ax_read_cmd_nopm(axdev, AX_ACCESS_PHY, AX88179_PHY_ID,
			 GMII_PHY_PHYSR, 2, &reg16, 1);

	if (!(tmp16 & GMII_PHY_PHYSR_LINK)) {
		netdev_info(dev->net, "ax88179 - Link status is: 0\n");
		return 0;
	} else if (GMII_PHY_PHYSR_GIGA == (tmp16 & GMII_PHY_PHYSR_SMASK)) {
		mode |= AX_MEDIUM_GIGAMODE | AX_MEDIUM_EN_125MHZ;
		if (dev->net->mtu > 1500)
			mode |= AX_MEDIUM_JUMBO_EN;

		if (link_sts & AX_USB_SS)
			memcpy(reg8, &AX88179_BULKIN_SIZE[0], 5);
		else if (link_sts & AX_USB_HS)
			memcpy(reg8, &AX88179_BULKIN_SIZE[1], 5);
		else
			memcpy(reg8, &AX88179_BULKIN_SIZE[3], 5);
	} else if (GMII_PHY_PHYSR_100 == (reg16 & GMII_PHY_PHYSR_SMASK)) {
		mode |= AX_MEDIUM_PS;
		if (link_sts & (AX_USB_SS | AX_USB_HS))
			memcpy(reg8, &AX88179_BULKIN_SIZE[2], 5);
		else
			memcpy(reg8, &AX88179_BULKIN_SIZE[3], 5);
	} else {
		memcpy(reg8, &AX88179_BULKIN_SIZE[3], 5);
	}

	/* RX bulk configuration */
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_RX_BULKIN_QCTRL, 5, 5, tmp);

	dev->rx_urb_size = (1024 * (tmp[3] + 2));

	if (tmp16 & GMII_PHY_PHYSR_FULL)
		mode |= AX_MEDIUM_FULL_DUPLEX;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
			  2, 2, &mode);

	ax179_data->eee_enabled = ax88179_chk_eee(dev);

	netif_carrier_on(dev->net);

	netdev_info(dev->net, "ax88179 - Link status is: 1\n");

	return 0;
}

static int ax88179_reset(struct usbnet *dev)
{
	u8 buf[5];
	u16 *tmp16;
	u8 *tmp;
	struct ax88179_data *ax179_data = (struct ax88179_data *)dev->data;
	struct ethtool_eee eee_data;

	tmp16 = (u16 *)buf;
	tmp = (u8 *)buf;

	/* Power up ethernet PHY */
	*tmp16 = 0;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, tmp16);

	*tmp16 = AX_PHYPWR_RSTCTL_IPRL;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, tmp16);
	msleep(500);

	*tmp = AX_CLK_SELECT_ACS | AX_CLK_SELECT_BCS;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_CLK_SELECT, 1, 1, tmp);
	msleep(200);

	/* Ethernet PHY Auto Detach*/
	ax88179_auto_detach(dev, 0);

	ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_NODE_ID, ETH_ALEN, ETH_ALEN,
			 dev->net->dev_addr);

	/* RX bulk configuration */
	memcpy(tmp, &AX88179_BULKIN_SIZE[0], 5);
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_RX_BULKIN_QCTRL, 5, 5, tmp);

	dev->rx_urb_size = 1024 * 20;

	*tmp = 0x34;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_PAUSE_WATERLVL_LOW, 1, 1, tmp);

	*tmp = 0x52;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_PAUSE_WATERLVL_HIGH,
			  1, 1, tmp);

	dev->net->features |= NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM |
			      NETIF_F_RXCSUM;

	dev->net->hw_features |= NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM |
				 NETIF_F_RXCSUM;

	/* Enable checksum offload */
	*tmp = AX_RXCOE_IP | AX_RXCOE_TCP | AX_RXCOE_UDP |
	       AX_RXCOE_TCPV6 | AX_RXCOE_UDPV6;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_RXCOE_CTL, 1, 1, tmp);

	*tmp = AX_TXCOE_IP | AX_TXCOE_TCP | AX_TXCOE_UDP |
	       AX_TXCOE_TCPV6 | AX_TXCOE_UDPV6;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_TXCOE_CTL, 1, 1, tmp);

	/* Configure RX control register => start operation */
	*tmp16 = AX_RX_CTL_DROPCRCERR | AX_RX_CTL_IPE | AX_RX_CTL_START |
		 AX_RX_CTL_AP | AX_RX_CTL_AMALL | AX_RX_CTL_AB;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_RX_CTL, 2, 2, tmp16);

	*tmp = AX_MONITOR_MODE_PMETYPE | AX_MONITOR_MODE_PMEPOL |
	       AX_MONITOR_MODE_RWMP;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_MONITOR_MOD, 1, 1, tmp);

	/* Configure default medium type => giga */
	*tmp16 = AX_MEDIUM_RECEIVE_EN | AX_MEDIUM_TXFLOW_CTRLEN |
		 AX_MEDIUM_RXFLOW_CTRLEN | AX_MEDIUM_FULL_DUPLEX |
		 AX_MEDIUM_GIGAMODE;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
			  2, 2, tmp16);

	ax88179_led_setting(dev);

	ax179_data->eee_enabled = 0;
	ax179_data->eee_active = 0;

	ax88179_disable_eee(dev);

	ax88179_ethtool_get_eee(dev, &eee_data);
	eee_data.advertised = 0;
	ax88179_ethtool_set_eee(dev, &eee_data);

	/* Restart autoneg */
	mii_nway_restart(&dev->mii);

	usbnet_link_change(dev, 0, 0);

	return 0;
}

static int ax88179_system_resume(struct ax_device *axdev)
{
	u16 reg16;
	u8 reg8;

	reg16 = 0;
	ax_write_cmd_nopm(axdev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, &reg16);
#if KERNEL_VERSION(2, 6, 36) <= LINUX_VERSION_CODE
	usleep_range(1000, 2000);
#else
	msleep(20);
#endif
	reg16 = AX_PHYPWR_RSTCTL_IPRL;
	ax_write_cmd_nopm(axdev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, &reg16);
	msleep(200);

	ax88179_AutoDetach(axdev, 1);

	ax_read_cmd_nopm(axdev, AX_ACCESS_MAC,  AX_CLK_SELECT, 1, 1, &reg8, 0);
	reg8 |= AX_CLK_SELECT_ACS | AX_CLK_SELECT_BCS;
	ax_write_cmd_nopm(axdev, AX_ACCESS_MAC, AX_CLK_SELECT, 1, 1, &reg8);
	msleep(100);

	reg16 = AX_RX_CTL_START | AX_RX_CTL_AP |
		AX_RX_CTL_AMALL | AX_RX_CTL_AB;
	ax_write_cmd_nopm(axdev, AX_ACCESS_MAC, AX_RX_CTL, 2, 2, &reg16);

	return 0;
}

const struct driver_info ax88179_info = {
	.bind = ax88179_bind,
	.unbind = ax88179_unbind,
	.hw_init = ax88179_hw_init,
	.stop = ax88179_stop,
	.link_reset = ax88179_link_reset,
	.rx_fixup = ax88179_rx_fixup,
	.tx_fixup = ax88179_tx_fixup,
	.system_suspend = ax88179_system_suspend,
	.system_resume = ax88179_system_resume,
	.napi_weight = AX88179_NAPI_WEIGHT,
	.buf_rx_size = AX88179_BUF_RX_SIZE,
};
