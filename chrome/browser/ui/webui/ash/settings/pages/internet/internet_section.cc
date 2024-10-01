// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/settings/pages/internet/internet_section.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/hotspot_config_service.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/webui/network_ui/network_health_resource_provider.h"
#include "ash/webui/network_ui/traffic_counters_resource_provider.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/webui/ash/cellular_setup/cellular_setup_localized_strings_provider.h"
#include "chrome/browser/ui/webui/ash/settings/pages/internet/internet_handler.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/extension_control_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/chromeos/strings/network/network_element_localized_strings_provider.h"

namespace ash::network_config {
namespace mojom = chromeos::network_config::mojom;
}

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kApnSubpagePath;
using ::chromeos::settings::mojom::kCellularDetailsSubpagePath;
using ::chromeos::settings::mojom::kCellularNetworksSubpagePath;
using ::chromeos::settings::mojom::kEthernetDetailsSubpagePath;
using ::chromeos::settings::mojom::kHotspotSubpagePath;
using ::chromeos::settings::mojom::kKnownNetworksSubpagePath;
using ::chromeos::settings::mojom::kMobileDataNetworksSubpagePath;
using ::chromeos::settings::mojom::kNetworkSectionPath;
using ::chromeos::settings::mojom::kPasspointDetailSubpagePath;
using ::chromeos::settings::mojom::kTetherDetailsSubpagePath;
using ::chromeos::settings::mojom::kVpnDetailsSubpagePath;
using ::chromeos::settings::mojom::kWifiDetailsSubpagePath;
using ::chromeos::settings::mojom::kWifiNetworksSubpagePath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

// These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused. They describe the discovery
// state of a network.
enum class NetworkDiscoveryState {
  kExistingNetwork = 0,
  kNewNetwork = 1,
  kMaxValue = kNewNetwork,
};

const std::vector<SearchConcept>& GetNetworkSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_NETWORK_SETTINGS,
       mojom::kNetworkSectionPath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kNetwork},
       {IDS_OS_SETTINGS_TAG_NETWORK_SETTINGS_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetEthernetConnectedSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ETHERNET_CONFIGURE,
       mojom::kEthernetDetailsSubpagePath,
       mojom::SearchResultIcon::kEthernet,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kConfigureEthernet}},
      {IDS_OS_SETTINGS_TAG_ETHERNET,
       mojom::kEthernetDetailsSubpagePath,
       mojom::SearchResultIcon::kEthernet,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kEthernetDetails}},
      {IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY,
       mojom::kEthernetDetailsSubpagePath,
       mojom::SearchResultIcon::kEthernet,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kEthernetAutoConfigureIp},
       {IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY_ALT1,
        IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_NAME_SERVERS,
       mojom::kEthernetDetailsSubpagePath,
       mojom::SearchResultIcon::kEthernet,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kEthernetDns},
       {IDS_OS_SETTINGS_TAG_NAME_SERVERS_ALT1,
        IDS_OS_SETTINGS_TAG_NAME_SERVERS_ALT2, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_PROXY,
       mojom::kEthernetDetailsSubpagePath,
       mojom::SearchResultIcon::kEthernet,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kEthernetProxy},
       {IDS_OS_SETTINGS_TAG_PROXY_ALT1, IDS_OS_SETTINGS_TAG_PROXY_ALT2,
        IDS_OS_SETTINGS_TAG_PROXY_ALT3, IDS_OS_SETTINGS_TAG_PROXY_ALT4,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetEthernetNotConnectedSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ETHERNET,
       mojom::kNetworkSectionPath,
       mojom::SearchResultIcon::kEthernet,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kNetwork}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetWifiSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_WIFI,
       mojom::kWifiNetworksSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kWifiNetworks},
       {IDS_OS_SETTINGS_TAG_WIFI_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_KNOWN_NETWORKS,
       mojom::kKnownNetworksSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kKnownNetworks},
       {IDS_OS_SETTINGS_TAG_KNOWN_NETWORKS_ALT1,
        IDS_OS_SETTINGS_TAG_KNOWN_NETWORKS_ALT2, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetWifiOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_WIFI_TURN_OFF,
       mojom::kWifiNetworksSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kWifiOnOff},
       {IDS_OS_SETTINGS_TAG_WIFI_TURN_OFF_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_ADD_WIFI,
       mojom::kWifiNetworksSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kWifiAddNetwork}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetWifiOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_WIFI_TURN_ON,
       mojom::kWifiNetworksSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kWifiOnOff},
       {IDS_OS_SETTINGS_TAG_WIFI_TURN_ON_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetWifiConnectedSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_DISCONNECT_WIFI,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDisconnectWifiNetwork}},
      {IDS_OS_SETTINGS_TAG_PREFER_WIFI_NETWORK,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPreferWifiNetwork},
       {IDS_OS_SETTINGS_TAG_PREFER_WIFI_NETWORK_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_FORGET_WIFI,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kForgetWifiNetwork}},
      {IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kWifiAutoConfigureIp},
       {IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY_ALT1,
        IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_NAME_SERVERS,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kWifiDns},
       {IDS_OS_SETTINGS_TAG_NAME_SERVERS_ALT1,
        IDS_OS_SETTINGS_TAG_NAME_SERVERS_ALT2, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_PROXY,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kWifiProxy},
       {IDS_OS_SETTINGS_TAG_PROXY_ALT1, IDS_OS_SETTINGS_TAG_PROXY_ALT2,
        IDS_OS_SETTINGS_TAG_PROXY_ALT3, IDS_OS_SETTINGS_TAG_PROXY_ALT4,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_AUTO_CONNECT_NETWORK,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kWifiAutoConnectToNetwork},
       {IDS_OS_SETTINGS_TAG_AUTO_CONNECT_NETWORK_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetWifiMeteredSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_SETTINGS_INTERNET_NETWORK_METERED,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kWifiMetered}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetWifiHiddenSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_HIDDEN_NETWORK,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kWifiHidden},
       {IDS_OS_SETTINGS_TAG_HIDDEN_NETWORK_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCellularSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CELLULAR,
       mojom::kCellularNetworksSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kMobileDataNetworks},
       {IDS_OS_SETTINGS_TAG_CELLULAR_ALT1, IDS_OS_SETTINGS_TAG_CELLULAR_ALT2,
        IDS_OS_SETTINGS_TAG_CELLULAR_ALT3, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_CELLULAR_SIM_LOCK,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularSimLock},
       {IDS_OS_SETTINGS_TAG_CELLULAR_SIM_LOCK_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_CELLULAR_ROAMING,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularRoaming}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCellularOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CELLULAR_TURN_OFF,
       mojom::kNetworkSectionPath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMobileOnOff},
       {IDS_OS_SETTINGS_TAG_CELLULAR_TURN_OFF_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCellularOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CELLULAR_TURN_ON,
       mojom::kNetworkSectionPath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMobileOnOff},
       {IDS_OS_SETTINGS_TAG_CELLULAR_TURN_ON_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCellularConnectedSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CELLULAR_DISCONNECT,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDisconnectCellularNetwork}},
      {IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularAutoConfigureIp},
       {IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY_ALT1,
        IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_NAME_SERVERS,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularDns},
       {IDS_OS_SETTINGS_TAG_NAME_SERVERS_ALT1,
        IDS_OS_SETTINGS_TAG_NAME_SERVERS_ALT2, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_PROXY,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularProxy},
       {IDS_OS_SETTINGS_TAG_PROXY_ALT1, IDS_OS_SETTINGS_TAG_PROXY_ALT2,
        IDS_OS_SETTINGS_TAG_PROXY_ALT3, IDS_OS_SETTINGS_TAG_PROXY_ALT4,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_AUTO_CONNECT_NETWORK,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularAutoConnectToNetwork},
       {IDS_OS_SETTINGS_TAG_AUTO_CONNECT_NETWORK_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCellularAddESimSearchTerms() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ADD_ESIM,
       mojom::kMobileDataNetworksSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddESimNetwork},
       {IDS_OS_SETTINGS_TAG_ADD_ESIM_ALT1, IDS_OS_SETTINGS_TAG_ADD_ESIM_ALT2,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>&
GeActiveCellularNetworkApnSettingsSearchConcepts() {
  if (ash::features::IsApnRevampEnabled()) {
    static const base::NoDestructor<std::vector<SearchConcept>> tags(
        {{IDS_OS_SETTINGS_TAG_CELLULAR_APN_SETTINGS,
          mojom::kApnSubpagePath,
          mojom::SearchResultIcon::kCellular,
          mojom::SearchResultDefaultRank::kMedium,
          mojom::SearchResultType::kSubpage,
          {.subpage = mojom::Subpage::kApn},
          {IDS_OS_SETTINGS_TAG_CELLULAR_APN_SETTINGS_ALT_1,
           SearchConcept::kAltTagEnd}},
         {IDS_OS_SETTINGS_TAG_ADD_APN,
          mojom::kApnSubpagePath,
          mojom::SearchResultIcon::kCellular,
          mojom::SearchResultDefaultRank::kMedium,
          mojom::SearchResultType::kSetting,
          {.setting = mojom::Setting::kCellularAddApn},
          {IDS_OS_SETTINGS_TAG_ADD_APN_ALT1, SearchConcept::kAltTagEnd}}});
    return *tags;
  }

  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CELLULAR_APN,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularApn}},
  });
  return *tags;
}

