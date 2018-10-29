// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_search_provider.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_item.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_default_app_list.h"
#include "chrome/browser/ui/app_list/extension_app_model_builder.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_search_result_ranker.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/crx_file/id_util.h"
#include "components/sync/model/string_ordinal.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace test {

namespace {

constexpr char kGmailQeuery[] = "Gmail";
constexpr char kGmailArcName[] = "Gmail ARC";
constexpr char kGmailExtensionName[] = "Gmail Ext";
constexpr char kGmailArcPackage[] = "com.google.android.gm";
constexpr char kGmailArcActivity[] =
    "com.google.android.gm.ConversationListActivityGmail";
constexpr char kKeyboardShortcutHelperInternalName[] = "Shortcuts";

constexpr char kRankingAppQuery[] = "testRankingApp";

// Activity and package should match
// chrome/test/data/arc_default_apps/test_app1.json
constexpr char kRankingInternalAppActivity[] = "test.app1.activity";
constexpr char kRankingInternalAppName[] = "testRankingAppInternal";
constexpr char kRankingInternalAppPackageName[] = "test.app1";

constexpr char kRankingNormalAppActivity[] = "test.ranking.app.normal.activity";
constexpr char kRankingNormalAppName[] = "testRankingAppNormal";
constexpr char kRankingNormalAppPackageName[] = "test.ranking.app.normal";

constexpr char kSettingsInternalName[] = "Settings";

// Waits for base::Time::Now() is updated.
void WaitTimeUpdated() {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromMilliseconds(1));
  run_loop.Run();
}

}  // namespace

const base::Time kTestCurrentTime = base::Time::FromInternalValue(100000);

bool MoreRelevant(const ChromeSearchResult* result1,
                  const ChromeSearchResult* result2) {
  return result1->relevance() > result2->relevance();
}

class AppSearchProviderTest : public AppListTestBase {
 public:
  AppSearchProviderTest() {}
  ~AppSearchProviderTest() override {}

  // AppListTestBase overrides:
  void SetUp() override {
    AppListTestBase::SetUp();

    model_updater_ = std::make_unique<FakeAppListModelUpdater>();
    controller_ = std::make_unique<::test::TestAppListControllerDelegate>();
  }

  void CreateSearch() {
    clock_.SetNow(kTestCurrentTime);
    app_search_ = std::make_unique<AppSearchProvider>(
        profile_.get(), nullptr, &clock_, model_updater_.get());
  }

  std::string RunQuery(const std::string& query) {
    app_search_->Start(base::UTF8ToUTF16(query));

    // Sort results by relevance.
    std::vector<ChromeSearchResult*> sorted_results;
    for (const auto& result : app_search_->results())
      sorted_results.emplace_back(result.get());
    std::sort(sorted_results.begin(), sorted_results.end(), &MoreRelevant);

    std::string result_str;
    for (size_t i = 0; i < sorted_results.size(); ++i) {
      if (!result_str.empty())
        result_str += ',';

      result_str += base::UTF16ToUTF8(sorted_results[i]->title());
    }
    return result_str;
  }

  std::string AddArcApp(const std::string& name,
                        const std::string& package,
                        const std::string& activity) {
    arc::mojom::AppInfo app_info;
    app_info.name = name;
    app_info.package_name = package;
    app_info.activity = activity;
    app_info.sticky = false;
    app_info.notifications_enabled = false;
    arc_test_.app_instance()->SendAppAdded(app_info);
    return ArcAppListPrefs::GetAppId(package, activity);
  }

