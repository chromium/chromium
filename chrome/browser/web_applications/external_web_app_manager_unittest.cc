// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/external_web_app_manager.h"

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

namespace web_app {

namespace {

constexpr char kUserTypesTestDir[] = "user_types";

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kGoodJsonTestDir[] = "good_json";
constexpr char kAppAllUrl[] = "https://www.google.com/all";
constexpr char kAppChildUrl[] = "https://www.google.com/child";
constexpr char kAppGuestUrl[] = "https://www.google.com/guest";
constexpr char kAppManagedUrl[] = "https://www.google.com/managed";
constexpr char kAppUnmanagedUrl[] = "https://www.google.com/unmanaged";
#endif

}  // namespace

class ExternalWebAppManagerTest : public testing::Test {
 public:
  ExternalWebAppManagerTest() = default;
  ExternalWebAppManagerTest(const ExternalWebAppManagerTest&) = delete;
  ExternalWebAppManagerTest& operator=(const ExternalWebAppManagerTest&) =
      delete;
  ~ExternalWebAppManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<ash::FakeChromeUserManager>());
#endif
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager_enabler_.reset();
#endif
    testing::Test::TearDown();
  }

 protected:
  std::vector<ExternalInstallOptions> LoadApps(const char* test_dir,
                                               Profile* profile = nullptr) {
    std::unique_ptr<TestingProfile> testing_profile;
    if (!profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      testing_profile = CreateProfileAndLogin();
      profile = testing_profile.get();
#else
      NOTREACHED();
#endif
    }

    // Uses the chrome/test/data/web_app_default_apps/test_dir directory that
    // holds the *.json data files from which tests should parse as app configs.
    base::FilePath config_dir;
    if (!base::PathService::Get(chrome::DIR_TEST_DATA, &config_dir)) {
      ADD_FAILURE()
          << "base::PathService::Get could not resolve chrome::DIR_TEST_DATA";
    }
    config_dir =
        config_dir.AppendASCII("web_app_default_apps").AppendASCII(test_dir);
    ExternalWebAppManager::SetConfigDirForTesting(&config_dir);

    auto external_web_app_manager =
        std::make_unique<ExternalWebAppManager>(profile);

    std::vector<ExternalInstallOptions> result;
    base::RunLoop run_loop;
    external_web_app_manager->LoadForTesting(base::BindLambdaForTesting(
        [&](std::vector<ExternalInstallOptions> install_options_list) {
          result = install_options_list;
          run_loop.Quit();
        }));
    run_loop.Run();

    ExternalWebAppManager::SetConfigDirForTesting(nullptr);

    return result;
  }

  // Helper that creates simple test profile.
  std::unique_ptr<TestingProfile> CreateProfile() {
    TestingProfile::Builder profile_builder;
    return profile_builder.Build();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Helper that creates simple test guest profile.
  std::unique_ptr<TestingProfile> CreateGuestProfile() {
    TestingProfile::Builder profile_builder;
    profile_builder.SetGuestSession();
    return profile_builder.Build();
  }

  // Helper that creates simple test profile and logs it into user manager.
  // This makes profile appears as a primary profile in ChromeOS.
  std::unique_ptr<TestingProfile> CreateProfileAndLogin() {
    std::unique_ptr<TestingProfile> profile = CreateProfile();
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile->GetProfileUserName(), "1234567890"));
    user_manager()->AddUser(account_id);
    user_manager()->LoginUser(account_id);
    return profile;
  }

  // Helper that creates simple test guest profile and logs it into user
  // manager. This makes profile appears as a primary profile in ChromeOS.
  std::unique_ptr<TestingProfile> CreateGuestProfileAndLogin() {
    std::unique_ptr<TestingProfile> profile = CreateGuestProfile();
    user_manager()->AddGuestUser();
    user_manager()->LoginUser(user_manager()->GetGuestAccountId());
    return profile;
  }

  void VerifySetOfApps(Profile* profile, const std::set<GURL>& expectations) {
    const auto install_options_list = LoadApps(kUserTypesTestDir, profile);
    ASSERT_EQ(expectations.size(), install_options_list.size());
    for (const auto& install_options : install_options_list)
      ASSERT_EQ(1u, expectations.count(install_options.install_url));
  }
