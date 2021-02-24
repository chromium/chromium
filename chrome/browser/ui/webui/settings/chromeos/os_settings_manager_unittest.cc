// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager.h"

#include "base/metrics/histogram_base.h"
#include "base/no_destructor.h"
#include "base/test/metrics/histogram_enum_reader.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/android_sms/android_sms_service_factory.h"
#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager_factory.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/constants_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/hierarchy.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager_factory.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_sections.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/components/local_search_service/public/cpp/local_search_service_proxy_factory.h"
#include "chromeos/components/local_search_service/search_metrics_reporter.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

// Verifies the OsSettingsManager initialization flow. Behavioral functionality
// is tested via unit tests on the sub-elements owned by OsSettingsManager.
class OsSettingsManagerTest : public testing::Test {
 protected:
  OsSettingsManagerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~OsSettingsManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    TestingProfile* profile =
        profile_manager_.CreateTestingProfile("TestingProfile");

    local_search_service::SearchMetricsReporter::RegisterLocalStatePrefs(
        pref_service_.registry());
    local_search_service::LocalSearchServiceProxyFactory::GetInstance()
        ->SetLocalState(&pref_service_);
    KerberosCredentialsManager* kerberos_credentials_manager =
        ProfileHelper::IsPrimaryProfile(profile)
            ? KerberosCredentialsManagerFactory::Get(profile)
            : nullptr;
    manager_ = std::make_unique<OsSettingsManager>(
        profile, local_search_service_proxy_.get(),
        multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(
            profile),
        phonehub::PhoneHubManagerFactory::GetForProfile(profile),
        ProfileSyncServiceFactory::GetForProfile(profile),
        SupervisedUserServiceFactory::GetForProfile(profile),
        kerberos_credentials_manager,
        ArcAppListPrefsFactory::GetForBrowserContext(profile),
        IdentityManagerFactory::GetForProfile(profile),
        android_sms::AndroidSmsServiceFactory::GetForBrowserContext(profile),
        CupsPrintersManagerFactory::GetForBrowserContext(profile),
        apps::AppServiceProxyFactory::GetForProfile(profile));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_ =
          std::make_unique<local_search_service::LocalSearchServiceProxy>(
              /*for_testing=*/true);
  std::unique_ptr<OsSettingsManager> manager_;
};

TEST_F(OsSettingsManagerTest, Initialization) {
  base::Optional<base::HistogramEnumEntryMap> sections_enum_entry_map =
      base::ReadEnumFromEnumsXml("OsSettingsSection");
  ASSERT_TRUE(sections_enum_entry_map);
  for (const auto& section : constants::AllSections()) {
    // For each mojom::Section value, there should be an associated
    // OsSettingsSection class registered.
    EXPECT_TRUE(manager_->sections_->GetSection(section))
        << "No OsSettingsSection instance created for " << section << ".";

    // Each mojom::Section should be registered in the hierarchy.
    const Hierarchy::SectionMetadata& metadata =
        manager_->hierarchy_->GetSectionMetadata(section);

    // Only "About Chrome OS" and "Kerberos" sections contain only a link to a
    // subpage.
    EXPECT_EQ(metadata.only_contains_link_to_subpage,
              section == mojom::Section::kAboutChromeOs ||
                  section == mojom::Section::kKerberos);

    EXPECT_TRUE(
        base::Contains(*sections_enum_entry_map, static_cast<int32_t>(section)))
        << "Missing OsSettingsSection enums.xml entry for " << section;
  }

  base::Optional<base::HistogramEnumEntryMap> subpages_enum_entry_map =
      base::ReadEnumFromEnumsXml("OsSettingsSubpage");
  ASSERT_TRUE(subpages_enum_entry_map);
  for (const auto& subpage : constants::AllSubpages()) {
    // Each mojom::Subpage should be registered in the hierarchy. Note that
    // GetSubpageMetadata() internally CHECK()s that the metadata exists before
    // returning it.
    manager_->hierarchy_->GetSubpageMetadata(subpage);

    EXPECT_TRUE(
        base::Contains(*subpages_enum_entry_map, static_cast<int32_t>(subpage)))
        << "Missing OsSettingsSubpage enums.xml entry for " << subpage;
  }

  base::Optional<base::HistogramEnumEntryMap> settings_enum_entry_map =
      base::ReadEnumFromEnumsXml("OsSetting");
  ASSERT_TRUE(settings_enum_entry_map);
  for (const auto& setting : constants::AllSettings()) {
    // Each mojom::Setting should be registered in the hierarchy. Note that
    // GetSettingMetadata() internally CHECK()s that the metadata exists before
    // returning it.
    manager_->hierarchy_->GetSettingMetadata(setting);

    EXPECT_TRUE(
        base::Contains(*settings_enum_entry_map, static_cast<int32_t>(setting)))
        << "Missing OsSetting enums.xml entry for " << setting;
  }
}

}  // namespace settings
}  // namespace chromeos
