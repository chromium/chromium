// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/os_settings_manager.h"

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_enum_reader.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/android_sms/android_sms_service_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager_factory.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/settings/ash/constants/constants_util.h"
#include "chrome/browser/ui/webui/settings/ash/hierarchy.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_manager_factory.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_sections.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy_factory.h"
#include "chromeos/ash/components/local_search_service/search_metrics_reporter.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

// Verifies the OsSettingsManager initialization flow. Behavioral functionality
// is tested via unit tests on the sub-elements owned by OsSettingsManager.
class OsSettingsManagerTest : public testing::Test {
 protected:
  OsSettingsManagerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~OsSettingsManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {::features::kAccessibilityChromeVoxPageMigration,
         ash::features::kInputDeviceSettingsSplit,
         ash::features::kPeripheralCustomization},
        {});
    ASSERT_TRUE(profile_manager_.SetUp());
    TestingProfile* profile =
        profile_manager_.CreateTestingProfile("TestingProfile");

    local_search_service::SearchMetricsReporter::RegisterLocalStatePrefs(
        pref_service_.registry());
    local_search_service::LocalSearchServiceProxyFactory::GetInstance()
        ->SetLocalState(&pref_service_);
    input_method::MockInputMethodManager::Initialize(
        new input_method::MockInputMethodManager);

    UserDataAuthClient::InitializeFake();

    manager_ = std::make_unique<OsSettingsManager>(
        profile, local_search_service_proxy_.get(),
        multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(
            profile),
        phonehub::PhoneHubManagerFactory::GetForProfile(profile),
        KerberosCredentialsManagerFactory::Get(profile),
        ArcAppListPrefsFactory::GetForBrowserContext(profile),
        IdentityManagerFactory::GetForProfile(profile),
        android_sms::AndroidSmsServiceFactory::GetForBrowserContext(profile),
        CupsPrintersManagerFactory::GetForBrowserContext(profile),
        apps::AppServiceProxyFactory::GetForProfile(profile),
        eche_app::EcheAppManagerFactory::GetForProfile(profile));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_ =
          std::make_unique<local_search_service::LocalSearchServiceProxy>(
              /*for_testing=*/true);
  std::unique_ptr<OsSettingsManager> manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OsSettingsManagerTest, Initialization) {
  absl::optional<base::HistogramEnumEntryMap> sections_enum_entry_map =
      base::ReadEnumFromEnumsXml("OsSettingsSection");
  ASSERT_TRUE(sections_enum_entry_map);
  for (const auto& section : AllSections()) {
    // For each mojom::Section value, there should be an associated
    // OsSettingsSection class registered.
    EXPECT_TRUE(manager_->sections_->GetSection(section))
        << "No OsSettingsSection instance created for " << section << ".";

    // Each mojom::Section should be registered in the hierarchy.
    manager_->hierarchy_->GetSectionMetadata(section);

    EXPECT_TRUE(
        base::Contains(*sections_enum_entry_map, static_cast<int32_t>(section)))
        << "Missing OsSettingsSection enums.xml entry for " << section;
  }

  absl::optional<base::HistogramEnumEntryMap> subpages_enum_entry_map =
      base::ReadEnumFromEnumsXml("OsSettingsSubpage");
  ASSERT_TRUE(subpages_enum_entry_map);
  for (const auto& subpage : AllSubpages()) {
    // Each mojom::Subpage should be registered in the hierarchy. Note that
    // GetSubpageMetadata() internally CHECK()s that the metadata exists before
    // returning it.
    manager_->hierarchy_->GetSubpageMetadata(subpage);

    EXPECT_TRUE(
        base::Contains(*subpages_enum_entry_map, static_cast<int32_t>(subpage)))
        << "Missing OsSettingsSubpage enums.xml entry for " << subpage;
  }

  absl::optional<base::HistogramEnumEntryMap> settings_enum_entry_map =
      base::ReadEnumFromEnumsXml("OsSetting");
  ASSERT_TRUE(settings_enum_entry_map);
  for (const auto& setting : AllSettings()) {
    // Each mojom::Setting should be registered in the hierarchy. Note that
    // GetSettingMetadata() internally CHECK()s that the metadata exists before
    // returning it.
    manager_->hierarchy_->GetSettingMetadata(setting);

    EXPECT_TRUE(
        base::Contains(*settings_enum_entry_map, static_cast<int32_t>(setting)))
        << "Missing OsSetting enums.xml entry for " << setting;
  }
}

}  // namespace ash::settings
