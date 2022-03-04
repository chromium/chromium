// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_SECTION_H_

// TODO(https://crbug.com/1164001): move to forward declaration.
#include "ash/components/phonehub/phone_hub_manager.h"
#include "ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "ash/webui/eche_app_ui/eche_app_manager.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ash/android_sms/android_sms_service.h"
#include "chrome/browser/ui/webui/nearby_share/public/mojom/nearby_share_settings.mojom.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
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
      android_sms::AndroidSmsService* android_sms_service,
      PrefService* pref_service,
      ash::eche_app::EcheAppManager* eche_app_manager);
  ~MultiDeviceSection() override;

 private:
  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  void AddHandlers(content::WebUI* web_ui) override;
  int GetSectionNameMessageId() const override;
  mojom::Section GetSection() const override;
  mojom::SearchResultIcon GetSectionIcon() const override;
  std::string GetSectionPath() const override;
  bool LogMetric(mojom::Setting setting, base::Value& value) const override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_status_with_device) override;
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  // Nearby Share enabled pref change observer.
  void OnNearbySharingEnabledChanged();

  bool IsFeatureSupported(ash::multidevice_setup::mojom::Feature feature);
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

  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  phonehub::PhoneHubManager* phone_hub_manager_;
  android_sms::AndroidSmsService* android_sms_service_;
  PrefService* pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  ash::eche_app::EcheAppManager* eche_app_manager_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_SECTION_H_