#endif

  void ExpectHistograms(int enabled, int disabled, int errors) {
    histograms_.ExpectUniqueSample(
        ExternalWebAppManager::kHistogramEnabledCount, enabled, 1);
    histograms_.ExpectUniqueSample(
        ExternalWebAppManager::kHistogramDisabledCount, disabled, 1);
    histograms_.ExpectUniqueSample(
        ExternalWebAppManager::kHistogramConfigErrorCount, errors, 1);
  }

  base::HistogramTester histograms_;

  ScopedTestingPreinstalledAppData preinstalled_web_app_override_;

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::FakeChromeUserManager* user_manager() {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  // To support primary/non-primary users.
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
#endif

  // To support context of browser threads.
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ExternalWebAppManagerTest, ReplacementExtensionBlockedByPolicy) {
  using PolicyUpdater = extensions::ExtensionManagementPrefUpdater<
      sync_preferences::TestingPrefServiceSyncable>;
  auto test_profile = std::make_unique<TestingProfile>();
  sync_preferences::TestingPrefServiceSyncable* prefs =
      test_profile->GetTestingPrefService();

  GURL install_url("https://test.app");
  constexpr char kExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
  ExternalInstallOptions options(install_url, DisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalDefault);
  options.user_type_allowlist = {"unmanaged"};
  options.uninstall_and_replace = {kExtensionId};
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating(
      []() { return std::make_unique<WebApplicationInfo>(); });
  preinstalled_web_app_override_.apps.push_back(std::move(options));

  auto expect_present = [&]() {
    std::vector<ExternalInstallOptions> options_list =
        LoadApps(/*test_dir=*/"", test_profile.get());
    ASSERT_EQ(options_list.size(), 1u);
    EXPECT_EQ(options_list[0].install_url, install_url);
  };

  auto expect_not_present = [&]() {
    std::vector<ExternalInstallOptions> options_list =
        LoadApps(/*test_dir=*/"", test_profile.get());
    ASSERT_EQ(options_list.size(), 0u);
  };

  expect_present();

  PolicyUpdater(prefs).SetBlocklistedByDefault(false);
  expect_present();

  PolicyUpdater(prefs).SetBlocklistedByDefault(true);
  expect_not_present();

  PolicyUpdater(prefs).SetIndividualExtensionInstallationAllowed(kExtensionId,
                                                                 true);
  expect_present();

  PolicyUpdater(prefs).SetBlocklistedByDefault(false);
  PolicyUpdater(prefs).SetIndividualExtensionInstallationAllowed(kExtensionId,
                                                                 false);
  expect_not_present();
}

// Only Chrome OS parses config files.
#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ExternalWebAppManagerTest, GoodJson) {
  const auto install_options_list = LoadApps(kGoodJsonTestDir);

  // The good_json directory contains two good JSON files:
  // chrome_platform_status.json and google_io_2016.json.
  // google_io_2016.json is missing a "create_shortcuts" field, so the default
  // value of false should be used.
  std::vector<ExternalInstallOptions> test_install_options_list;
  {
    ExternalInstallOptions install_options(
        GURL("https://www.chromestatus.com/features"), DisplayMode::kBrowser,
        ExternalInstallSource::kExternalDefault);
    install_options.user_type_allowlist = {"unmanaged"};
    install_options.add_to_applications_menu = true;
    install_options.add_to_search = true;
    install_options.add_to_management = true;
    install_options.add_to_desktop = true;
    install_options.add_to_quick_launch_bar = true;
    install_options.require_manifest = true;
    test_install_options_list.push_back(std::move(install_options));
  }
  {
    ExternalInstallOptions install_options(
        GURL("https://events.google.com/io2016/?utm_source=web_app_manifest"),
        DisplayMode::kStandalone, ExternalInstallSource::kExternalDefault);
    install_options.user_type_allowlist = {"unmanaged"};
    install_options.add_to_applications_menu = true;
    install_options.add_to_search = true;
    install_options.add_to_management = true;
    install_options.add_to_desktop = false;
    install_options.add_to_quick_launch_bar = false;
    install_options.require_manifest = true;
    install_options.uninstall_and_replace.push_back("migrationsourceappid");
    test_install_options_list.push_back(std::move(install_options));
  }

  EXPECT_EQ(test_install_options_list.size(), install_options_list.size());
  for (const auto& install_option : test_install_options_list) {
    EXPECT_TRUE(base::Contains(install_options_list, install_option));
  }
  ExpectHistograms(/*enabled=*/2, /*disabled=*/0, /*errors=*/0);
}