const std::vector<SearchConcept>&
GetCellularPrimaryIsNonPolicyESimSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CELLULAR_REMOVE_PROFILE,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularRemoveESimNetwork},
       {IDS_OS_SETTINGS_TAG_CELLULAR_REMOVE_PROFILE_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_CELLULAR_RENAME_PROFILE,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularRenameESimNetwork},
       {IDS_OS_SETTINGS_TAG_CELLULAR_RENAME_PROFILE_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCellularMeteredSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_SETTINGS_INTERNET_NETWORK_METERED,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularMetered}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetInstantTetheringSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags([] {
    SearchConcept instant_tethering_concept{
        IDS_OS_SETTINGS_TAG_INSTANT_MOBILE_NETWORKS,
        mojom::kMobileDataNetworksSubpagePath,
        mojom::SearchResultIcon::kInstantTethering,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSubpage,
        {.subpage = mojom::Subpage::kMobileDataNetworks},
    };

    if (ash::features::IsInstantHotspotRebrandEnabled()) {
      instant_tethering_concept.alt_tag_ids[0] =
          IDS_OS_SETTINGS_TAG_INSTANT_MOBILE_NETWORKS_ALT1;
      instant_tethering_concept.alt_tag_ids[1] = SearchConcept::kAltTagEnd;
    }

    return std::vector<SearchConcept>{instant_tethering_concept};
  }());
  return *tags;
}

const std::vector<SearchConcept>& GetInstantTetheringOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {features::IsInstantHotspotRebrandEnabled()
           ? IDS_OS_SETTINGS_TAG_INSTANT_TETHERING_TURN_OFF
           : IDS_OS_SETTINGS_TAG_INSTANT_TETHERING_TURN_OFF_LEGACY,
       mojom::kMobileDataNetworksSubpagePath,
       mojom::SearchResultIcon::kInstantTethering,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kInstantTetheringOnOff},
       {features::IsInstantHotspotRebrandEnabled()
            ? IDS_OS_SETTINGS_TAG_INSTANT_TETHERING_TURN_OFF_ALT1
            : IDS_OS_SETTINGS_TAG_INSTANT_TETHERING_TURN_OFF_ALT1_LEGACY,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetInstantTetheringOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {features::IsInstantHotspotRebrandEnabled()
           ? IDS_OS_SETTINGS_TAG_INSTANT_TETHERING_TURN_ON
           : IDS_OS_SETTINGS_TAG_INSTANT_TETHERING_TURN_ON_LEGACY,
       mojom::kMobileDataNetworksSubpagePath,
       mojom::SearchResultIcon::kInstantTethering,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kInstantTetheringOnOff},
       {features::IsInstantHotspotRebrandEnabled()
            ? IDS_OS_SETTINGS_TAG_INSTANT_TETHERING_TURN_ON_ALT1
            : IDS_OS_SETTINGS_TAG_INSTANT_TETHERING_TURN_ON_ALT1_LEGACY,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetInstantTetheringConnectedSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {features::IsInstantHotspotRebrandEnabled()
           ? IDS_OS_SETTINGS_TAG_INSTANT_TETHERING_DISCONNECT
           : IDS_OS_SETTINGS_TAG_INSTANT_TETHERING_DISCONNECT_LEGACY,
       mojom::kTetherDetailsSubpagePath,
       mojom::SearchResultIcon::kInstantTethering,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDisconnectTetherNetwork}},
      {features::IsInstantHotspotRebrandEnabled()
           ? IDS_OS_SETTINGS_TAG_INSTANT_TETHERING
           : IDS_OS_SETTINGS_TAG_INSTANT_TETHERING_LEGACY,
       mojom::kTetherDetailsSubpagePath,
       mojom::SearchResultIcon::kInstantTethering,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kTetherDetails}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetVpnConnectedSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_VPN,
       mojom::kVpnDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kVpnDetails}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetHotspotSubpageSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_HOTSPOT,
       mojom::kHotspotSubpagePath,
       mojom::SearchResultIcon::kHotspot,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kHotspotDetails}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetHotspotOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_HOTSPOT_TURN_OFF,
       mojom::kHotspotSubpagePath,
       mojom::SearchResultIcon::kHotspot,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kHotspotOnOff}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetHotspotOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_HOTSPOT_TURN_ON,
       mojom::kHotspotSubpagePath,
       mojom::SearchResultIcon::kHotspot,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kHotspotOnOff}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetHotspotAutoDisabledSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_HOTSPOT_AUTO_DISABLED,
       mojom::kHotspotSubpagePath,
       mojom::SearchResultIcon::kHotspot,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kHotspotAutoDisabled},
       {IDS_OS_SETTINGS_TAG_HOTSPOT_AUTO_DISABLED_ALT1,
        IDS_OS_SETTINGS_TAG_HOTSPOT_AUTO_DISABLED_ALT2,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<mojom::Setting>& GetEthernetDetailsSettings() {
  static const base::NoDestructor<std::vector<mojom::Setting>> settings({
      mojom::Setting::kConfigureEthernet,
      mojom::Setting::kEthernetAutoConfigureIp,
      mojom::Setting::kEthernetDns,
      mojom::Setting::kEthernetProxy,
  });
  return *settings;
}

const std::vector<mojom::Setting>& GetWifiDetailsSettings() {
  static const base::NoDestructor<std::vector<mojom::Setting>> settings({
      mojom::Setting::kDisconnectWifiNetwork,
      mojom::Setting::kPreferWifiNetwork,
      mojom::Setting::kForgetWifiNetwork,
      mojom::Setting::kWifiAutoConfigureIp,
      mojom::Setting::kWifiDns,
      mojom::Setting::kWifiProxy,
      mojom::Setting::kWifiAutoConnectToNetwork,
      mojom::Setting::kWifiMetered,
      mojom::Setting::kWifiHidden,
  });
  return *settings;
}

const std::vector<mojom::Setting>& GetCellularDetailsSettings() {
  static const base::NoDestructor<std::vector<mojom::Setting>> settings({
      mojom::Setting::kCellularSimLock,
      mojom::Setting::kCellularRoaming,
      mojom::Setting::kCellularApn,
      mojom::Setting::kDisconnectCellularNetwork,
      mojom::Setting::kCellularAutoConfigureIp,
      mojom::Setting::kCellularDns,
      mojom::Setting::kCellularProxy,
      mojom::Setting::kCellularAutoConnectToNetwork,
      mojom::Setting::kCellularMetered,
      mojom::Setting::kCellularRemoveESimNetwork,
      mojom::Setting::kCellularRenameESimNetwork,
      mojom::Setting::kCellularAddApn,
  });
  return *settings;
}

const std::vector<mojom::Setting>& GetHotspotDetailsSettings() {
  static const base::NoDestructor<std::vector<mojom::Setting>> settings({
      mojom::Setting::kHotspotOnOff,
      mojom::Setting::kHotspotAutoDisabled,
  });
  return *settings;
}

const std::vector<mojom::Setting>& GetTetherDetailsSettings() {
  static const base::NoDestructor<std::vector<mojom::Setting>> settings({
      mojom::Setting::kDisconnectTetherNetwork,
  });
  return *settings;
}

bool IsConnected(network_config::mojom::ConnectionStateType connection_state) {
  return connection_state ==
             network_config::mojom::ConnectionStateType::kOnline ||
         connection_state ==
             network_config::mojom::ConnectionStateType::kConnected;
}

bool IsPartOfDetailsSubpage(mojom::SearchResultType type,
                            OsSettingsIdentifier id,
                            mojom::Subpage details_subpage) {
  switch (type) {
    case mojom::SearchResultType::kSection:
      // Applies to a section, not a details subpage.
      return false;

    case mojom::SearchResultType::kSubpage:
      return id.subpage == details_subpage;

    case mojom::SearchResultType::kSetting: {
      const mojom::Setting& setting = id.setting;
      switch (details_subpage) {
        case mojom::Subpage::kEthernetDetails:
          return base::Contains(GetEthernetDetailsSettings(), setting);
        case mojom::Subpage::kWifiDetails:
          return base::Contains(GetWifiDetailsSettings(), setting);
        case mojom::Subpage::kCellularDetails:
          return base::Contains(GetCellularDetailsSettings(), setting);
        case mojom::Subpage::kTetherDetails:
          return base::Contains(GetTetherDetailsSettings(), setting);
        default:
          return false;
      }
    }
  }
}

std::string GetDetailsSubpageUrl(const std::string& url_to_modify,
                                 const std::string& guid) {
  return base::StringPrintf(
      "%s%sguid=%s", url_to_modify.c_str(),
      url_to_modify.find('?') == std::string::npos ? "?" : "&", guid.c_str());
}

bool AllowAddESim(const network_config::mojom::GlobalPolicyPtr& global_policy) {
  if (HermesManagerClient::Get()->GetAvailableEuiccs().size() == 0) {
    return false;
  }

  return !global_policy->allow_only_policy_cellular_networks;
}

std::optional<std::string> GetCellularActiveSimIccid(
    const network_config::mojom::DeviceStatePropertiesPtr& device) {
  for (const auto& sim_info : *device->sim_infos) {
    if (sim_info->is_primary) {
      return sim_info->iccid;
    }
  }
  return std::nullopt;
}

bool IsPolicySource(network_config::mojom::OncSource onc_source) {
  return onc_source == network_config::mojom::OncSource::kUserPolicy ||
         onc_source == network_config::mojom::OncSource::kDevicePolicy;
}

bool IsUserLoggedIn() {
  return user_manager::UserManager::Get() &&
         user_manager::UserManager::Get()->IsUserLoggedIn();
}

}  // namespace