  void AddExtension(const std::string& id,
                    const std::string& name,
                    extensions::Manifest::Location location,
                    int extra_flags) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder()
            .SetManifest(
                extensions::DictionaryBuilder()
                    .Set("name", name)
                    .Set("version", "0.1")
                    .Set("app",
                         extensions::DictionaryBuilder()
                             .Set("urls",
                                  extensions::ListBuilder()
                                      .Append("http://localhost/extensions/"
                                              "hosted_app/main.html")
                                      .Build())
                             .Build())
                    .Set("launch",
                         extensions::DictionaryBuilder()
                             .Set("urls",
                                  extensions::ListBuilder()
                                      .Append("http://localhost/extensions/"
                                              "hosted_app/main.html")
                                      .Build())
                             .Build())
                    .Build())
            .SetLocation(location)
            .AddFlags(extra_flags)
            .SetID(id)
            .Build();
    service()->AddExtension(extension.get());
    // This sets install time and other extension parameters.
    extensions::ExtensionPrefs::Get(profile())->OnExtensionInstalled(
        extension.get(), extensions::Extension::ENABLED,
        syncer::StringOrdinal::CreateInitialOrdinal() /* page_ordinal */,
        std::string() /* install_parameter */);
  }

  const SearchProvider::Results& results() { return app_search_->results(); }
  ArcAppTest& arc_test() { return arc_test_; }

  // Train the |app_search| provider with id.
  void Train(const std::string& id) { app_search_->Train(id); }

 private:
  base::SimpleTestClock clock_;
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  std::unique_ptr<AppSearchProvider> app_search_;
  std::unique_ptr<::test::TestAppListControllerDelegate> controller_;
  ArcAppTest arc_test_;

  DISALLOW_COPY_AND_ASSIGN(AppSearchProviderTest);
};

TEST_F(AppSearchProviderTest, Basic) {
  arc_test().SetUp(profile());
  arc_test().app_instance()->RefreshAppList();
  std::vector<arc::mojom::AppInfo> arc_apps(arc_test().fake_apps().begin(),
                                            arc_test().fake_apps().begin() + 2);
  arc_test().app_instance()->SendRefreshAppList(arc_apps);

  CreateSearch();

  EXPECT_EQ("", RunQuery("!@#$-,-_"));
  EXPECT_EQ("", RunQuery("unmatched query"));

  // Search for "pa" should return both packaged app. The order is undefined
  // because the test only considers textual relevance and the two apps end
  // up having the same score.
  std::string result = RunQuery("pa");
  EXPECT_TRUE(result == "Packaged App 1,Packaged App 2" ||
              result == "Packaged App 2,Packaged App 1");

  EXPECT_EQ("Packaged App 1", RunQuery("pa1"));
  EXPECT_EQ("Packaged App 2", RunQuery("pa2"));
  EXPECT_EQ("Packaged App 1", RunQuery("papp1"));
  EXPECT_EQ("Hosted App", RunQuery("host"));

  result = RunQuery("fake");
  EXPECT_TRUE(result == "Fake App 0,Fake App 1" ||
              result == "Fake App 1,Fake App 0");
  result = RunQuery("app1");
  EXPECT_TRUE(result == "Packaged App 1,Fake App 1" ||
              result == "Fake App 1,Packaged App 1");
}

TEST_F(AppSearchProviderTest, DisableAndEnable) {
  CreateSearch();

  EXPECT_EQ("Hosted App", RunQuery("host"));

  service_->DisableExtension(kHostedAppId,
                             extensions::disable_reason::DISABLE_USER_ACTION);
  EXPECT_EQ("Hosted App", RunQuery("host"));

  service_->EnableExtension(kHostedAppId);
  EXPECT_EQ("Hosted App", RunQuery("host"));
}

TEST_F(AppSearchProviderTest, UninstallExtension) {
  CreateSearch();

  EXPECT_EQ("Packaged App 1", RunQuery("pa1"));
  EXPECT_FALSE(results().empty());
  service_->UninstallExtension(kPackagedApp1Id,
                               extensions::UNINSTALL_REASON_FOR_TESTING, NULL);

  // Allow async AppSearchProvider::UpdateResults to run.
  base::RunLoop().RunUntilIdle();

  // Uninstalling an app should update the result list without needing to start
  // a new search.
  EXPECT_TRUE(results().empty());

  // Rerunning the query also should return no results.
  EXPECT_EQ("", RunQuery("pa1"));

  // Let uninstall code to clean up.
  base::RunLoop().RunUntilIdle();
}

TEST_F(AppSearchProviderTest, InstallUninstallArc) {
  arc_test().SetUp(profile());
  std::vector<arc::mojom::AppInfo> arc_apps;
  arc_test().app_instance()->RefreshAppList();
  arc_test().app_instance()->SendRefreshAppList(arc_apps);

  CreateSearch();

  EXPECT_TRUE(results().empty());
  EXPECT_EQ("", RunQuery("fapp0"));

  arc_apps.push_back(arc_test().fake_apps()[0]);
  arc_test().app_instance()->RefreshAppList();
  arc_test().app_instance()->SendRefreshAppList(arc_apps);

  // Allow async AppSearchProvider::UpdateResults to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("Fake App 0", RunQuery("fapp0"));
  EXPECT_FALSE(results().empty());

  arc_apps.clear();
  arc_test().app_instance()->RefreshAppList();
  arc_test().app_instance()->SendRefreshAppList(arc_apps);

  // Allow async AppSearchProvider::UpdateResults to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(results().empty());
  EXPECT_EQ("", RunQuery("fapp0"));

  // Let uninstall code to clean up.
  base::RunLoop().RunUntilIdle();
}

