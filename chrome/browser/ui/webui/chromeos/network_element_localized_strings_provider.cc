// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/network_element_localized_strings_provider.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/net/shill_error.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace network_element {

namespace {

constexpr webui::LocalizedString kElementLocalizedStrings[] = {
    {"OncType", IDS_NETWORK_TYPE},
    {"OncTypeCellular", IDS_NETWORK_TYPE_CELLULAR},
    {"OncTypeEthernet", IDS_NETWORK_TYPE_ETHERNET},
    {"OncTypeMobile", IDS_NETWORK_TYPE_MOBILE_DATA},
    {"OncTypeTether", IDS_NETWORK_TYPE_TETHER},
    {"OncTypeVPN", IDS_NETWORK_TYPE_VPN},
    {"OncTypeWireless", IDS_NETWORK_TYPE_WIRELESS},
    {"OncTypeWiFi", IDS_NETWORK_TYPE_WIFI},
    {"ipAddressNotAvailable", IDS_NETWORK_IP_ADDRESS_NA},
    {"networkListItemConnected", IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED},
    {"networkListItemConnecting", IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING},
    {"networkListItemConnectingTo", IDS_NETWORK_LIST_CONNECTING_TO},
    {"networkListItemInitializing", IDS_NETWORK_LIST_INITIALIZING},
    {"networkListItemSubpageButtonLabel",
     IDS_NETWORK_LIST_ITEM_SUBPAGE_BUTTON_LABEL},
    {"networkListItemLabel", IDS_NETWORK_LIST_ITEM_LABEL},
    {"networkListItemLabelCellular", IDS_NETWORK_LIST_ITEM_LABEL_CELLULAR},
    {"networkListItemLabelCellularWithProviderName",
     IDS_NETWORK_LIST_ITEM_LABEL_CELLULAR_WITH_PROVIDER_NAME},
    {"networkListItemLabelCellularManaged",
     IDS_NETWORK_LIST_ITEM_LABEL_CELLULAR_MANAGED},
    {"networkListItemLabelCellularManagedWithProviderName",
     IDS_NETWORK_LIST_ITEM_LABEL_CELLULAR_MANAGED_WITH_PROVIDER_NAME},
    {"networkListItemLabelEthernetManaged",
     IDS_NETWORK_LIST_ITEM_LABEL_ETHERNET_MANAGED},
    {"networkListItemLabelTether", IDS_NETWORK_LIST_ITEM_LABEL_TETHER},
    {"networkListItemLabelTetherWithProviderName",
     IDS_NETWORK_LIST_ITEM_LABEL_TETHER_WITH_PROVIDER_NAME},
    {"networkListItemLabelWifi", IDS_NETWORK_LIST_ITEM_LABEL_WIFI},
    {"networkListItemLabelWifiManaged",
     IDS_NETWORK_LIST_ITEM_LABEL_WIFI_MANAGED},
    {"networkListItemLabelEthernetWithConnectionStatus",
     IDS_NETWORK_LIST_ITEM_LABEL_ETHERNET_WITH_CONNECTION_STATUS},
    {"networkListItemLabelEthernetManagedWithConnectionStatus",
     IDS_NETWORK_LIST_ITEM_LABEL_ETHERNET_MANAGED_WITH_CONNECTION_STATUS},
    {"networkListItemLabelCellularWithConnectionStatus",
     IDS_NETWORK_LIST_ITEM_LABEL_CELLULAR_WITH_CONNECTION_STATUS},
    {"networkListItemLabelCellularWithConnectionStatusAndProviderName",
     IDS_NETWORK_LIST_ITEM_LABEL_CELLULAR_WITH_CONNECTION_STATUS_AND_PROVIDER_NAME},
    {"networkListItemLabelCellularManagedWithConnectionStatus",
     IDS_NETWORK_LIST_ITEM_LABEL_CELLULAR_MANAGED_WITH_CONNECTION_STATUS},
    {"networkListItemLabelCellularManagedWithConnectionStatusAndProviderName",
     IDS_NETWORK_LIST_ITEM_LABEL_CELLULAR_MANAGED_WITH_CONNECTION_STATUS_AND_PROVIDER_NAME},
    {"networkListItemLabelWifiWithConnectionStatus",
     IDS_NETWORK_LIST_ITEM_LABEL_WIFI_WITH_CONNECTION_STATUS},
    {"networkListItemLabelWifiManagedWithConnectionStatus",
     IDS_NETWORK_LIST_ITEM_LABEL_WIFI_MANAGED_WITH_CONNECTION_STATUS},
    {"networkListItemLabelTetherWithConnectionStatus",
     IDS_NETWORK_LIST_ITEM_LABEL_TETHER_WITH_CONNECTION_STATUS},
    {"networkListItemLabelTetherWithConnectionStatusAndProviderName",
     IDS_NETWORK_LIST_ITEM_LABEL_TETHER_WITH_CONNECTION_STATUS_AND_PROVIDER_NAME},
    {"networkListItemLabelESimPendingProfile",
     IDS_NETWORK_LIST_ITEM_LABEL_ESIM_PENDING_PROFILE},
    {"networkListItemLabelESimPendingProfileWithProviderName",
     IDS_NETWORK_LIST_ITEM_LABEL_ESIM_PENDING_PROFILE_WITH_PROVIDER_NAME},
    {"networkListItemLabelESimPendingProfileInstalling",
     IDS_NETWORK_LIST_ITEM_LABEL_ESIM_PENDING_PROFILE_INSTALLING},
    {"networkListItemLabelESimPendingProfileWithProviderNameInstalling",
     IDS_NETWORK_LIST_ITEM_LABEL_ESIM_PENDING_PROFILE_WITH_PROVIDER_NAME_INSTALLING},
    {"wifiNetworkStatusSecured", IDS_WIFI_NETWORK_STATUS_SECURED},
    {"wifiNetworkStatusUnsecured", IDS_WIFI_NETWORK_STATUS_UNSECURED},
    {"networkListItemNotAvailable", IDS_NETWORK_LIST_NOT_AVAILABLE},
    {"networkListItemScanning", IDS_SETTINGS_INTERNET_MOBILE_SEARCH},
    {"networkListItemSimCardLocked", IDS_NETWORK_LIST_SIM_CARD_LOCKED},
    {"networkListItemUpdatedCellularSimCardLocked",
     IDS_NETWORK_LIST_UPDATED_CELLULAR_SIM_CARD_LOCKED},
    {"networkListItemNotConnected", IDS_NETWORK_LIST_NOT_CONNECTED},
    {"networkListItemNoNetwork", IDS_NETWORK_LIST_NO_NETWORK},
    {"networkListItemActivate", IDS_NETWORK_LIST_ITEM_ACTIVATE},
    {"networkListItemActivateA11yLabel",
     IDS_NETWORK_LIST_ITEM_ACTIVATE_A11Y_LABEL},
    {"networkListItemActivating", IDS_NETWORK_LIST_ITEM_ACTIVATING},
    {"networkListItemUnavailableSimNetwork",
     IDS_NETWORK_LIST_ITEM_UNAVAILABLE_SIM_NETWORK},
    {"networkListItemDownload", IDS_NETWORK_LIST_ITEM_DOWNLOAD},
    {"networkListItemDownloadA11yLabel",
     IDS_NETWORK_LIST_ITEM_DOWNLOAD_A11Y_LABEL},
    {"networkListItemUnlock", IDS_NETWORK_LIST_ITEM_UNLOCK},
    {"networkListItemUnlockA11YLabel", IDS_NETWORK_LIST_ITEM_UNLOCK_A11Y_LABEL},
    {"networkListItemAddingProfile", IDS_NETWORK_LIST_ITEM_ADDING_PROFILE},
    {"vpnNameTemplate", IDS_NETWORK_LIST_THIRD_PARTY_VPN_NAME_TEMPLATE},
    {"networkIconLabelEthernet", IDS_NETWORK_ICON_LABEL_ETHERNET},
    {"networkIconLabelVpn", IDS_NETWORK_ICON_LABEL_VPN},
    {"networkIconLabelOff", IDS_NETWORK_ICON_LABEL_NETWORK_OFF},
    {"networkIconLabelNoNetwork", IDS_NETWORK_ICON_LABEL_NO_NETWORK},
    {"networkIconLabelConnecting", IDS_NETWORK_ICON_LABEL_CONNECTING},
    {"networkIconLabelNotConnected", IDS_NETWORK_ICON_LABEL_NOT_CONNECTED},
    {"networkIconLabelSignalStrength", IDS_NETWORK_ICON_LABEL_SIGNAL_STRENGTH},
};

}  //  namespace

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedStrings(kElementLocalizedStrings);
}

