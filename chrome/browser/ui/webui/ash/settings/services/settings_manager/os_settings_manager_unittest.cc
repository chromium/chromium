// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/services/settings_manager/os_settings_manager.h"

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/constants/ash_features.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_enum_reader.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/ash/settings/constants/constants_util.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_sections.h"
#include "chrome/browser/ui/webui/ash/settings/search/hierarchy.h"
#include "chrome/browser/ui/webui/ash/settings/services/settings_manager/os_settings_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy_factory.h"
#include "chromeos/ash/components/local_search_service/search_metrics_reporter.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
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
      : fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~OsSettingsManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kInputDeviceSettingsSplit,
         ash::features::kOsSettingsRevampWayfinding,
         ash::features::kPeripheralCustomization, arc::kPerAppLanguage},
        {});
    ASSERT_TRUE(profile_manager_.SetUp());
    Profile* profile = profile_manager_.CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName);
    // Log in user to ensure ARC PlayStore can be enabled.
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId(profile->GetProfileUserName(), "1234"));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    // Enables ARC for test profile.
    arc::SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    arc::SetArcPlayStoreEnabledForProfile(profile, true);

    NearbySharingServiceFactory::
        SetIsNearbyShareSupportedForBrowserContextForTesting(false);
    local_search_service::SearchMetricsReporter::RegisterLocalStatePrefs(
        pref_service_.registry());
    local_search_service::LocalSearchServiceProxyFactory::GetInstance()
        ->SetLocalState(&pref_service_);
    input_method::MockInputMethodManager::Initialize(
        new input_method::MockInputMethodManager);
    statistics_provider_.SetMachineStatistic(ash::system::kRegionKey, "us");

    UserDataAuthClient::InitializeFake();

    manager_ = std::make_unique<OsSettingsManager>(
        profile, local_search_service_proxy_.get(),
        multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(
            profile),
        phonehub::PhoneHubManagerFactory::GetForProfile(profile),
        KerberosCredentialsManagerFactory::Get(profile),
        ArcAppListPrefsFactory::GetForBrowserContext(profile),
        IdentityManagerFactory::GetForProfile(profile),
        CupsPrintersManagerFactory::GetForBrowserContext(profile),
        apps::AppServiceProxyFactory::GetForProfile(profile),
        eche_app::EcheAppManagerFactory::GetForProfile(profile));
  }

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_ =
          std::make_unique<local_search_service::LocalSearchServiceProxy>(
              /*for_testing=*/true);
  std::unique_ptr<OsSettingsManager> manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
};

TEST_F(OsSettingsManagerTest, Initialization) {
  std::optional<base::HistogramEnumEntryMap> sections_enum_entry_map =
      base::ReadEnumFromEnumsXml("OsSettingsSection", "chromeos_settings");
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

  std::optional<base::HistogramEnumEntryMap> subpages_enum_entry_map =
      base::ReadEnumFromEnumsXml("OsSettingsSubpage", "chromeos_settings");
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

  std::optional<base::HistogramEnumEntryMap> settings_enum_entry_map =
      base::ReadEnumFromEnumsXml("OsSetting", "chromeos_settings");
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