TEST_F(ExternalWebAppManagerTest, BadJson) {
  const auto app_infos = LoadApps("bad_json");

  // The bad_json directory contains one (malformed) JSON file.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/1);
}

TEST_F(ExternalWebAppManagerTest, TxtButNoJson) {
  const auto app_infos = LoadApps("txt_but_no_json");

  // The txt_but_no_json directory contains one file, and the contents of that
  // file is valid JSON, but that file's name does not end with ".json".
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/0);
}

TEST_F(ExternalWebAppManagerTest, MixedJson) {
  const auto app_infos = LoadApps("mixed_json");

  // The mixed_json directory contains one empty JSON file, one malformed JSON
  // file and one good JSON file. ScanDirForExternalWebAppsForTesting should
  // still pick up that one good JSON file: polytimer.json.
  EXPECT_EQ(1u, app_infos.size());
  if (app_infos.size() == 1) {
    EXPECT_EQ(app_infos[0].install_url.spec(),
              std::string("https://polytimer.rocks/?homescreen=1"));
  }
  ExpectHistograms(/*enabled=*/1, /*disabled=*/0, /*errors=*/2);
}

TEST_F(ExternalWebAppManagerTest, MissingAppUrl) {
  const auto app_infos = LoadApps("missing_app_url");

  // The missing_app_url directory contains one JSON file which is correct
  // except for a missing "app_url" field.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/1);
}

TEST_F(ExternalWebAppManagerTest, EmptyAppUrl) {
  const auto app_infos = LoadApps("empty_app_url");

  // The empty_app_url directory contains one JSON file which is correct
  // except for an empty "app_url" field.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/1);
}

TEST_F(ExternalWebAppManagerTest, InvalidAppUrl) {
  const auto app_infos = LoadApps("invalid_app_url");

  // The invalid_app_url directory contains one JSON file which is correct
  // except for an invalid "app_url" field.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/1);
}

TEST_F(ExternalWebAppManagerTest, TrueHideFromUser) {
  const auto app_infos = LoadApps("true_hide_from_user");

  EXPECT_EQ(1u, app_infos.size());
  const auto& app = app_infos[0];
  EXPECT_FALSE(app.add_to_applications_menu);
  EXPECT_FALSE(app.add_to_search);
  EXPECT_FALSE(app.add_to_management);
  ExpectHistograms(/*enabled=*/1, /*disabled=*/0, /*errors=*/0);
}

TEST_F(ExternalWebAppManagerTest, InvalidHideFromUser) {
  const auto app_infos = LoadApps("invalid_hide_from_user");

  // The invalid_hide_from_user directory contains on JSON file which is correct
  // except for an invalid "hide_from_user" field.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/1);
}

TEST_F(ExternalWebAppManagerTest, InvalidCreateShortcuts) {
  const auto app_infos = LoadApps("invalid_create_shortcuts");

  // The invalid_create_shortcuts directory contains one JSON file which is
  // correct except for an invalid "create_shortcuts" field.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/1);
}

TEST_F(ExternalWebAppManagerTest, MissingLaunchContainer) {
  const auto app_infos = LoadApps("missing_launch_container");

  // The missing_launch_container directory contains one JSON file which is
  // correct except for a missing "launch_container" field.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/1);
}

TEST_F(ExternalWebAppManagerTest, InvalidLaunchContainer) {
  const auto app_infos = LoadApps("invalid_launch_container");

  // The invalid_launch_container directory contains one JSON file which is
  // correct except for an invalid "launch_container" field.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/1);
}

TEST_F(ExternalWebAppManagerTest, InvalidUninstallAndReplace) {
  const auto app_infos = LoadApps("invalid_uninstall_and_replace");

  // The invalid_uninstall_and_replace directory contains 2 JSON files which are
  // correct except for invalid "uninstall_and_replace" fields.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/2);
}