TEST_F(AppSearchProviderTest, FetchRecommendations) {
  CreateSearch();

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile_.get());

  prefs->SetLastLaunchTime(kHostedAppId, base::Time::FromInternalValue(20));
  prefs->SetLastLaunchTime(kPackagedApp1Id, base::Time::FromInternalValue(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, base::Time::FromInternalValue(5));
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2,Settings,Camera",
            RunQuery(""));

  prefs->SetLastLaunchTime(kHostedAppId, base::Time::FromInternalValue(5));
  prefs->SetLastLaunchTime(kPackagedApp1Id, base::Time::FromInternalValue(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, base::Time::FromInternalValue(20));
  EXPECT_EQ("Packaged App 2,Packaged App 1,Hosted App,Settings,Camera",
            RunQuery(""));

  // Times in the future should just be handled as highest priority.
  prefs->SetLastLaunchTime(kHostedAppId,
                           kTestCurrentTime + base::TimeDelta::FromSeconds(5));
  prefs->SetLastLaunchTime(kPackagedApp1Id, base::Time::FromInternalValue(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, base::Time::FromInternalValue(5));
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2,Settings,Camera",
            RunQuery(""));
}

TEST_F(AppSearchProviderTest, FetchUnlaunchedRecommendations) {
  CreateSearch();

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile_.get());

  // The order of unlaunched recommendations should be based on the install time
  // order.
  prefs->SetLastLaunchTime(kHostedAppId, base::Time::Now());
  prefs->SetLastLaunchTime(kPackagedApp1Id, base::Time::FromInternalValue(0));
  prefs->SetLastLaunchTime(kPackagedApp2Id, base::Time::FromInternalValue(0));
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2,Settings,Camera",
            RunQuery(""));
}

TEST_F(AppSearchProviderTest, FetchRecommendationsFromRanker) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {app_list_features::kEnableAppSearchResultRanker}, {});
  CreateSearch();

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile_.get());

  prefs->SetLastLaunchTime(kHostedAppId, base::Time::FromInternalValue(20));
  prefs->SetLastLaunchTime(kPackagedApp1Id, base::Time::FromInternalValue(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, base::Time::FromInternalValue(5));
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2,Settings,Camera",
            RunQuery(""));

  Train(kPackagedApp2Id);
  Train(kPackagedApp2Id);
  Train(kPackagedApp1Id);
  EXPECT_EQ("Packaged App 2,Packaged App 1,Hosted App,Settings,Camera",
            RunQuery(""));
}

TEST_F(AppSearchProviderTest, RankerIsDisabledWithFlag) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {}, {app_list_features::kEnableAppSearchResultRanker});
  CreateSearch();

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile_.get());

  prefs->SetLastLaunchTime(kHostedAppId, base::Time::FromInternalValue(20));
  prefs->SetLastLaunchTime(kPackagedApp1Id, base::Time::FromInternalValue(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, base::Time::FromInternalValue(5));
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2,Settings,Camera",
            RunQuery(""));

  Train(kPackagedApp2Id);
  Train(kPackagedApp2Id);
  Train(kPackagedApp1Id);
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2,Settings,Camera",
            RunQuery(""));
}

