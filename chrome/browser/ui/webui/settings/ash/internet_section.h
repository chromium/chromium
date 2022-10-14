// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_INTERNET_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_INTERNET_SECTION_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_section.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

class InternetSection : public OsSettingsSection,
                        public network_config::CrosNetworkConfigObserver {
 public:
  InternetSection(Profile* profile, SearchTagRegistry* search_tag_registry);
  ~InternetSection() override;

 private:
  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  void AddHandlers(content::WebUI* web_ui) override;
  int GetSectionNameMessageId() const override;
  chromeos::settings::mojom::Section GetSection() const override;
  mojom::SearchResultIcon GetSectionIcon() const override;
  std::string GetSectionPath() const override;
  bool LogMetric(chromeos::settings::mojom::Setting setting,
                 base::Value& value) const override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;
  std::string ModifySearchResultUrl(
      mojom::SearchResultType type,
      OsSettingsIdentifier id,
      const std::string& url_to_modify) const override;

  // network_config::CrosNetworkConfigObserver:
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks) override;
  void OnDeviceStateListChanged() override;

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

  // Null if no active cellular network exists. The active cellular network
  // corresponds to the currently active SIM slot, and may not be
  // currently connected. A connected cellular network will always be the
  // active cellular network.
  absl::optional<std::string> active_cellular_iccid_;
  absl::optional<std::string> active_cellular_guid_;

  // Note: If not connected, the below fields are null.
  absl::optional<std::string> connected_ethernet_guid_;
  absl::optional<std::string> connected_wifi_guid_;
  absl::optional<std::string> connected_tether_guid_;
  absl::optional<std::string> connected_vpn_guid_;

  bool does_ethernet_device_exist_ = false;

  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      receiver_{this};
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_INTERNET_SECTION_H_
