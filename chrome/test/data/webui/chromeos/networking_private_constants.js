// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define chrome.networkingPrivate enums, normally provided by chrome WebUI.
// NOTE: These need to be kept in sync with netwroking_private.idl.

chrome.networkingPrivate = chrome.networkingPrivate || {};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/networkingPrivate#type-ActivationStateType
 */
chrome.networkingPrivate.ActivationStateType = {
  ACTIVATED: 'Activated',
  ACTIVATING: 'Activating',
  NOT_ACTIVATED: 'NotActivated',
  PARTIALLY_ACTIVATED: 'PartiallyActivated',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/networkingPrivate#type-CaptivePortalStatus
 */
chrome.networkingPrivate.CaptivePortalStatus = {
  UNKNOWN: 'Unknown',
  OFFLINE: 'Offline',
  ONLINE: 'Online',
  PORTAL: 'Portal',
  PROXY_AUTH_REQUIRED: 'ProxyAuthRequired',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/networkingPrivate#type-ConnectionStateType
 */
chrome.networkingPrivate.ConnectionStateType = {
  CONNECTED: 'Connected',
  CONNECTING: 'Connecting',
  NOT_CONNECTED: 'NotConnected',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/networkingPrivate#type-DeviceStateType
 */
chrome.networkingPrivate.DeviceStateType = {
  UNINITIALIZED: 'Uninitialized',
  DISABLED: 'Disabled',
  ENABLING: 'Enabling',
  ENABLED: 'Enabled',
  PROHIBITED: 'Prohibited',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/networkingPrivate#type-IPConfigType
 */
chrome.networkingPrivate.IPConfigType = {
  DHCP: 'DHCP',
  STATIC: 'Static',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/networkingPrivate#type-NetworkType
 */
chrome.networkingPrivate.NetworkType = {
  ALL: 'All',
  CELLULAR: 'Cellular',
  ETHERNET: 'Ethernet',
  TETHER: 'Tether',
  VPN: 'VPN',
  WIRELESS: 'Wireless',
  WI_FI: 'WiFi',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/networkingPrivate#type-ProxySettingsType
 */
chrome.networkingPrivate.ProxySettingsType = {
  DIRECT: 'Direct',
  MANUAL: 'Manual',
  PAC: 'PAC',
  WPAD: 'WPAD',
};