TEST_F(AppSearchProviderTest, FilterDuplicate) {
  arc_test().SetUp(profile());

  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(profile_.get());
  ASSERT_TRUE(extension_prefs);

  AddExtension(extension_misc::kGmailAppId, kGmailExtensionName,
               extensions::Manifest::EXTERNAL_PREF_DOWNLOAD,
               0 /* extra_flags */);
  const std::string arc_gmail_app_id =
      AddArcApp(kGmailArcName, kGmailArcPackage, kGmailArcActivity);
  arc_test().arc_app_list_prefs()->SetLastLaunchTime(arc_gmail_app_id);

  std::unique_ptr<ArcAppListPrefs::AppInfo> arc_gmail_app_info =
      arc_test().arc_app_list_prefs()->GetApp(arc_gmail_app_id);
  ASSERT_TRUE(arc_gmail_app_info);

  EXPECT_FALSE(arc_gmail_app_info->last_launch_time.is_null());
  EXPECT_FALSE(arc_gmail_app_info->install_time.is_null());

  extension_prefs->SetLastLaunchTime(
      extension_misc::kGmailAppId,
      arc_gmail_app_info->last_launch_time - base::TimeDelta::FromSeconds(1));

  CreateSearch();
  EXPECT_EQ(kGmailArcName, RunQuery(kGmailQeuery));

  extension_prefs->SetLastLaunchTime(
      extension_misc::kGmailAppId,
      arc_gmail_app_info->last_launch_time + base::TimeDelta::FromSeconds(1));

  CreateSearch();
  EXPECT_EQ(kGmailExtensionName, RunQuery(kGmailQeuery));
}

TEST_F(AppSearchProviderTest, FetchInternalApp) {
  CreateSearch();

  // Search Keyboard Shortcut Helper.
  EXPECT_EQ(kKeyboardShortcutHelperInternalName, RunQuery("Keyboard"));
  EXPECT_EQ(kKeyboardShortcutHelperInternalName, RunQuery("Shortcut"));
  EXPECT_EQ(kKeyboardShortcutHelperInternalName, RunQuery("Helper"));

  // Search Settings.
  EXPECT_EQ(kSettingsInternalName, RunQuery("Settings"));
  EXPECT_EQ(kSettingsInternalName, RunQuery("Set"));
}

enum class TestExtensionInstallType {
  CONTROLLED_BY_POLICY,
  CHROME_COMPONENT,
  INSTALLED_BY_DEFAULT,
  INSTALLED_BY_OEM,
};

class AppSearchProviderWithExtensionInstallType
    : public AppSearchProviderTest,
      public ::testing::WithParamInterface<TestExtensionInstallType> {
 public:
  AppSearchProviderWithExtensionInstallType() = default;
  ~AppSearchProviderWithExtensionInstallType() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppSearchProviderWithExtensionInstallType);
};

TEST_P(AppSearchProviderWithExtensionInstallType, InstallInernallyRanking) {
  extensions::ExtensionPrefs* const prefs =
      extensions::ExtensionPrefs::Get(profile());
  ASSERT_TRUE(prefs);

  // Install normal app.
  const std::string normal_app_id =
      crx_file::id_util::GenerateId(kRankingNormalAppName);
  AddExtension(normal_app_id, kRankingNormalAppName,
               extensions::Manifest::EXTERNAL_PREF_DOWNLOAD,
               0 /* extra_flags */);

  // Wait a bit to make sure time is updated.
  WaitTimeUpdated();

  // Install app internally.
  const std::string internal_app_id =
      crx_file::id_util::GenerateId(kRankingInternalAppName);
  switch (GetParam()) {
    case TestExtensionInstallType::CONTROLLED_BY_POLICY:
      AddExtension(internal_app_id, kRankingInternalAppName,
                   extensions::Manifest::EXTERNAL_POLICY_DOWNLOAD,
                   0 /* extra_flags */);
      break;
    case TestExtensionInstallType::CHROME_COMPONENT:
      AddExtension(internal_app_id, kRankingInternalAppName,
                   extensions::Manifest::COMPONENT, 0 /* extra_flags */);
      break;
    case TestExtensionInstallType::INSTALLED_BY_DEFAULT:
      AddExtension(internal_app_id, kRankingInternalAppName,
                   extensions::Manifest::EXTERNAL_PREF_DOWNLOAD,
                   extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
      break;
    case TestExtensionInstallType::INSTALLED_BY_OEM:
      AddExtension(internal_app_id, kRankingInternalAppName,
                   extensions::Manifest::EXTERNAL_PREF_DOWNLOAD,
                   extensions::Extension::WAS_INSTALLED_BY_OEM);
      break;
  }

  EXPECT_LT(prefs->GetInstallTime(normal_app_id),
            prefs->GetInstallTime(internal_app_id));

  // Installed internally app has runking below other apps, even if it's install
  // time is later.
  CreateSearch();
  EXPECT_EQ(std::string(kRankingNormalAppName) + "," +
                std::string(kRankingInternalAppName),
            RunQuery(kRankingAppQuery));

  // Using installed internally app moves it in ranking up.
  WaitTimeUpdated();
  prefs->SetLastLaunchTime(internal_app_id, base::Time::Now());
  CreateSearch();
  EXPECT_EQ(std::string(kRankingInternalAppName) + "," +
                std::string(kRankingNormalAppName),
            RunQuery(kRankingAppQuery));
}

INSTANTIATE_TEST_CASE_P(
    ,
    AppSearchProviderWithExtensionInstallType,
    ::testing::ValuesIn({TestExtensionInstallType::CONTROLLED_BY_POLICY,
                         TestExtensionInstallType::CHROME_COMPONENT,
                         TestExtensionInstallType::INSTALLED_BY_DEFAULT,
                         TestExtensionInstallType::INSTALLED_BY_OEM}));

enum class TestArcAppInstallType {
  CONTROLLED_BY_POLICY,
  INSTALLED_BY_DEFAULT,
};

class AppSearchProviderWithArcAppInstallType
    : public AppSearchProviderTest,
      public ::testing::WithParamInterface<TestArcAppInstallType> {
 public:
  AppSearchProviderWithArcAppInstallType() = default;
  ~AppSearchProviderWithArcAppInstallType() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppSearchProviderWithArcAppInstallType);
};

