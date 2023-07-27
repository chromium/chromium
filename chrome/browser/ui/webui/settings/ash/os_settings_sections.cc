// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/os_settings_sections.h"

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/ash/about_section.h"
#include "chrome/browser/ui/webui/settings/ash/accessibility_section.h"
#include "chrome/browser/ui/webui/settings/ash/apps_section.h"
#include "chrome/browser/ui/webui/settings/ash/bluetooth_section.h"
#include "chrome/browser/ui/webui/settings/ash/crostini_section.h"
#include "chrome/browser/ui/webui/settings/ash/date_time_section.h"
#include "chrome/browser/ui/webui/settings/ash/device_section.h"
#include "chrome/browser/ui/webui/settings/ash/files_section.h"
#include "chrome/browser/ui/webui/settings/ash/internet_section.h"
#include "chrome/browser/ui/webui/settings/ash/kerberos_section.h"
#include "chrome/browser/ui/webui/settings/ash/languages_section.h"
#include "chrome/browser/ui/webui/settings/ash/main_section.h"
#include "chrome/browser/ui/webui/settings/ash/multidevice_section.h"
#include "chrome/browser/ui/webui/settings/ash/people_section.h"
#include "chrome/browser/ui/webui/settings/ash/personalization_section.h"
#include "chrome/browser/ui/webui/settings/ash/printing_section.h"
#include "chrome/browser/ui/webui/settings/ash/privacy_section.h"
#include "chrome/browser/ui/webui/settings/ash/reset_section.h"
#include "chrome/browser/ui/webui/settings/ash/search_section.h"
#include "chrome/browser/ui/webui/settings/ash/system_preferences_section.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::Section;
}

OsSettingsSections::OsSettingsSections(
    Profile* profile,
    SearchTagRegistry* search_tag_registry,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    phonehub::PhoneHubManager* phone_hub_manager,
    KerberosCredentialsManager* kerberos_credentials_manager,
    ArcAppListPrefs* arc_app_list_prefs,
    signin::IdentityManager* identity_manager,
    android_sms::AndroidSmsService* android_sms_service,
    CupsPrintersManager* printers_manager,
    apps::AppServiceProxy* app_service_proxy,
    eche_app::EcheAppManager* eche_app_manager) {
  auto* prefs = profile->GetPrefs();

  // Special case: Main section does not have an associated enum value.
  sections_.push_back(
      std::make_unique<MainSection>(profile, search_tag_registry));

  AddSection(mojom::Section::kNetwork,
             std::make_unique<InternetSection>(profile, search_tag_registry));

  AddSection(
      mojom::Section::kBluetooth,
      std::make_unique<BluetoothSection>(profile, search_tag_registry, prefs));

  AddSection(
      mojom::Section::kMultiDevice,
      std::make_unique<MultiDeviceSection>(
          profile, search_tag_registry, multidevice_setup_client,
          phone_hub_manager, android_sms_service, prefs, eche_app_manager));

  AddSection(mojom::Section::kPeople,
             std::make_unique<PeopleSection>(profile, search_tag_registry,
                                             identity_manager, prefs));

  AddSection(mojom::Section::kDevice, std::make_unique<DeviceSection>(
                                          profile, search_tag_registry, prefs));

  AddSection(mojom::Section::kPersonalization,
             std::make_unique<PersonalizationSection>(
                 profile, search_tag_registry, prefs));

  AddSection(mojom::Section::kSearchAndAssistant,
             std::make_unique<SearchSection>(profile, search_tag_registry));

  AddSection(mojom::Section::kApps, std::make_unique<AppsSection>(
                                        profile, search_tag_registry, prefs,
                                        arc_app_list_prefs, app_service_proxy));

  AddSection(
      mojom::Section::kCrostini,
      std::make_unique<CrostiniSection>(profile, search_tag_registry, prefs));

  AddSection(mojom::Section::kDateAndTime,
             std::make_unique<DateTimeSection>(profile, search_tag_registry));

  AddSection(
      mojom::Section::kPrivacyAndSecurity,
      std::make_unique<PrivacySection>(profile, search_tag_registry, prefs));

  AddSection(
      mojom::Section::kLanguagesAndInput,
      std::make_unique<LanguagesSection>(profile, search_tag_registry, prefs));

  AddSection(mojom::Section::kFiles,
             std::make_unique<FilesSection>(profile, search_tag_registry));

  AddSection(mojom::Section::kPrinting,
             std::make_unique<PrintingSection>(profile, search_tag_registry,
                                               printers_manager));

  AddSection(mojom::Section::kAccessibility,
             std::make_unique<AccessibilitySection>(
                 profile, search_tag_registry, prefs));

  AddSection(mojom::Section::kReset,
             std::make_unique<ResetSection>(profile, search_tag_registry));

  AddSection(mojom::Section::kAboutChromeOs,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
             std::make_unique<AboutSection>(profile, search_tag_registry, prefs)
#else
             std::make_unique<AboutSection>(profile, search_tag_registry)
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  );

  AddSection(mojom::Section::kKerberos,
             std::make_unique<KerberosSection>(profile, search_tag_registry,
                                               kerberos_credentials_manager));

  if (ash::features::IsOsSettingsRevampWayfindingEnabled()) {
    AddSection(mojom::Section::kSystemPreferences,
               std::make_unique<SystemPreferencesSection>(profile,
                                                          search_tag_registry));
  }
}

OsSettingsSections::OsSettingsSections() = default;

OsSettingsSections::~OsSettingsSections() = default;

const OsSettingsSection* OsSettingsSections::GetSection(
    mojom::Section section) const {
  const auto it = sections_map_.find(section);
  CHECK(it != sections_map_.end());
  return it->second;
}

void OsSettingsSections::AddSection(
    chromeos::settings::mojom::Section section_id,
    std::unique_ptr<OsSettingsSection> section) {
  DCHECK(!base::Contains(sections_map_, section_id));

  sections_map_[section_id] = section.get();
  sections_.push_back(std::move(section));
}

}  // namespace ash::settings