InternetSection::InternetSection(Profile* profile,
                                 SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  // General network search tags are always added.
  updater.AddSearchTags(GetNetworkSearchConcepts());

  // Receive updates when devices (e.g., Ethernet, Wi-Fi) go on/offline.
  GetNetworkConfigService(cros_network_config_.BindNewPipeAndPassReceiver());
  cros_network_config_->AddObserver(
      network_config_receiver_.BindNewPipeAndPassRemote());

  // Receive updates when hotspot info changed.
  GetHotspotConfigService(cros_hotspot_config_.BindNewPipeAndPassReceiver());
  cros_hotspot_config_->AddObserver(
      hotspot_config_receiver_.BindNewPipeAndPassRemote());

  // Fetch initial list of devices and active networks.
  FetchDeviceList();
  FetchNetworkList();

  // Fetch initial hotspot info.
  FetchHotspotInfo();
}

InternetSection::~InternetSection() = default;

void InternetSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  const bool isRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  webui::LocalizedString kLocalizedStrings[] = {
      {"deviceInfoPopupMenuItemTitle",
       IDS_SETTINGS_DEVICE_INFO_POPUP_MENU_ITEM_TITLE},
      {"deviceInfoPopupEidLabel", IDS_SETTINGS_DEVICE_INFO_EID_LABEL},
      {"deviceInfoPopupImeiLabel", IDS_SETTINGS_DEVICE_INFO_IMEI_LABEL},
      {"deviceInfoPopupSerialLabel", IDS_SETTINGS_DEVICE_INFO_SERIAL_LABEL},
      {"deviceInfoPopupA11yEid", IDS_SETTINGS_DEVICE_INFO_A11Y_LABEL_EID},
      {"deviceInfoPopupA11yImei", IDS_SETTINGS_DEVICE_INFO_A11Y_LABEL_IMEI},
      {"deviceInfoPopupA11ySerial", IDS_SETTINGS_DEVICE_INFO_A11Y_LABEL_SERIAL},
      {"deviceInfoPopupA11yEidAndImei",
       IDS_SETTINGS_DEVICE_INFO_A11Y_LABEL_EID_AND_IMEI},
      {"deviceInfoPopupA11yEidAndSerial",
       IDS_SETTINGS_DEVICE_INFO_A11Y_LABEL_EID_AND_SERIAL},
      {"deviceInfoPopupA11yImeiAndSerial",
       IDS_SETTINGS_DEVICE_INFO_A11Y_LABEL_IMEI_AND_SERIAL},
      {"deviceInfoPopupA11yEidImeiAndSerial",
       IDS_SETTINGS_DEVICE_INFO_A11Y_LABEL_EID_IMEI_AND_SERIAL},
      {"internetMenuItemDescriptionWifi",
       IDS_OS_SETTINGS_INTERNET_MENU_ITEM_DESCRIPTION_WIFI_ONLY},
      {"internetMenuItemDescriptionWifiAndMobileData",
       IDS_OS_SETTINGS_INTERNET_MENU_ITEM_DESCRIPTION_WIFI_AND_MOBILE_DATA},
      {"internetMenuItemDescriptionInstantHotspotAvailable",
       IDS_OS_SETTINGS_INTERNET_MENU_ITEM_DESCRIPTION_INSTANT_HOTSPOT_AVAILABLE},
      {"internetAddCellular", IDS_SETTINGS_INTERNET_ADD_CELLULAR},
      {"internetAddConnection", IDS_SETTINGS_INTERNET_ADD_CONNECTION},
      {"internetAddConnectionExpandA11yLabel",
       IDS_SETTINGS_INTERNET_ADD_CONNECTION_EXPAND_ACCESSIBILITY_LABEL},
      {"internetAddConnectionNotAllowed",
       IDS_SETTINGS_INTERNET_ADD_CONNECTION_NOT_ALLOWED},
      {"internetAddThirdPartyVPN", IDS_SETTINGS_INTERNET_ADD_THIRD_PARTY_VPN},
      {"internetAddVPN", IDS_SETTINGS_INTERNET_ADD_VPN},
      {"internetAddWiFi", IDS_SETTINGS_INTERNET_ADD_WIFI},
      {"internetConfigName", IDS_SETTINGS_INTERNET_CONFIG_NAME},
      {"internetDetailPageTitle", IDS_SETTINGS_INTERNET_DETAIL},
      {"internetDeviceBusy", IDS_SETTINGS_INTERNET_DEVICE_BUSY},
      {"internetDeviceFlashing", IDS_SETTINGS_INTERNET_DEVICE_FLASHING},
      {"internetJoinType", IDS_SETTINGS_INTERNET_JOIN_TYPE},
      {"internetKnownNetworksPageTitle", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS},
      {"internetYourDeviceHotspots",
       IDS_SETTINGS_INTERNET_YOUR_DEVICE_HOTSPOTS},
      {"internetTetherNotificationControlTitle",
       IDS_SETTINGS_INTERNET_TETHER_NOTIFICATION_CONTROL_TITLE},
      {"internetTetherNotificationControlDescription",
       IDS_SETTINGS_INTERNET_TETHER_NOTIFICATION_CONTROL_DESCRIPTION},
      {"internetNoNetworks", IDS_SETTINGS_INTERNET_NO_NETWORKS},
      {"internetNoTetherHosts", IDS_SETTINGS_INTERNET_NO_TETHER_HOSTS},
      {"internetPageTitle", features::IsInstantHotspotRebrandEnabled()
                                ? IDS_SETTINGS_INTERNET
                                : IDS_SETTINGS_INTERNET_LEGACY},
      {"internetSummaryButtonA11yLabel",
       IDS_SETTINGS_INTERNET_SUMMARY_BUTTON_ACCESSIBILITY_LABEL},
      {"internetToggleTetherA11yLabel",
       IDS_SETTINGS_INTERNET_TOGGLE_MOBILE_ACCESSIBILITY_LABEL},
      {"internetToggleMobileA11yLabel",
       IDS_SETTINGS_INTERNET_TOGGLE_MOBILE_ACCESSIBILITY_LABEL},
      {"internetToggleWiFiA11yLabel",
       IDS_SETTINGS_INTERNET_TOGGLE_WIFI_ACCESSIBILITY_LABEL},
      {"knownNetworksAll", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_ALL},
      {"knownNetworksButton", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_BUTTON},
      {"knownNetworksMessage",
       isRevampEnabled ? IDS_OS_SETTINGS_REVAMP_INTERNET_KNOWN_NETWORKS_MESSAGE
                       : IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MESSAGE},
      {"knownNetworksPreferred",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_PREFFERED},
      {"knownNetworksMenuAddPreferred",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MENU_ADD_PREFERRED},
      {"knownNetworksMenuRemovePreferred",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MENU_REMOVE_PREFERRED},
      {"knownNetworksMenuForget",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MENU_FORGET},
      {"knownNetworksMenuButtonTitle",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MENU_BUTTON_TITLE},
      {"mobileDeviceInfoPopupDescription",
       IDS_SETTINGS_MOBILE_DEVICE_INFO_POPUP_DESCRIPTION},
      {"mobileDeviceInfoPopupTitle",
       IDS_SETTINGS_MOBILE_DEVICE_INFO_POPUP_TITLE},
      {"mobileNetworkScanningLabel", IDS_MOBILE_NETWORK_SCANNING_MESSAGE},
      {"networkAllowDataRoaming",
       IDS_SETTINGS_SETTINGS_NETWORK_ALLOW_DATA_ROAMING},
      {"networkAllowDataRoamingRequired",
       IDS_SETTINGS_SETTINGS_NETWORK_ALLOW_DATA_ROAMING_REQUIRED},
      {"networkAllowDataRoamingEnabledHome",
       IDS_SETTINGS_SETTINGS_NETWORK_ALLOW_DATA_ROAMING_ENABLED_HOME},
      {"networkAllowDataRoamingEnabledRoaming",
       IDS_SETTINGS_SETTINGS_NETWORK_ALLOW_DATA_ROAMING_ENABLED_ROAMING},
      {"networkAllowDataRoamingDisabled",
       IDS_SETTINGS_SETTINGS_NETWORK_ALLOW_DATA_ROAMING_DISABLED},
      {"networkDeviceTurningOn", IDS_SETTINGS_NETWORK_DEVICE_TURNING_ON},
      {"networkVpnPreferences", IDS_SETTINGS_INTERNET_NETWORK_VPN_PREFERENCES},
      {"networkAlwaysOnVpn", IDS_SETTINGS_INTERNET_NETWORK_ALWAYS_ON_VPN},
      {"networkAlwaysOnVpnEnableSublabel",
       IDS_SETTINGS_INTERNET_NETWORK_ALWAYS_ON_VPN_ENABLE_SUBLABEL},
      {"networkAlwaysOnVpnEnableLabel",
       IDS_SETTINGS_INTERNET_NETWORK_ALWAYS_ON_VPN_ENABLE_LABEL},
      {"networkAlwaysOnVpnLockdownLabel",
       IDS_SETTINGS_INTERNET_NETWORK_ALWAYS_ON_VPN_LOCKDOWN_LABEL},
      {"networkAlwaysOnVpnLockdownSublabel",
       IDS_SETTINGS_INTERNET_NETWORK_ALWAYS_ON_VPN_LOCKDOWN_SUBLABEL},
      {"networkAlwaysOnVpnService",
       IDS_SETTINGS_INTERNET_NETWORK_ALWAYS_ON_VPN_SERVICE},
      {"networkAutoConnect", IDS_SETTINGS_INTERNET_NETWORK_AUTO_CONNECT},
      {"networkAutoConnectCellular",
       IDS_SETTINGS_INTERNET_NETWORK_AUTO_CONNECT_CELLULAR},
      {"networkButtonActivate", IDS_SETTINGS_INTERNET_BUTTON_ACTIVATE},
      {"networkButtonConfigure", IDS_SETTINGS_INTERNET_BUTTON_CONFIGURE},
      {"networkButtonConnect", IDS_SETTINGS_INTERNET_BUTTON_CONNECT},
      {"networkButtonDisconnect", IDS_SETTINGS_INTERNET_BUTTON_DISCONNECT},
      {"networkButtonForget", IDS_SETTINGS_INTERNET_BUTTON_FORGET},
      {"networkButtonSignin", IDS_SETTINGS_INTERNET_BUTTON_SIGNIN},
      {"networkButtonViewAccount", IDS_SETTINGS_INTERNET_BUTTON_VIEW_ACCOUNT},
      {"networkConnectNotAllowed", IDS_SETTINGS_INTERNET_CONNECT_NOT_ALLOWED},
      {"networkHidden", IDS_SETTINGS_INTERNET_NETWORK_HIDDEN},
      {"networkHiddenSublabel", IDS_SETTINGS_INTERNET_NETWORK_HIDDEN_SUBLABEL},
      {"networkIPAddress", IDS_SETTINGS_INTERNET_NETWORK_IP_ADDRESS},
      {"networkIPConfigAuto", IDS_SETTINGS_INTERNET_NETWORK_IP_CONFIG_AUTO},
      {"networkMetered", IDS_SETTINGS_INTERNET_NETWORK_METERED},
      {"networkMeteredDesc", IDS_SETTINGS_INTERNET_NETWORK_METERED_DESC},
      {"networkNameserversLearnMore", IDS_LEARN_MORE},
      {"networkPrefer", IDS_SETTINGS_INTERNET_NETWORK_PREFER},
      {"networkPreferDescription",
       IDS_OS_SETTINGS_REVAMP_INTERNET_NETWORK_PREFER_DESCRIPTION},
      {"networkPrimaryUserControlled",
       IDS_SETTINGS_INTERNET_NETWORK_PRIMARY_USER_CONTROLLED},
      {"networkA11yManagedByAdministrator",
       IDS_SETTINGS_INTERNET_A11Y_MANAGED_BY_ADMINISTRATOR},
      {"networkDetailMenuRemoveESim",
       IDS_SETTINGS_INTERNET_NETWORK_MENU_REMOVE},
      {"networkDetailMenuRenameESim",
       IDS_SETTINGS_INTERNET_NETWORK_MENU_RENAME},
      {"networkScanningLabel", IDS_NETWORK_SCANNING_MESSAGE},
      {"networkSectionAdvanced",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_ADVANCED},
      {"networkSectionAdvancedA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_ADVANCED_ACCESSIBILITY_LABEL},
      {"networkSectionNetwork", IDS_SETTINGS_INTERNET_NETWORK_SECTION_NETWORK},
      {"networkSectionNetworkExpandA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_NETWORK_ACCESSIBILITY_LABEL},
      {"networkSectionPasspointGoToSubscriptionTitle",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_PASSPOINT_GO_TO_SUBSCRIPTION_TITLE},
      {"networkSectionPasspointGoToSubscriptionInformation",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_PASSPOINT_GO_TO_SUBSCRIPTION_INFORMATION},
      {"networkSectionPasspointGoToSubscriptionButtonLabel",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_PASSPOINT_GO_TO_SUBSCRIPTION_BUTTON},
      {"networkSuppressTextMessages", IDS_SUPPRESS_TEXT_MESSAGES},
      {"passpointRemoveGoToSubscriptionButtonA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_PASSPOINT_GO_TO_SUBSCRIPTION_BUTTON_A11Y_LABEL},
      {"networkSectionProxy", IDS_SETTINGS_INTERNET_NETWORK_SECTION_PROXY},
      {"networkSectionProxyExpandA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_PROXY_ACCESSIBILITY_LABEL},
      {"networkShared", IDS_SETTINGS_INTERNET_NETWORK_SHARED},
      {"networkSharedOwner",
       isRevampEnabled ? IDS_OS_SETTINGS_REVAMP_INTERNET_NETWORK_SHARED_OWNER
                       : IDS_SETTINGS_INTERNET_NETWORK_SHARED_OWNER},
      {"networkSharedNotOwner", IDS_SETTINGS_INTERNET_NETWORK_SHARED_NOT_OWNER},
      {"networkVpnBuiltin", IDS_NETWORK_TYPE_VPN_BUILTIN},
      {"networkOutOfRange", IDS_SETTINGS_INTERNET_WIFI_NETWORK_OUT_OF_RANGE},
      {"networkMobileProviderLocked",
       IDS_SETTINGS_INTERNET_MOBILE_PROVIDER_LOCKED},
      {"cellularSetupDialogTitle",
       IDS_SETTINGS_INTERNET_CELLULAR_SETUP_DIALOG_TITLE},
      {"tetherPhoneOutOfRange",
       IDS_SETTINGS_INTERNET_TETHER_PHONE_OUT_OF_RANGE},
      {"gmscoreNotificationsTitle",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_TITLE},
      {"gmscoreNotificationsOneDeviceSubtitle",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_ONE_DEVICE_SUBTITLE},
      {"gmscoreNotificationsTwoDevicesSubtitle",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_TWO_DEVICES_SUBTITLE},
      {"gmscoreNotificationsManyDevicesSubtitle",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_MANY_DEVICES_SUBTITLE},
      {"gmscoreNotificationsFirstStep",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_FIRST_STEP},
      {"gmscoreNotificationsSecondStep",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_SECOND_STEP},
      {"gmscoreNotificationsThirdStep",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_THIRD_STEP},
      {"gmscoreNotificationsFourthStep",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_FOURTH_STEP},
      {"tetherConnectionDialogTitle",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DIALOG_TITLE},
      {"tetherConnectionAvailableDeviceTitle",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_AVAILABLE_DEVICE_TITLE},
      {"tetherConnectionBatteryPercentage",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_BATTERY_PERCENTAGE},
      {"tetherConnectionExplanation",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_EXPLANATION},
      {"tetherConnectionCarrierWarning",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_CARRIER_WARNING},
      {"tetherConnectionDescriptionTitle",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DESCRIPTION_TITLE},
      {"tetherConnectionDescriptionMobileData",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DESCRIPTION_MOBILE_DATA},
      {"tetherConnectionDescriptionBattery",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DESCRIPTION_BATTERY},
      {"tetherConnectionDescriptionWiFi",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DESCRIPTION_WIFI},
      {"tetherConnectionNotNowButton",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_NOT_NOW_BUTTON},
      {"tetherConnectionConnectButton",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_CONNECT_BUTTON},
      {"tetherEnableBluetooth", IDS_ENABLE_BLUETOOTH},
      {"cellularNetworkEsimLabel", IDS_SETTINGS_INTERNET_ESIM_LABEL},
      {"cellularNetworkPsimLabel", IDS_SETTINGS_INTERNET_PSIM_LABEL},
      {"pSimNotInsertedLabel", IDS_SETTINGS_INTERNET_PSIM_NOT_INSERTED_LABEL},
      {"eSimNetworkNotSetupWithDownloadLink",
       IDS_SETTINGS_INTERNET_ESIM_NOT_SETUP_WITH_SETUP_LINK},
      {"eSimNetworkNotSetup", IDS_SETTINGS_INTERNET_ESIM_NOT_SETUP},
      {"cellularNetworkTetherLabel", IDS_SETTINGS_INTERNET_TETHER_LABEL},
      {"showEidPopupButtonLabel",
       IDS_SETTINGS_INTERNET_SHOW_EID_POPUP_BUTTON_LABEL},
      {"eSimNoConnectionErrorToast",
       IDS_SETTINGS_INTERNET_ESIM_NO_CONNECTION_ERROR_TOAST},
      {"eSimMobileDataNotEnabledErrorToast",
       IDS_SETTINGS_INTERNET_ESIM_MOBILE_DATA_NOT_ENABLED_ERROR_TOAST},
      {"eSimProfileLimitReachedErrorToast",
       IDS_SETTINGS_INTERNET_ESIM_PROFILE_LIMIT_REACHED_ERROR_TOAST},
      {"eSimInstallErrorDialogTitle",
       IDS_SETTINGS_INTERNET_NETWORK_INSTALL_ERROR_DIALOG_TITLE},
      {"eSimInstallErrorDialogConfirmationCodeMessage",
       IDS_SETTINGS_INTERNET_NETWORK_INSTALL_ERROR_DIALOG_CONFIRMATION_CODE_MESSAGE},
      {"eSimInstallErrorDialogConfirmationCodeError",
       IDS_CELLULAR_SETUP_ESIM_PAGE_INSTALL_ERROR_DIALOG_CONFIRMATION_CODE_ERROR},
      {"eSimInstallErrorDialogGenericErrorMessage",
       IDS_SETTINGS_INTERNET_NETWORK_INSTALL_ERROR_DIALOG_GENERIC_ERROR_MESSAGE},
      {"eSimRenameProfileDialogLabel",
       IDS_SETTINGS_INTERNET_NETWORK_RENAME_DIALOG_RENAME_PROFILE},
      {"eSimRenameProfileDialogDone",
       IDS_SETTINGS_INTERNET_NETWORK_RENAME_DIALOG_DONE},
      {"eSimRenameProfileDialogCancel",
       IDS_SETTINGS_INTERNET_NETWORK_RENAME_DIALOG_CANCEL},
      {"eSimRenameProfileInputTitle",
       IDS_SETTINGS_INTERNET_NETWORK_RENAME_INPUT_TITLE},
      {"eSimRenameProfileInputSubtitle",
       IDS_SETTINGS_INTERNET_NETWORK_RENAME_INPUT_SUBTITLE},
      {"eSimRenameProfileInputCharacterCount",
       IDS_SETTINGS_INTERNET_NETWORK_RENAME_INPUT_CHARACTER_COUNT},
      {"eSimRenameProfileDoneBtnA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_RENAME_DONE_BUTTON_A11Y_LABEL},
      {"eSimRenameProfileInputA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_RENAME_INPUT_A11Y_LABEL},
      {"eSimRenameProfileDialogError",
       IDS_SETTINGS_INTERNET_NETWORK_RENAME_DIALOG_ERROR_MESSAGE},
      {"eSimRenameProfileDialogErrorToast",
       IDS_SETTINGS_INTERNET_NETWORK_RENAME_DIALOG_ERROR_TOAST},
      {"eSimRemoveProfileDialogCancel",
       IDS_SETTINGS_INTERNET_NETWORK_REMOVE_PROFILE_DIALOG_CANCEL},
      {"esimRemoveProfileDialogTitle",
       IDS_SETTINGS_INTERNET_NETWORK_REMOVE_PROFILE_DIALOG_TITLE},
      {"eSimRemoveProfileDialogDescription",
       IDS_SETTINGS_INTERNET_NETWORK_REMOVE_PROFILE_DIALOG_DESCRIPTION},
      {"eSimRemoveProfileDialogRemove",
       IDS_SETTINGS_INTERNET_NETWORK_REMOVE_PROFILE_DIALOG_REMOVE},
      {"eSimRemoveProfileDialogError",
       IDS_SETTINGS_INTERNET_NETWORK_REMOVE_PROFILE_DIALOG_ERROR_MESSAGE},
      {"eSimRemoveProfileDialogOkay",
       IDS_SETTINGS_INTERNET_NETWORK_REMOVE_PROFILE_DIALOG_OKAY},
      {"eSimRemoveProfileCancelA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_REMOVE_PROFILE_DIALOG_A11Y_CANCEL},
      {"eSimRemoveProfileRemoveA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_REMOVE_PROFILE_DIALOG_A11Y_REMOVE},
      {"eSimDialogConnectionWarning",
       IDS_SETTINGS_INTERNET_ESIM_DIALOG_CONNECTION_WARNING},
      {"cellularNetworkInstallingProfile",
       IDS_SETTINGS_INTERNET_NETWORK_CELLULAR_INSTALLING_PROFILE},
      {"cellularNetworkRemovingProfile",
       IDS_SETTINGS_INTERNET_NETWORK_CELLULAR_REMOVING_PROFILE},
      {"cellularNetworkRenamingProfile",
       IDS_SETTINGS_INTERNET_NETWORK_CELLULAR_RENAMING_PROFILE},
      {"cellularNetworkConnectingToProfile",
       IDS_SETTINGS_INTERNET_NETWORK_CELLULAR_CONNECTING_TO_PROFILE},
      {"cellularNetworRefreshingProfileListProfile",
       IDS_SETTINGS_INTERNET_NETWORK_CELLULAR_REFRESHING_PROFILE_LIST},
      {"cellularNetworkResettingESim",
       IDS_SETTINGS_INTERNET_NETWORK_CELLULAR_RESETTING_ESIM},
      {"cellularNetworkRequestingAvailableProfiles",
       IDS_SETTINGS_INTERNET_NETWORK_CELLULAR_REQUESTING_AVAILABLE_PROFILES},
      {"hotspotPageTitle", IDS_SETTINGS_INTERNET_HOTSPOT},
      {"hotspotToggleA11yLabel",
       IDS_SETTINGS_INTERNET_HOTSPOT_TOGGLE_A11Y_LABEL},
      {"hotspotSummaryStateOn", IDS_SETTINGS_INTERNET_HOTSPOT_SUMMARY_STATE_ON},
      {"hotspotSummaryStateTurningOn",
       IDS_SETTINGS_INTERNET_HOTSPOT_SUMMARY_STATE_TURNING_ON},
      {"hotspotSummaryStateOff",
       IDS_SETTINGS_INTERNET_HOTSPOT_SUMMARY_STATE_OFF},
      {"hotspotSummaryStateTurningOff",
       IDS_SETTINGS_INTERNET_HOTSPOT_SUMMARY_STATE_TURNING_OFF},
      {"hotspotEnabledA11yLabel",
       IDS_SETTINGS_INTERNET_HOTSPOT_ENABLED_A11Y_LABEL},
      {"hotspotDisabledA11yLabel",
       IDS_SETTINGS_INTERNET_HOTSPOT_DISABLED_A11Y_LABEL},
      {"hotspotNameLabel", IDS_SETTINGS_INTERNET_HOTSPOT_NAME_LABEL},
      {"hotspotConfigureButton",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIGURE_BUTTON},
      {"hotspotConnectedDeviceCountLabel",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONNECTED_DEVICE_COUNT_LABEL},
      {"hotspotAutoDisableLabel",
       IDS_SETTINGS_INTERNET_HOTSPOT_AUTO_DISABLED_LABEL},
      {"hotspotAutoDisableSublabel",
       IDS_SETTINGS_INTERNET_HOTSPOT_AUTO_DISABLED_SUBLABEL},
      {"hotspotConfigNameLabel",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_NAME_LABEL},
      {"hotspotConfigNameInfo", IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_NAME_INFO},
      {"hotspotConfigNameEmptyInfo",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_NAME_EMPTY_INFO},
      {"hotspotConfigNameTooLongInfo",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_NAME_TOO_LONG_INFO},
      {"hotspotConfigPasswordLabel",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_PASSWORD_LABEL},
      {"hotspotConfigPasswordInfo",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_PASSWORD_INFO},
      {"hotspotConfigSecurityLabel",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_SECURITY_LABEL},
      {"hotspotConfigBssidToggleLabel",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_BSSID_TOGGLE_LABEL},
      {"hotspotConfigBssidToggleSublabel",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_BSSID_TOGGLE_SUBLABEL},
      {"hotspotConfigCompatibilityToggleLabel",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_COMPATIBILITY_TOGGLE_LABEL},
      {"hotspotConfigCompatibilityToggleSublabel",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_COMPATIBILITY_TOGGLE_SUBLABEL},
      {"hotspotConfigWarningMessage",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_WARNING_MESSAGE},
      {"hotspotConfigSaveButton",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_SAVE_BUTTON},
      {"hotspotConfigCancelButton",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_CANCEL_BUTTON},
      {"hotspotConfigGeneralErrorMessage",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_GENERAL_ERROR_MESSAGE},
      {"hotspotConfigInvalidConfigurationErrorMessage",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_INVALID_CONFIGURATION_ERROR_MESSAGE},
      {"hotspotConfigNotLoginErrorMessage",
       IDS_SETTINGS_INTERNET_HOTSPOT_CONFIG_NOT_LOGIN_ERROR_MESSAGE},
      {"passpointProviderLabel",
       IDS_SETTINGS_INTERNET_PASSPOINT_PROVIDER_LABEL},
      {"passpointRemoveButton",
       IDS_SETTINGS_INTERNET_PASSPOINT_REMOVE_SUBSCRIPTION},
      {"passpointSectionLabel", IDS_SETTINGS_INTERNET_PASSPOINT_SECTION_LABEL},
      {"passpointHeadlineText", IDS_SETTINGS_INTERNET_PASSPOINT_HEADLINE},
      {"passpointSubscriptionExpirationLabel",
       IDS_SETTINGS_INTERNET_PASSPOINT_SUBSCRIPTION_EXPIRATION},
      {"passpointSourceLabel", IDS_SETTINGS_INTERNET_PASSPOINT_SOURCE},
      {"passpointTrustedCALabel", IDS_SETTINGS_INTERNET_PASSPOINT_TRUSTED_CA},
      {"passpointSystemCALabel", IDS_SETTINGS_INTERNET_PASSPOINT_SYSTEM_CA},
      {"passpointAssociatedWifiNetworks",
       IDS_SETTINGS_INTERNET_PASSPOINT_ASSOCIATED_WIFI_NETWORKS},
      {"passpointDomainsLabel", IDS_SETTINGS_INTERNET_PASSPOINT_DOMAINS},
      {"passpointDomainsA11yLabel",
       IDS_SETTINGS_INTERNET_PASSPOINT_DOMAINS_A11Y_LABEL},
      {"passpointRemovalTitle", IDS_SETTINGS_INTERNET_PASSPOINT_REMOVAL_TITLE},
      {"passpointRemovalDescription",
       IDS_SETTINGS_INTERNET_PASSPOINT_REMOVAL_DESCRIPTION},
      {"passpointLearnMoreA11yLabel",
       IDS_SETTINGS_INTERNET_PASSPOINT_LEARN_MORE_A11Y},
      {"passpointRemoveCancelA11yLabel",
       IDS_SETTINGS_INTERNET_PASSPOINT_REMOVE_CANCEL_A11Y},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  ui::network_element::AddLocalizedStrings(html_source);
  ui::network_element::AddOncLocalizedStrings(html_source);
  ui::network_element::AddDetailsLocalizedStrings(html_source);
  ui::network_element::AddConfigLocalizedStrings(html_source);
  ui::network_element::AddErrorLocalizedStrings(html_source);
  cellular_setup::AddNonStringLoadTimeData(html_source);
  cellular_setup::AddLocalizedStrings(html_source);
  network_health::AddResources(html_source);
  traffic_counters::AddResources(html_source);

  html_source->AddBoolean(
      "bypassConnectivityCheck",
      base::FeatureList::IsEnabled(
          features::kCellularBypassESimInstallationConnectivityCheck));
  html_source->AddBoolean("showTechnologyBadge",
                          !ash::features::IsSeparateNetworkIconsEnabled());
  html_source->AddBoolean(
      "showMeteredToggle",
      base::FeatureList::IsEnabled(::features::kMeteredShowToggle));
  html_source->AddBoolean(
      "trafficCountersForWifiTesting",
      ash::features::IsTrafficCountersForWiFiTestingEnabled());
  html_source->AddBoolean(
      "showHiddenToggle",
      base::FeatureList::IsEnabled(::features::kShowHiddenNetworkToggle));
  html_source->AddBoolean("isInstantHotspotRebrandEnabled",
                          ash::features::IsInstantHotspotRebrandEnabled());

  html_source->AddString("networkGoogleNameserversLearnMoreUrl",
                         chrome::kGoogleNameserversLearnMoreURL);

  html_source->AddString("wifiHiddenNetworkLearnMoreUrl",
                         chrome::kWifiHiddenNetworkURL);

  html_source->AddString("wifiPasspointLearnMoreUrl",
                         chrome::kWifiPasspointURL);

  html_source->AddString(
      "networkNotSynced",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_INTERNET_NETWORK_NOT_SYNCED,
          GetHelpUrlWithBoard(chrome::kWifiSyncLearnMoreURL)));
  html_source->AddString(
      "networkSyncedUser",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_INTERNET_NETWORK_SYNCED_USER,
          GetHelpUrlWithBoard(chrome::kWifiSyncLearnMoreURL)));
  html_source->AddString(
      "networkSyncedDevice",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_INTERNET_NETWORK_SYNCED_DEVICE,
          GetHelpUrlWithBoard(chrome::kWifiSyncLearnMoreURL)));
  html_source->AddString(
      "internetNoNetworksMobileData",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_INTERNET_LOOKING_FOR_MOBILE_NETWORK,
          GetHelpUrlWithBoard(chrome::kInstantTetheringLearnMoreURL)));
  html_source->AddString(
      "tetherNetworkNotSetup",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_INTERNET_TETHER_NOT_SETUP_WITH_LEARN_MORE_LINK,
          GetHelpUrlWithBoard(chrome::kInstantTetheringLearnMoreURL)));
  // TODO(b/259623645): Replace learn more link with hotspot url once it is
  // ready.
  html_source->AddString(
      "hotspotSubpageSubtitle",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_INTERNET_HOTSPOT_SUBTITLE_WITH_LEARN_MORE_LINK,
          ui::GetChromeOSDeviceName(),
          GetHelpUrlWithBoard(chrome::kChromebookHotspotLearnMoreURL)));
  html_source->AddString(
      "hotspotMobileDataNotSupportedSublabelWithLink",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_INTERNET_HOTSPOT_MOBILE_DATA_NOT_SUPPORTED_SUBLABEL_WITH_LEARN_MORE_LINK,
          GetHelpUrlWithBoard(chrome::kChromebookHotspotLearnMoreURL)));
  html_source->AddString(
      "hotspotNoMobileDataSublabelWithLink",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_INTERNET_HOTSPOT_NO_MOBILE_DATA_SUBLABEL_WITH_LEARN_MORE_LINK,
          GetHelpUrlWithBoard(chrome::kChromebookHotspotLearnMoreURL)));
  html_source->AddString(
      "hotspotSettingsTitle",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_INTERNET_HOTSPOT_SETTINGS_TITLE,
                                 ui::GetChromeOSDeviceName()));
  html_source->AddString(
      "hotspotSettingsSubtitle",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_INTERNET_HOTSPOT_SETTINGS_SUBTITLE_WITH_LEARN_MORE_LINK,
          ui::GetChromeOSDeviceName(),
          GetHelpUrlWithBoard(chrome::kChromebookHotspotLearnMoreURL)));

  html_source->AddString(
      "cellularSubpageSubtitle",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_INTERNET_CELLULAR_SUBTITLE_WITH_LEARN_MORE_LINK,
          GetHelpUrlWithBoard(chrome::kCellularCarrierLockLearnMoreURL)));

  html_source->AddString(
      "networkCarrierLocked",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_INTERNET_NETWORK_CARRIER_LOCKED_WITH_LEARN_MORE_LINK,
          GetHelpUrlWithBoard(chrome::kCellularCarrierLockLearnMoreURL)));

  html_source->AddBoolean("isUserLoggedIn", IsUserLoggedIn());
}

void InternetSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(std::make_unique<InternetHandler>(profile()));
  web_ui->AddMessageHandler(std::make_unique<ExtensionControlHandler>());
}

int InternetSection::GetSectionNameMessageId() const {
  return features::IsInstantHotspotRebrandEnabled()
             ? IDS_SETTINGS_INTERNET
             : IDS_SETTINGS_INTERNET_LEGACY;
}

mojom::Section InternetSection::GetSection() const {
  return mojom::Section::kNetwork;
}

mojom::SearchResultIcon InternetSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kWifi;
}

const char* InternetSection::GetSectionPath() const {
  return mojom::kNetworkSectionPath;
}

bool InternetSection::LogMetric(mojom::Setting setting,
                                base::Value& value) const {
  switch (setting) {
    case mojom::Setting::kWifiHidden:
      base::UmaHistogramBoolean("ChromeOS.Settings.Wifi.Hidden",
                                value.GetBool());
      return true;
    case mojom::Setting::kWifiAddNetwork:
      // An added wifi network an empty GUID means the user manually
      // configured and added a new wifi.
      base::UmaHistogramEnumeration(
          "ChromeOS.Settings.Wifi.AddNetwork",
          value.GetString().empty() ? NetworkDiscoveryState::kNewNetwork
                                    : NetworkDiscoveryState::kExistingNetwork);
      return true;
    default:
      return false;
  }
}

void InternetSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  // Ethernet details.
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_INTERNET_ETHERNET_DETAILS,
                                     mojom::Subpage::kEthernetDetails,
                                     mojom::SearchResultIcon::kEthernet,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kEthernetDetailsSubpagePath);
  RegisterNestedSettingBulk(mojom::Subpage::kEthernetDetails,
                            GetEthernetDetailsSettings(), generator);

  // Wi-Fi networks.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_INTERNET_WIFI_NETWORKS, mojom::Subpage::kWifiNetworks,
      mojom::SearchResultIcon::kWifi, mojom::SearchResultDefaultRank::kMedium,
      mojom::kWifiNetworksSubpagePath);
  static constexpr mojom::Setting kWifiNetworksSettings[] = {
      mojom::Setting::kWifiOnOff,
      mojom::Setting::kWifiAddNetwork,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kWifiNetworks,
                            kWifiNetworksSettings, generator);
  generator->RegisterTopLevelAltSetting(mojom::Setting::kWifiOnOff);

  // Wi-Fi details.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_INTERNET_WIFI_DETAILS, mojom::Subpage::kWifiDetails,
      mojom::Subpage::kWifiNetworks, mojom::SearchResultIcon::kWifi,
      mojom::SearchResultDefaultRank::kMedium, mojom::kWifiDetailsSubpagePath);
  RegisterNestedSettingBulk(mojom::Subpage::kWifiDetails,
                            GetWifiDetailsSettings(), generator);

  // Known networks.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_INTERNET_KNOWN_NETWORKS, mojom::Subpage::kKnownNetworks,
      mojom::Subpage::kWifiNetworks, mojom::SearchResultIcon::kWifi,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kKnownNetworksSubpagePath);
  generator->RegisterNestedAltSetting(mojom::Setting::kPreferWifiNetwork,
                                      mojom::Subpage::kKnownNetworks);
  generator->RegisterNestedAltSetting(mojom::Setting::kForgetWifiNetwork,
                                      mojom::Subpage::kKnownNetworks);
  // Mobile data. Used for both Cellular and Instant Tethering networks.
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_INTERNET_MOBILE_DATA_NETWORKS,
                                     mojom::Subpage::kMobileDataNetworks,
                                     mojom::SearchResultIcon::kCellular,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kMobileDataNetworksSubpagePath);
  static constexpr mojom::Setting kMobileDataNetworksSettings[] = {
      mojom::Setting::kMobileOnOff, mojom::Setting::kInstantTetheringOnOff,
      mojom::Setting::kAddESimNetwork};
  RegisterNestedSettingBulk(mojom::Subpage::kMobileDataNetworks,
                            kMobileDataNetworksSettings, generator);
  generator->RegisterTopLevelAltSetting(mojom::Setting::kMobileOnOff);

  // Passpoint details.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_INTERNET_PASSPOINT_DETAILS,
      mojom::Subpage::kPasspointDetails, mojom::Subpage::kKnownNetworks,
      mojom::SearchResultIcon::kWifi, mojom::SearchResultDefaultRank::kMedium,
      mojom::kPasspointDetailSubpagePath);

  // Cellular details. Cellular details are considered a child of the mobile
  // data subpage. However, note that if Instant Tethering is not available,
  // clicking on "Mobile data" at the Network section navigates users directly
  // to the cellular details page and skips over the mobile data subpage.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_INTERNET_CELLULAR_DETAILS, mojom::Subpage::kCellularDetails,
      mojom::Subpage::kMobileDataNetworks, mojom::SearchResultIcon::kCellular,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kCellularDetailsSubpagePath);
  RegisterNestedSettingBulk(mojom::Subpage::kCellularDetails,
                            GetCellularDetailsSettings(), generator);

  // Hotspot details.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_INTERNET_HOTSPOT_DETAILS, mojom::Subpage::kHotspotDetails,
      mojom::SearchResultIcon::kHotspot,
      mojom::SearchResultDefaultRank::kMedium, mojom::kHotspotSubpagePath);
  RegisterNestedSettingBulk(mojom::Subpage::kHotspotDetails,
                            GetHotspotDetailsSettings(), generator);
  generator->RegisterTopLevelAltSetting(mojom::Setting::kHotspotOnOff);

  // APN.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_INTERNET_NETWORK_ACCESS_POINT, mojom::Subpage::kApn,
      mojom::Subpage::kCellularDetails, mojom::SearchResultIcon::kCellular,
      mojom::SearchResultDefaultRank::kMedium, mojom::kApnSubpagePath);

  // Instant Tethering. Although this is a multi-device feature, its UI resides
  // in the network section.
  generator->RegisterNestedSubpage(
      features::IsInstantHotspotRebrandEnabled()
          ? IDS_SETTINGS_INTERNET_INSTANT_TETHERING_DETAILS
          : IDS_SETTINGS_INTERNET_INSTANT_TETHERING_DETAILS_LEGACY,
      mojom::Subpage::kTetherDetails, mojom::Subpage::kMobileDataNetworks,
      mojom::SearchResultIcon::kInstantTethering,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kTetherDetailsSubpagePath);
  RegisterNestedSettingBulk(mojom::Subpage::kTetherDetails,
                            GetTetherDetailsSettings(), generator);

  // VPN.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_INTERNET_VPN_DETAILS, mojom::Subpage::kVpnDetails,
      mojom::SearchResultIcon::kWifi, mojom::SearchResultDefaultRank::kMedium,
      mojom::kVpnDetailsSubpagePath);
}

