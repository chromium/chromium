// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_SECTION_H_

#include "base/values.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {

namespace android_sms {
class AndroidSmsService;
}  // namespace android_sms

namespace phonehub {
class PhoneHubManager;
}  // namespace phonehub

namespace settings {

class SearchTagRegistry;

// Provides UI strings and search tags for MultiDevice settings. Different
// search tags are registered depending on whether MultiDevice features are
// allowed and whether the user has opted into the suite of features.
class MultiDeviceSection
    : public OsSettingsSection,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  MultiDeviceSection(
      Profile* profile,
      SearchTagRegistry* search_tag_registry,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      phonehub::PhoneHubManager* phone_hub_manager,
      android_sms::AndroidSmsService* android_sms_service,
      PrefService* pref_service);
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

  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  phonehub::PhoneHubManager* phone_hub_manager_;
  android_sms::AndroidSmsService* android_sms_service_;
  PrefService* pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_SECTION_H_
