// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_INTERNET_INTERNET_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_INTERNET_INTERNET_SECTION_H_

#include <optional>
#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"
#include "chromeos/ash/services/hotspot_config/public/cpp/cros_hotspot_config_observer.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

class InternetSection
    : public OsSettingsSection,
      public chromeos::network_config::CrosNetworkConfigObserver,
      public hotspot_config::CrosHotspotConfigObserver {
 public:
  InternetSection(Profile* profile, SearchTagRegistry* search_tag_registry);
  ~InternetSection() override;

  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  void AddHandlers(content::WebUI* web_ui) override;
  int GetSectionNameMessageId() const override;
  chromeos::settings::mojom::Section GetSection() const override;
  mojom::SearchResultIcon GetSectionIcon() const override;
  const char* GetSectionPath() const override;
  bool LogMetric(chromeos::settings::mojom::Setting setting,
                 base::Value& value) const override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;
  std::string ModifySearchResultUrl(
      mojom::SearchResultType type,
      OsSettingsIdentifier id,
      const std::string& url_to_modify) const override;

 private:
  // network_config::CrosNetworkConfigObserver:
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks) override;
  void OnDeviceStateListChanged() override;

  // hotspot_config::CrosHotspotConfigObserver:
  void OnHotspotInfoChanged() override;

  void FetchDeviceList();
  void OnGlobalPolicy(
      chromeos::network_config::mojom::GlobalPolicyPtr global_policy);
  void OnDeviceList(
      chromeos::network_config::mojom::GlobalPolicyPtr global_policy,
      std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
          devices);

  void FetchNetworkList();
  void OnNetworkList(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  void FetchHotspotInfo();
  void OnHotspotInfo(hotspot_config::mojom::HotspotInfoPtr hotspot_info);

  // Null if no active cellular network exists. The active cellular network
  // corresponds to the currently active SIM slot, and may not be
  // currently connected. A connected cellular network will always be the
  // active cellular network.
  std::optional<std::string> active_cellular_iccid_;
  std::optional<std::string> active_cellular_guid_;

  // Note: If not connected, the below fields are null.
  std::optional<std::string> connected_ethernet_guid_;
  std::optional<std::string> connected_wifi_guid_;
  std::optional<std::string> connected_tether_guid_;
  std::optional<std::string> connected_vpn_guid_;

  bool does_ethernet_device_exist_ = false;

  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      network_config_receiver_{this};
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_;
  mojo::Receiver<hotspot_config::mojom::CrosHotspotConfigObserver>
      hotspot_config_receiver_{this};
  mojo::Remote<hotspot_config::mojom::CrosHotspotConfig> cros_hotspot_config_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_INTERNET_INTERNET_SECTION_H_
