// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_MULTIDEVICE_MULTIDEVICE_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_MULTIDEVICE_MULTIDEVICE_SECTION_H_

#include "ash/webui/eche_app_ui/eche_app_manager.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash {

namespace phonehub {
class PhoneHubManager;
}

namespace settings {

class SearchTagRegistry;

// Provides UI strings and search tags for MultiDevice settings. Different
// search tags are registered depending on whether MultiDevice features are
// allowed and whether the user has opted into the suite of features.
class MultiDeviceSection
    : public OsSettingsSection,
      public multidevice_setup::MultiDeviceSetupClient::Observer,
      public nearby_share::mojom::NearbyShareSettingsObserver {
 public:
  MultiDeviceSection(
      Profile* profile,
      SearchTagRegistry* search_tag_registry,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      phonehub::PhoneHubManager* phone_hub_manager,
      PrefService* pref_service,
      eche_app::EcheAppManager* eche_app_manager);
  ~MultiDeviceSection() override;

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

 private:
  friend class MultiDeviceSectionTest;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_status_with_device) override;
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  // Screen lock enabled pref change observer.
  void OnEnableScreenLockChanged();

  // Phone screen lock status pref change observer.
  void OnScreenLockStatusChanged();

  bool IsFeatureSupported(multidevice_setup::mojom::Feature feature);
  void RefreshNearbyBackgroundScanningShareSearchConcepts();

  // nearby_share::mojom::NearbyShareSettingsObserver:
  void OnEnabledChanged(bool enabled) override;
  void OnFastInitiationNotificationStateChanged(
      nearby_share::mojom::FastInitiationNotificationState state) override;
  void OnIsFastInitiationHardwareSupportedChanged(bool is_supported) override;
  void OnDeviceNameChanged(const std::string& device_name) override {}
  void OnDataUsageChanged(nearby_share::mojom::DataUsage data_usage) override {}
  void OnVisibilityChanged(
      nearby_share::mojom::Visibility visibility) override {}
  void OnAllowedContactsChanged(
      const std::vector<std::string>& allowed_contacts) override {}
  void OnIsOnboardingCompleteChanged(bool is_complete) override {}

  mojo::Receiver<nearby_share::mojom::NearbyShareSettingsObserver>
      settings_receiver_{this};

  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;
  raw_ptr<phonehub::PhoneHubManager> phone_hub_manager_;
  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<eche_app::EcheAppManager> eche_app_manager_;
  raw_ptr<content::WebUIDataSource, DanglingUntriaged> html_source_;
};

}  // namespace settings
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_MULTIDEVICE_MULTIDEVICE_SECTION_H_