void AddLocalizedValuesToBuilder(::login::LocalizedValuesBuilder* builder) {
  for (const auto& entry : kElementLocalizedStrings)
    builder->Add(entry.name, entry.id);
}

void AddOncLocalizedStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      // Thes strings are generated by prepending 'Onc' to the ONC property
      // name. Any '.' in the property name is replaced with '-'. Properties
      // with translatable enumerated values have the value appended after '_'.
      {"OncCellular-APN-AccessPointName",
       IDS_ONC_CELLULAR_APN_ACCESS_POINT_NAME},
      {"OncCellular-APN-AccessPointName_none",
       IDS_ONC_CELLULAR_APN_ACCESS_POINT_NAME_NONE},
      {"OncCellular-APN-Authentication", IDS_ONC_CELLULAR_APN_AUTHENTICATION},
      {"OncCellular-APN-Password", IDS_ONC_CELLULAR_APN_PASSWORD},
      {"OncCellular-APN-Username", IDS_ONC_CELLULAR_APN_USERNAME},
      {"OncCellular-APN-Attach", IDS_ONC_CELLULAR_APN_ATTACH},
      {"OncCellular-APN-Attach_TooltipText",
       IDS_ONC_CELLULAR_APN_ATTACH_TOOLTIP_TEXT},
      {"OncCellular-ActivationState", IDS_ONC_CELLULAR_ACTIVATION_STATE},
      {"OncCellular-ActivationState_Activated",
       IDS_ONC_CELLULAR_ACTIVATION_STATE_ACTIVATED},
      {"OncCellular-ActivationState_Activating",
       IDS_ONC_CELLULAR_ACTIVATION_STATE_ACTIVATING},
      {"OncCellular-ActivationState_NotActivated",
       IDS_ONC_CELLULAR_ACTIVATION_STATE_NOT_ACTIVATED},
      {"OncCellular-ActivationState_PartiallyActivated",
       IDS_ONC_CELLULAR_ACTIVATION_STATE_PARTIALLY_ACTIVATED},
      {"OncCellular-ActivationState_NoService",
       IDS_ONC_CELLULAR_ACTIVATION_STATE_NO_SERVICE},
      {"OncCellular-Family", IDS_ONC_CELLULAR_FAMILY},
      {"OncCellular-FirmwareRevision", IDS_ONC_CELLULAR_FIRMWARE_REVISION},
      {"OncCellular-HardwareRevision", IDS_ONC_CELLULAR_HARDWARE_REVISION},
      {"OncCellular-HomeProvider-Code", IDS_ONC_CELLULAR_HOME_PROVIDER_CODE},
      {"OncCellular-HomeProvider-Country",
       IDS_ONC_CELLULAR_HOME_PROVIDER_COUNTRY},
      {"OncCellular-HomeProvider-Name", IDS_ONC_CELLULAR_HOME_PROVIDER_NAME},
      {"OncCellular-Manufacturer", IDS_ONC_CELLULAR_MANUFACTURER},
      {"OncCellular-ModelID", IDS_ONC_CELLULAR_MODEL_ID},
      {"OncCellular-NetworkTechnology", IDS_ONC_CELLULAR_NETWORK_TECHNOLOGY},
      {"OncCellular-RoamingState", IDS_ONC_CELLULAR_ROAMING_STATE},
      {"OncCellular-RoamingState_Home", IDS_ONC_CELLULAR_ROAMING_STATE_HOME},
      {"OncCellular-RoamingState_Roaming",
       IDS_ONC_CELLULAR_ROAMING_STATE_ROAMING},
      {"OncCellular-ServingOperator-Code",
       IDS_ONC_CELLULAR_SERVING_OPERATOR_CODE},
      {"OncCellular-ServingOperator-Name",
       IDS_ONC_CELLULAR_SERVING_OPERATOR_NAME},
      {"OncConnected", IDS_ONC_CONNECTED},
      {"OncConnecting", IDS_ONC_CONNECTING},
      {"OncEAP-AnonymousIdentity", IDS_ONC_EAP_ANONYMOUS_IDENTITY},
      {"OncEAP-Identity", IDS_ONC_EAP_IDENTITY},
      {"OncEAP-Inner", IDS_ONC_EAP_INNER},
      {"OncEAP-Inner_Automatic", IDS_ONC_EAP_INNER_AUTOMATIC},
      {"OncEAP-Inner_CHAP", IDS_ONC_EAP_INNER_CHAP},
      {"OncEAP-Inner_GTC", IDS_ONC_EAP_INNER_GTC},
      {"OncEAP-Inner_MD5", IDS_ONC_EAP_INNER_MD5},
      {"OncEAP-Inner_MSCHAP", IDS_ONC_EAP_INNER_MSCHAP},
      {"OncEAP-Inner_MSCHAPv2", IDS_ONC_EAP_INNER_MSCHAPV2},
      {"OncEAP-Inner_PAP", IDS_ONC_EAP_INNER_PAP},
      {"OncEAP-Outer", IDS_ONC_EAP_OUTER},
      {"OncEAP-Outer_LEAP", IDS_ONC_EAP_OUTER_LEAP},
      {"OncEAP-Outer_PEAP", IDS_ONC_EAP_OUTER_PEAP},
      {"OncEAP-Outer_EAP-TLS", IDS_ONC_EAP_OUTER_TLS},
      {"OncEAP-Outer_EAP-TTLS", IDS_ONC_EAP_OUTER_TTLS},
      {"OncEAP-Password", IDS_ONC_WIFI_PASSWORD},
      {"OncEAP-ServerCA", IDS_ONC_EAP_SERVER_CA},
      {"OncEAP-SubjectMatch", IDS_ONC_EAP_SUBJECT_MATCH},
      {"OncEAP-UserCert", IDS_ONC_EAP_USER_CERT},
      {"OncMacAddress", IDS_ONC_MAC_ADDRESS},
      {"OncName", IDS_ONC_NAME},
      {"OncNotConnected", IDS_ONC_NOT_CONNECTED},
      {"OncPortalState", IDS_ONC_PORTAL_STATE},
      {"OncPortalState_NoInternet", IDS_ONC_PORTAL_STATE_NO_INTERNET},
      {"OncPortalState_Portal", IDS_ONC_PORTAL_STATE_PORTAL},
      {"OncPortalState_PortalSuspected", IDS_ONC_PORTAL_STATE_PORTAL_SUSPECTED},
      {"OncPortalState_ProxyAuthRequired", IDS_ONC_PORTAL_STATE_PROXY_AUTH},
      {"OncTether-BatteryPercentage", IDS_ONC_TETHER_BATTERY_PERCENTAGE},
      {"OncTether-BatteryPercentage_Value",
       IDS_ONC_TETHER_BATTERY_PERCENTAGE_VALUE},
      {"OncTether-SignalStrength", IDS_ONC_TETHER_SIGNAL_STRENGTH},
      {"OncTether-SignalStrength_Weak", IDS_ONC_TETHER_SIGNAL_STRENGTH_WEAK},
      {"OncTether-SignalStrength_Okay", IDS_ONC_TETHER_SIGNAL_STRENGTH_OKAY},
      {"OncTether-SignalStrength_Good", IDS_ONC_TETHER_SIGNAL_STRENGTH_GOOD},
      {"OncTether-SignalStrength_Strong",
       IDS_ONC_TETHER_SIGNAL_STRENGTH_STRONG},
      {"OncTether-SignalStrength_VeryStrong",
       IDS_ONC_TETHER_SIGNAL_STRENGTH_VERY_STRONG},
      {"OncTether-Carrier", IDS_ONC_TETHER_CARRIER},
      {"OncTether-Carrier_Unknown", IDS_ONC_TETHER_CARRIER_UNKNOWN},
      {"OncVPN-Host", IDS_ONC_VPN_HOST},
      {"OncVPN-IPsec-Group", IDS_ONC_VPN_IPSEC_GROUP},
      {"OncVPN-IPsec-PSK", IDS_ONC_VPN_IPSEC_PSK},
      {"OncVPN-L2TP-Password", IDS_ONC_VPN_PASSWORD},
      {"OncVPN-L2TP-Username", IDS_ONC_VPN_USERNAME},
      {"OncVPN-OpenVPN-ExtraHosts", IDS_ONC_VPN_OPENVPN_EXTRA_HOSTS},
      {"OncVPN-OpenVPN-OTP", IDS_ONC_VPN_OPENVPN_OTP},
      {"OncVPN-OpenVPN-Password", IDS_ONC_VPN_PASSWORD},
      {"OncVPN-OpenVPN-Username", IDS_ONC_VPN_USERNAME},
      {"OncVPN-ProviderName", IDS_ONC_VPN_THIRD_PARTY_VPN_PROVIDER_NAME},
      {"OncVPN-Type", IDS_ONC_VPN_TYPE},
      {"OncVPN-Type_L2TP_IPsec", IDS_ONC_VPN_TYPE_L2TP_IPSEC},
      {"OncVPN-Type_L2TP_IPsec_PSK", IDS_ONC_VPN_TYPE_L2TP_IPSEC_PSK},
      {"OncVPN-Type_L2TP_IPsec_Cert", IDS_ONC_VPN_TYPE_L2TP_IPSEC_CERT},
      {"OncVPN-Type_OpenVPN", IDS_ONC_VPN_TYPE_OPENVPN},
      {"OncVPN-Type_ARCVPN", IDS_ONC_VPN_TYPE_ARCVPN},
      {"OncWiFi-Frequency", IDS_ONC_WIFI_FREQUENCY},
      {"OncWiFi-Passphrase", IDS_ONC_WIFI_PASSWORD},
      {"OncWiFi-SSID", IDS_ONC_WIFI_SSID},
      {"OncWiFi-BSSID", IDS_ONC_WIFI_BSSID},
      {"OncWiFi-Security", IDS_ONC_WIFI_SECURITY},
      {"OncWiFi-Security_None", IDS_ONC_WIFI_SECURITY_NONE},
      {"OncWiFi-Security_WEP-PSK", IDS_ONC_WIFI_SECURITY_WEP},
      {"OncWiFi-Security_WPA-EAP", IDS_ONC_WIFI_SECURITY_EAP},
      {"OncWiFi-Security_WPA-PSK", IDS_ONC_WIFI_SECURITY_PSK},
      {"OncWiFi-Security_WEP-8021X", IDS_ONC_WIFI_SECURITY_EAP},
      {"OncWiFi-SignalStrength", IDS_ONC_WIFI_SIGNAL_STRENGTH},
      {"Oncipv4-Gateway", IDS_ONC_IPV4_GATEWAY},
      {"Oncipv4-IPAddress", IDS_ONC_IPV4_ADDRESS},
      // We use 'Netmask' in the UI instead of RoutingPrefix.
      {"Oncipv4-Netmask", IDS_ONC_IPV4_NETMASK},
      {"Oncipv6-IPAddress", IDS_ONC_IPV6_ADDRESS},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddDetailsLocalizedStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"hidePassword", IDS_SETTINGS_PASSWORD_HIDE},
      {"showPassword", IDS_SETTINGS_PASSWORD_SHOW},
      {"networkProxy", IDS_SETTINGS_INTERNET_NETWORK_PROXY_PROXY},
      {"networkProxyAddException",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_ADD_EXCEPTION},
      {"networkProxyAllowShared",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_ALLOW_SHARED},
      {"networkProxyAllowSharedEnableWarningTitle",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_ALLOW_SHARED_ENABLE_WARNING_TITLE},
      {"networkProxyAllowSharedDisableWarningTitle",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_ALLOW_SHARED_DISABLE_WARNING_TITLE},
      {"networkProxyAllowSharedWarningMessage",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_ALLOW_SHARED_WARNING_MESSAGE},
      {"networkProxyAutoConfig",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_AUTO_CONFIG},
      {"networkProxyConnectionType",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_CONNECTION_TYPE},
      {"networkProxyEnforcedPolicy",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_ENFORCED_POLICY},
      {"networkProxyExceptionInputA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_EXCEPTION_INPUT_ACCESSIBILITY_LABEL},
      {"networkProxyExceptionList",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_EXCEPTION_LIST},
      {"networkProxyExceptionRemoveA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_EXCEPT_REMOVE_ACCESSIBILITY_LABEL},
      {"networkProxyFtp", IDS_SETTINGS_INTERNET_NETWORK_PROXY_FTP_PROXY},
      {"networkProxyHostInputA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_HOST_INPUT_ACCESSIBILITY_LABEL},
      {"networkProxyHttp", IDS_SETTINGS_INTERNET_NETWORK_PROXY_HTTP_PROXY},
      {"networkProxyPort", IDS_SETTINGS_INTERNET_NETWORK_PROXY_PORT},
      {"networkProxyPortInputA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_PORT_INPUT_ACCESSIBILITY_LABEL},
      {"networkProxyShttp", IDS_SETTINGS_INTERNET_NETWORK_PROXY_SHTTP_PROXY},
      {"networkProxySocks", IDS_SETTINGS_INTERNET_NETWORK_PROXY_SOCKS_HOST},
      {"networkProxyTypeDirect",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_TYPE_DIRECT},
      {"networkProxyTypeManual",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_TYPE_MANUAL},
      {"networkProxyTypePac", IDS_SETTINGS_INTERNET_NETWORK_PROXY_TYPE_PAC},
      {"networkProxyTypeWpad", IDS_SETTINGS_INTERNET_NETWORK_PROXY_TYPE_WPAD},
      {"networkProxyUseSame", IDS_SETTINGS_INTERNET_NETWORK_PROXY_USE_SAME},
      {"networkSimCardLocked", IDS_SETTINGS_INTERNET_NETWORK_SIM_CARD_LOCKED},
      {"networkSimCardMissing", IDS_SETTINGS_INTERNET_NETWORK_SIM_CARD_MISSING},
      {"networkSimChange", IDS_SETTINGS_INTERNET_NETWORK_SIM_BUTTON_CHANGE},
      {"networkSimChangePin", IDS_SETTINGS_INTERNET_NETWORK_SIM_CHANGE_PIN},
      {"networkSimChangePinTitle",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_CHANGE_PIN_TITLE},
      {"networkSimLockedTooltip",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_LOCKED_TOOLTIP},
      {"networkSimEnter", IDS_SETTINGS_INTERNET_NETWORK_SIM_BUTTON_ENTER},
      {"networkSimLockedSubtitle",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_LOCKED_SUBTITLE},
      {"networkSimEnterNewPin",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_ENTER_NEW_PIN},
      {"networkSimEnterOldPin",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_ENTER_OLD_PIN},
      {"networkSimEnterPin", IDS_SETTINGS_INTERNET_NETWORK_SIM_ENTER_PIN},
      {"networkSimEnterPinTitle",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_ENTER_PIN_TITLE},
      {"networkSimEnterPuk", IDS_SETTINGS_INTERNET_NETWORK_SIM_ENTER_PUK},
      {"networkSimLockEnable", IDS_SETTINGS_INTERNET_NETWORK_SIM_LOCK_ENABLE},
      {"networkSimLockedTitle", IDS_SETTINGS_INTERNET_NETWORK_SIM_LOCKED_TITLE},
      {"networkSimPukDialogSubtitle",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_LOCKED_PUK_SUBTITLE},
      {"networkSimLockedWarning",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_LOCKED_WARNING},
      {"networkSimReEnterNewPin",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_RE_ENTER_NEW_PIN},
      {"networkSimReEnterNewPin",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_RE_ENTER_NEW_PIN},
      {"networkSimErrorIncorrectPin",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_ERROR_INCORRECT_PIN},
      {"networkSimErrorIncorrectPinPlural",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_ERROR_INCORRECT_PIN_PLURAL},
      {"networkSimErrorIncorrectPuk",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_ERROR_INCORRECT_PUK},
      {"networkSimErrorIncorrectPukPlural",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_ERROR_INCORRECT_PUK_PLURAL},
      {"networkSimErrorInvalidPin",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_ERROR_INVALID_PIN},
      {"networkSimErrorInvalidPinPlural",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_ERROR_INVALID_PIN_PLURAL},
      {"networkSimErrorInvalidPuk",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_ERROR_INVALID_PUK},
      {"networkSimErrorInvalidPukPlural",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_ERROR_INVALID_PUK_PLURAL},
      {"networkSimErrorPinMismatch",
       IDS_SETTINGS_INTERNET_NETWORK_SIM_ERROR_PIN_MISMATCH},
      {"networkSimUnlock", IDS_SETTINGS_INTERNET_NETWORK_SIM_BUTTON_UNLOCK},
      {"networkAccessPoint", IDS_SETTINGS_INTERNET_NETWORK_ACCESS_POINT},
      {"networkChooseMobile", IDS_SETTINGS_INTERNET_NETWORK_CHOOSE_MOBILE},
      {"networkCellularScan", IDS_SETTINGS_INTERNET_NETWORK_CELLULAR_SCAN},
      {"networkCellularScanCompleted",
       IDS_SETTINGS_INTERNET_NETWORK_CELLULAR_SCAN_COMPLETED},
      {"networkCellularScanConnectedHelp",
       IDS_SETTINGS_INTERNET_NETWORK_CELLULAR_SCAN_CONNECTED_HELP},
      {"networkCellularScanning",
       IDS_SETTINGS_INTERNET_NETWORK_CELLULAR_SCANNING},
      {"networkCellularNoNetworks",
       IDS_SETTINGS_INTERNET_NETWORK_CELLULAR_NO_NETWORKS},
      {"networkNameservers", IDS_SETTINGS_INTERNET_NETWORK_NAMESERVERS},
      {"networkNameserversAutomatic",
       IDS_SETTINGS_INTERNET_NETWORK_NAMESERVERS_AUTOMATIC},
      {"networkNameserversCustom",
       IDS_SETTINGS_INTERNET_NETWORK_NAMESERVERS_CUSTOM},
      {"networkNameserversGoogle",
       IDS_SETTINGS_INTERNET_NETWORK_NAMESERVERS_GOOGLE},
      {"networkNameserversCustomInputA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_NAMESERVERS_INPUT_ACCESSIBILITY_LABEL},
      {"networkProxyWpad", IDS_SETTINGS_INTERNET_NETWORK_PROXY_WPAD},
      {"networkProxyWpadNone", IDS_SETTINGS_INTERNET_NETWORK_PROXY_WPAD_NONE},
      {"remove", IDS_REMOVE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean(
      "useAttachApn",
      base::FeatureList::IsEnabled(chromeos::features::kCellularUseAttachApn));
}

void AddConfigLocalizedStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"networkCAUseDefault", IDS_SETTINGS_INTERNET_NETWORK_CA_USE_DEFAULT},
      {"networkCADoNotCheck", IDS_SETTINGS_INTERNET_NETWORK_CA_DO_NOT_CHECK},
      {"networkNoUserCert", IDS_SETTINGS_INTERNET_NETWORK_NO_USER_CERT},
      {"networkCertificateName",
       IDS_SETTINGS_INTERNET_NETWORK_CERTIFICATE_NAME},
      {"networkCertificateNameHardwareBacked",
       IDS_SETTINGS_INTERNET_NETWORK_CERTIFICATE_NAME_HARDWARE_BACKED},
      {"networkCertificateNoneInstalled",
       IDS_SETTINGS_INTERNET_NETWORK_CERTIFICATE_NONE_INSTALLED},
      {"networkConfigSaveCredentials",
       IDS_SETTINGS_INTERNET_CONFIG_SAVE_CREDENTIALS},
      {"networkConfigShare", IDS_SETTINGS_INTERNET_CONFIG_SHARE},
      {"networkAutoConnect", IDS_SETTINGS_INTERNET_NETWORK_AUTO_CONNECT},
      {"hiddenNetworkWarning", IDS_SETTINGS_HIDDEN_NETWORK_WARNING},
      {"hidePassword", IDS_SETTINGS_PASSWORD_HIDE},
      {"showPassword", IDS_SETTINGS_PASSWORD_SHOW},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean(
      "showHiddenNetworkWarning",
      base::FeatureList::IsEnabled(::features::kHiddenNetworkWarning));

  // Login screen and public account users can only create shared network
  // configurations. Other users default to unshared network configurations.
  // NOTE: Guest and kiosk users can only create unshared network configs.
  // NOTE: Insecure wifi networks are always shared.
  html_source->AddBoolean("shareNetworkDefault",
                          !LoginState::Get()->UserHasNetworkProfile());
  // Only authenticated users can toggle the share state.
  html_source->AddBoolean("shareNetworkAllowEnable",
                          LoginState::Get()->IsUserAuthenticated());
}

void AddErrorLocalizedStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"Error.CannotChangeSharedConfig",
       IDS_NETWORK_ERROR_CANNOT_CHANGE_SHARED_CONFIG},
      {"Error.PolicyControlled", IDS_NETWORK_ERROR_POLICY_CONTROLLED},
      {"networkErrorNoUserCertificate", IDS_NETWORK_ERROR_NO_USER_CERT},
      {NetworkConnectionHandler::kErrorPassphraseRequired,
       IDS_NETWORK_ERROR_PASSPHRASE_REQUIRED},
      {"networkErrorUnknown", IDS_NETWORK_ERROR_UNKNOWN},
      {"networkErrorNotAvailableForNetworkAuth",
       IDS_SETTINGS_INTERNET_NETWORK_NOT_AVAILABLE_FOR_NETWORK_AUTH},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  // Include Shill errors.
  const char* const shill_errors[] = {
      shill::kErrorOutOfRange,
      shill::kErrorPinMissing,
      shill::kErrorDhcpFailed,
      shill::kErrorConnectFailed,
      shill::kErrorBadPassphrase,
      shill::kErrorBadWEPKey,
      shill::kErrorActivationFailed,
      shill::kErrorNeedEvdo,
      shill::kErrorNeedHomeNetwork,
      shill::kErrorOtaspFailed,
      shill::kErrorAaaFailed,
      shill::kErrorInternal,
      shill::kErrorDNSLookupFailed,
      shill::kErrorHTTPGetFailed,
      shill::kErrorIpsecPskAuthFailed,
      shill::kErrorIpsecCertAuthFailed,
      shill::kErrorEapAuthenticationFailed,
      shill::kErrorEapLocalTlsFailed,
      shill::kErrorEapRemoteTlsFailed,
      shill::kErrorPppAuthFailed,
      shill::kErrorResultInvalidPassphrase,
  };
  for (const auto* error : shill_errors) {
    html_source->AddString(
        error, base::UTF16ToUTF8(shill_error::GetShillErrorString(error, "")));
  }
}

}  // namespace network_element

}  // namespace chromeos