std::string InternetSection::ModifySearchResultUrl(
    mojom::SearchResultType type,
    OsSettingsIdentifier id,
    const std::string& url_to_modify) const {
  std::string modified_url =
      OsSettingsSection::ModifySearchResultUrl(type, id, url_to_modify);

  if (IsPartOfDetailsSubpage(type, id, mojom::Subpage::kEthernetDetails)) {
    return GetDetailsSubpageUrl(modified_url, *connected_ethernet_guid_);
  }

  if (IsPartOfDetailsSubpage(type, id, mojom::Subpage::kWifiDetails)) {
    return GetDetailsSubpageUrl(modified_url, *connected_wifi_guid_);
  }

  if (IsPartOfDetailsSubpage(type, id, mojom::Subpage::kCellularDetails)) {
    return GetDetailsSubpageUrl(modified_url, *active_cellular_guid_);
  }

  if (IsPartOfDetailsSubpage(type, id, mojom::Subpage::kApn)) {
    return GetDetailsSubpageUrl(modified_url, *active_cellular_guid_);
  }

  if (IsPartOfDetailsSubpage(type, id, mojom::Subpage::kTetherDetails)) {
    return GetDetailsSubpageUrl(modified_url, *connected_tether_guid_);
  }

  if (IsPartOfDetailsSubpage(type, id, mojom::Subpage::kVpnDetails)) {
    return GetDetailsSubpageUrl(modified_url, *connected_vpn_guid_);
  }

  // Use default implementation.
  return modified_url;
}