// TODO (879413): Enable this after resolving flakiness.
TEST_P(AppSearchProviderWithArcAppInstallType,
       DISABLED_InstallInernallyRanking) {
  const bool default_app =
      GetParam() == TestArcAppInstallType::INSTALLED_BY_DEFAULT;
  if (default_app) {
    ArcDefaultAppList::UseTestAppsDirectory();
    arc_test().set_wait_default_apps(true);
  }
  arc_test().SetUp(profile());

  ArcAppListPrefs* const prefs = arc_test().arc_app_list_prefs();
  ASSERT_TRUE(prefs);

  // Install normal app.
  const std::string normal_app_id =
      AddArcApp(kRankingNormalAppName, kRankingNormalAppPackageName,
                kRankingNormalAppActivity);

  // Wait a bit to make sure time is updated.
  WaitTimeUpdated();

  if (GetParam() == TestArcAppInstallType::CONTROLLED_BY_POLICY) {
    const std::string policy = base::StringPrintf(
        "{\"applications\":[{\"installType\":\"FORCE_INSTALLED\","
        "\"packageName\":"
        "\"%s\"}]}",
        kRankingInternalAppPackageName);
    prefs->OnPolicySent(policy);
  }

  // Reinstall default app to make install time after normall app install time.
  if (default_app) {
    static_cast<arc::mojom::AppHost*>(prefs)->OnPackageAppListRefreshed(
        kRankingInternalAppPackageName, {} /* apps */);
  }

  const std::string internal_app_id =
      AddArcApp(kRankingInternalAppName, kRankingInternalAppPackageName,
                kRankingInternalAppActivity);

  EXPECT_EQ(default_app, prefs->IsDefault(internal_app_id));

  std::unique_ptr<ArcAppListPrefs::AppInfo> normal_app =
      prefs->GetApp(normal_app_id);
  std::unique_ptr<ArcAppListPrefs::AppInfo> internal_app =
      prefs->GetApp(internal_app_id);
  ASSERT_TRUE(normal_app && internal_app);

  EXPECT_LT(normal_app->install_time, internal_app->install_time);

  // Installed internally app has runking below other apps, even if it's install
  // time is later.
  CreateSearch();
  EXPECT_EQ(std::string(kRankingNormalAppName) + "," +
                std::string(kRankingInternalAppName),
            RunQuery(kRankingAppQuery));

  // Using installed internally app moves it in ranking up.
  WaitTimeUpdated();
  prefs->SetLastLaunchTime(internal_app_id);
  CreateSearch();
  EXPECT_EQ(std::string(kRankingInternalAppName) + "," +
                std::string(kRankingNormalAppName),
            RunQuery(kRankingAppQuery));
}

INSTANTIATE_TEST_CASE_P(
    ,
    AppSearchProviderWithArcAppInstallType,
    ::testing::ValuesIn({TestArcAppInstallType::CONTROLLED_BY_POLICY,
                         TestArcAppInstallType::INSTALLED_BY_DEFAULT}));

}  // namespace test
}  // namespace app_list