TEST_F(ExternalWebAppManagerTest, DefaultWebAppInstallDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kDefaultWebAppInstallation);
  const auto app_infos = LoadApps(kGoodJsonTestDir);

  EXPECT_EQ(0u, app_infos.size());
  histograms_.ExpectTotalCount(
      ExternalWebAppManager::kHistogramConfigErrorCount, 0);
  histograms_.ExpectTotalCount(ExternalWebAppManager::kHistogramEnabledCount,
                               0);
  histograms_.ExpectTotalCount(ExternalWebAppManager::kHistogramDisabledCount,
                               0);
}

TEST_F(ExternalWebAppManagerTest, EnabledByFinch) {
  base::AutoReset<bool> testing_scope =
      SetExternalAppInstallFeatureAlwaysEnabledForTesting();

  const auto app_infos = LoadApps("enabled_by_finch");

  // The enabled_by_finch directory contains two JSON file containing apps
  // that have field trials. As the matching feature is enabled, they should be
  // in our list of apps to install.
  EXPECT_EQ(2u, app_infos.size());
  ExpectHistograms(/*enabled=*/2, /*disabled=*/0, /*errors=*/0);
}

TEST_F(ExternalWebAppManagerTest, NotEnabledByFinch) {
  const auto app_infos = LoadApps("enabled_by_finch");

  // The enabled_by_finch directory contains two JSON file containing apps
  // that have field trials. As the matching feature isn't enabled, they should
  // not be in our list of apps to install.
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/2, /*errors=*/0);
}

TEST_F(ExternalWebAppManagerTest, ChildUser) {
  const auto profile = CreateProfileAndLogin();
  profile->SetSupervisedUserId(supervised_users::kChildAccountSUID);
  VerifySetOfApps(profile.get(), {GURL(kAppAllUrl), GURL(kAppChildUrl)});
}

TEST_F(ExternalWebAppManagerTest, GuestUser) {
  VerifySetOfApps(CreateGuestProfileAndLogin().get(),
                  {GURL(kAppAllUrl), GURL(kAppGuestUrl)});
}

TEST_F(ExternalWebAppManagerTest, ManagedUser) {
  const auto profile = CreateProfileAndLogin();
  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  VerifySetOfApps(profile.get(), {GURL(kAppAllUrl), GURL(kAppManagedUrl)});
}

TEST_F(ExternalWebAppManagerTest, UnmanagedUser) {
  VerifySetOfApps(CreateProfileAndLogin().get(),
                  {GURL(kAppAllUrl), GURL(kAppUnmanagedUrl)});
}

TEST_F(ExternalWebAppManagerTest, NonPrimaryProfile) {
  VerifySetOfApps(CreateProfile().get(),
                  {GURL(kAppAllUrl), GURL(kAppUnmanagedUrl)});
}

TEST_F(ExternalWebAppManagerTest, ExtraWebApps) {
  // The extra_web_apps directory contains two JSON files in different named
  // subdirectories. The --extra-web-apps-dir switch should control which
  // directory apps are loaded from.
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      chromeos::switches::kExtraWebAppsDir, "model1");

  const auto app_infos = LoadApps("extra_web_apps");
  EXPECT_EQ(1u, app_infos.size());
  ExpectHistograms(/*enabled=*/1, /*disabled=*/0, /*errors=*/0);
}

TEST_F(ExternalWebAppManagerTest, ExtraWebAppsNoMatchingDirectory) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      chromeos::switches::kExtraWebAppsDir, "model3");

  const auto app_infos = LoadApps("extra_web_apps");
  EXPECT_EQ(0u, app_infos.size());
  ExpectHistograms(/*enabled=*/0, /*disabled=*/0, /*errors=*/0);
}

#else   // BUILDFLAG(IS_CHROMEOS_ASH)
// No app is expected for non-ChromeOS builds.
TEST_F(ExternalWebAppManagerTest, NoApp) {
  EXPECT_TRUE(LoadApps(kUserTypesTestDir, CreateProfile().get()).empty());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace web_app