void InternetSection::OnDeviceStateListChanged() {
  FetchDeviceList();
}

void InternetSection::OnActiveNetworksChanged(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  FetchNetworkList();
}

void InternetSection::OnHotspotInfoChanged() {
  FetchHotspotInfo();
}

void InternetSection::FetchHotspotInfo() {
  cros_hotspot_config_->GetHotspotInfo(
      base::BindOnce(&InternetSection::OnHotspotInfo, base::Unretained(this)));
}

void InternetSection::OnHotspotInfo(
    hotspot_config::mojom::HotspotInfoPtr hotspot_info) {
  using hotspot_config::mojom::HotspotAllowStatus;
  using hotspot_config::mojom::HotspotState;

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetHotspotSubpageSearchConcepts());
  updater.RemoveSearchTags(GetHotspotOnSearchConcepts());
  updater.RemoveSearchTags(GetHotspotOffSearchConcepts());
  updater.RemoveSearchTags(GetHotspotAutoDisabledSearchConcepts());
  if (hotspot_info->allow_status != HotspotAllowStatus::kAllowed) {
    return;
  }
  if (hotspot_info->config) {
    updater.AddSearchTags(GetHotspotAutoDisabledSearchConcepts());
  }
  updater.AddSearchTags(GetHotspotSubpageSearchConcepts());

  if (hotspot_info->state == HotspotState::kEnabled) {
    updater.AddSearchTags(GetHotspotOnSearchConcepts());
  }
  if (hotspot_info->state == HotspotState::kDisabled) {
    updater.AddSearchTags(GetHotspotOffSearchConcepts());
  }
}

void InternetSection::FetchDeviceList() {
  cros_network_config_->GetGlobalPolicy(
      base::BindOnce(&InternetSection::OnGlobalPolicy, base::Unretained(this)));
}

void InternetSection::OnGlobalPolicy(
    network_config::mojom::GlobalPolicyPtr global_policy) {
  cros_network_config_->GetDeviceStateList(
      base::BindOnce(&InternetSection::OnDeviceList, base::Unretained(this),
                     std::move(global_policy)));
}

void InternetSection::OnDeviceList(
    network_config::mojom::GlobalPolicyPtr global_policy,
    std::vector<network_config::mojom::DeviceStatePropertiesPtr> devices) {
  using network_config::mojom::DeviceStateType;
  using network_config::mojom::NetworkType;

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  updater.RemoveSearchTags(GetWifiSearchConcepts());
  updater.RemoveSearchTags(GetWifiOnSearchConcepts());
  updater.RemoveSearchTags(GetWifiOffSearchConcepts());
  updater.RemoveSearchTags(GetCellularOnSearchConcepts());
  updater.RemoveSearchTags(GetCellularOffSearchConcepts());
  updater.RemoveSearchTags(GetCellularAddESimSearchTerms());
  updater.RemoveSearchTags(GetInstantTetheringSearchConcepts());
  updater.RemoveSearchTags(GetInstantTetheringOnSearchConcepts());
  updater.RemoveSearchTags(GetInstantTetheringOffSearchConcepts());

  // Keep track of ethernet devices to handle an edge case where Ethernet device
  // is present but no network is connected.
  does_ethernet_device_exist_ = false;

  active_cellular_iccid_.reset();

  for (const auto& device : devices) {
    switch (device->type) {
      case NetworkType::kWiFi:
        updater.AddSearchTags(GetWifiSearchConcepts());
        if (device->device_state == DeviceStateType::kEnabled) {
          updater.AddSearchTags(GetWifiOnSearchConcepts());
        } else if (device->device_state == DeviceStateType::kDisabled) {
          updater.AddSearchTags(GetWifiOffSearchConcepts());
        }
        break;

      case NetworkType::kCellular:
        active_cellular_iccid_ = GetCellularActiveSimIccid(device);

        // Note: Cellular search concepts all point to the cellular details
        // page, which is only available if a cellular network exists. This
        // check is in OnNetworkList().
        if (device->device_state == DeviceStateType::kEnabled) {
          updater.AddSearchTags(GetCellularOnSearchConcepts());
          if (AllowAddESim(global_policy)) {
            updater.AddSearchTags(GetCellularAddESimSearchTerms());
          }
        } else if (device->device_state == DeviceStateType::kDisabled) {
          updater.AddSearchTags(GetCellularOffSearchConcepts());
        }
        break;

      case NetworkType::kTether:
        updater.AddSearchTags(GetInstantTetheringSearchConcepts());
        if (device->device_state == DeviceStateType::kEnabled) {
          updater.AddSearchTags(GetInstantTetheringOnSearchConcepts());
        } else if (device->device_state == DeviceStateType::kDisabled) {
          updater.AddSearchTags(GetInstantTetheringOffSearchConcepts());
        }
        break;

      case NetworkType::kEthernet:
        does_ethernet_device_exist_ = true;
        break;

      default:
        // Note: Ethernet and VPN only show search tags when connected, and
        // categories such as Mobile/Wireless do not have search tags.
        break;
    }
  }
}

void InternetSection::FetchNetworkList() {
  cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kVisible,
          network_config::mojom::NetworkType::kAll,
          network_config::mojom::kNoLimit),
      base::BindOnce(&InternetSection::OnNetworkList, base::Unretained(this)));
}

void InternetSection::OnNetworkList(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  using network_config::mojom::NetworkType;

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  updater.RemoveSearchTags(GetEthernetConnectedSearchConcepts());
  updater.RemoveSearchTags(GetEthernetNotConnectedSearchConcepts());
  updater.RemoveSearchTags(GetWifiConnectedSearchConcepts());
  updater.RemoveSearchTags(GetWifiMeteredSearchConcepts());
  updater.RemoveSearchTags(GetWifiHiddenSearchConcepts());
  updater.RemoveSearchTags(GetCellularSearchConcepts());
  updater.RemoveSearchTags(GeActiveCellularNetworkApnSettingsSearchConcepts());
  updater.RemoveSearchTags(GetCellularConnectedSearchConcepts());
  updater.RemoveSearchTags(GetCellularPrimaryIsNonPolicyESimSearchConcepts());
  updater.RemoveSearchTags(GetCellularMeteredSearchConcepts());
  updater.RemoveSearchTags(GetInstantTetheringConnectedSearchConcepts());
  updater.RemoveSearchTags(GetVpnConnectedSearchConcepts());

  active_cellular_guid_.reset();

  connected_ethernet_guid_.reset();
  connected_wifi_guid_.reset();
  connected_tether_guid_.reset();
  connected_vpn_guid_.reset();

  for (const auto& network : networks) {
    // Special case: Some cellular search functionality is available even if the
    // primary cellular network is not connected.
    if (network->type == NetworkType::kCellular) {
      bool is_primary_cellular_network =
          active_cellular_iccid_.has_value() &&
          network->type_state->get_cellular()->iccid == *active_cellular_iccid_;

      if (is_primary_cellular_network) {
        active_cellular_guid_ = network->guid;
        updater.AddSearchTags(GetCellularSearchConcepts());
        updater.AddSearchTags(
            GeActiveCellularNetworkApnSettingsSearchConcepts());

        // If the primary cellular network is ESim and not policy ESim.
        if (!network->type_state->get_cellular()->eid.empty() &&
            !IsPolicySource(network->source)) {
          updater.AddSearchTags(
              GetCellularPrimaryIsNonPolicyESimSearchConcepts());
        }
      }
    }

    if (!IsConnected(network->connection_state)) {
      continue;
    }

    switch (network->type) {
      case NetworkType::kEthernet:
        connected_ethernet_guid_ = network->guid;
        updater.AddSearchTags(GetEthernetConnectedSearchConcepts());
        break;

      case NetworkType::kWiFi:
        connected_wifi_guid_ = network->guid;
        updater.AddSearchTags(GetWifiConnectedSearchConcepts());
        if (base::FeatureList::IsEnabled(::features::kMeteredShowToggle)) {
          updater.AddSearchTags(GetWifiMeteredSearchConcepts());
        }
        if (base::FeatureList::IsEnabled(
                ::features::kShowHiddenNetworkToggle)) {
          updater.AddSearchTags(GetWifiHiddenSearchConcepts());
        }
        break;

      case NetworkType::kCellular:
        updater.AddSearchTags(GetCellularConnectedSearchConcepts());
        if (base::FeatureList::IsEnabled(::features::kMeteredShowToggle)) {
          updater.AddSearchTags(GetCellularMeteredSearchConcepts());
        }
        break;

      case NetworkType::kTether:
        connected_tether_guid_ = network->guid;
        updater.AddSearchTags(GetInstantTetheringConnectedSearchConcepts());
        break;

      case NetworkType::kVPN:
        connected_vpn_guid_ = network->guid;
        updater.AddSearchTags(GetVpnConnectedSearchConcepts());
        break;

      default:
        // Note: Category types such as Mobile/Wireless do not have search tags.
        break;
    }
  }

  // Edge case where Ethernet device is present but no network is connected,
  // i.e. on Chromeboxes. http://crbug.com/1096768
  if (does_ethernet_device_exist_ && !connected_ethernet_guid_.has_value()) {
    updater.AddSearchTags(GetEthernetNotConnectedSearchConcepts());
  }
}

}  // namespace ash::settings
