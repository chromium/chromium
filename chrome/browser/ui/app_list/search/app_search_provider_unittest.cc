// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_search_provider.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_default_app_list.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "components/crx_file/id_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/stub_icon_loader.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_sessions/mock_sync_sessions_client.h"
#include "components/sync_sessions/open_tabs_ui_delegate_impl.h"
#include "components/sync_sessions/synced_session_tracker.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;

namespace app_list {
namespace test {

namespace {

constexpr char kGmailQuery[] = "Gmail";
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

constexpr char kWebAppUrl[] = "https://webappone.com/";
constexpr char kWebAppName[] = "WebApp1";

// Waits for base::Time::Now() is updated.
void WaitTimeUpdated() {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(1));
  run_loop.Run();
}

base::Time MicrosecondsSinceEpoch(int microseconds) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(microseconds));
}

}  // namespace

const base::Time kTestCurrentTime = MicrosecondsSinceEpoch(100000);

bool MoreRelevant(const ChromeSearchResult* result1,
                  const ChromeSearchResult* result2) {
  return result1->relevance() > result2->relevance();
}

void UpdateIconKey(apps::AppServiceProxy& proxy, const std::string& app_id) {
  apps::AppType app_type;
  apps::IconKeyPtr icon_key;
  proxy.AppRegistryCache().ForOneApp(
      app_id, [&app_type, &icon_key](const apps::AppUpdate& update) {
        app_type = update.AppType();
        icon_key = std::make_unique<apps::IconKey>(
            update.IconKey()->timeline + 1, update.IconKey()->resource_id,
            update.IconKey()->icon_effects);
      });

  std::vector<apps::AppPtr> apps;
  apps::AppPtr app = std::make_unique<apps::App>(app_type, app_id);
  app->icon_key = std::move(*icon_key);
  apps.push_back(std::move(app));
  proxy.AppRegistryCache().OnApps(std::move(apps), apps::AppType::kUnknown,
                                  false /* should_notify_initialized */);
}

class AppSearchProviderTest : public AppListTestBase {
 public:
  AppSearchProviderTest() = default;

  AppSearchProviderTest(const AppSearchProviderTest&) = delete;
  AppSearchProviderTest& operator=(const AppSearchProviderTest&) = delete;

  ~AppSearchProviderTest() override {}

  // AppListTestBase overrides:
  void SetUp() override {
    AppListTestBase::SetUp();

    model_updater_ = std::make_unique<FakeAppListModelUpdater>(
        /*profile=*/nullptr, /*reorder_delegate=*/nullptr);
    controller_ = std::make_unique<::test::TestAppListControllerDelegate>();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void CreateSearch() {
    clock_.SetNow(kTestCurrentTime);
    search_controller_ = std::make_unique<TestSearchController>();
    auto app_search = std::make_unique<AppSearchProvider>(
        profile_.get(), nullptr, &clock_, model_updater_.get());
    app_search_ = app_search.get();
    search_controller_->AddProvider(0, std::move(app_search));
  }

  void CreateSearchWithContinueReading() {
    CreateSearch();

    session_tracker_ = std::make_unique<sync_sessions::SyncedSessionTracker>(
        &mock_sync_sessions_client_);
    open_tabs_ui_delegate_ =
        std::make_unique<sync_sessions::OpenTabsUIDelegateImpl>(
            &mock_sync_sessions_client_, session_tracker_.get(),
            base::DoNothing());
    app_search_->set_open_tabs_ui_delegate_for_testing(
        open_tabs_ui_delegate_.get());
  }

  std::string RunQuery(const std::string& query) {
    if (query.empty()) {
      search_controller_->StartZeroState(base::DoNothing(), base::TimeDelta());
    } else {
      search_controller_->StartSearch(base::UTF8ToUTF16(query));
    }

    // Sort results by relevance.
    std::vector<ChromeSearchResult*> sorted_results;
    for (const auto& result : results())
      sorted_results.emplace_back(result.get());
    std::sort(sorted_results.begin(), sorted_results.end(), &MoreRelevant);

    // If the query is empty and we're in the non-productivity launcher, every
    // other result is a chip result identical to the tile result. Skip these.
    const int increment =
        (!app_list_features::IsCategoricalSearchEnabled() && query.empty()) ? 2
                                                                            : 1;
    std::string result_str;
    for (size_t i = 0; i < sorted_results.size(); i += increment) {
      if (!result_str.empty())
        result_str += ',';

      result_str += base::UTF16ToUTF8(sorted_results[i]->title());
    }
    return result_str;
  }

  // Used for testing Continue Reading. Because the result is placed in the
  // container based on index flags instead of relevance, use this methodology
  // to generate list of test results.
  std::string RunQueryNotSortingByRelevance(const std::string& query) {
    search_controller_->StartSearch(base::UTF8ToUTF16(query));

    std::vector<ChromeSearchResult*> non_relevance_results;
    std::vector<ChromeSearchResult*> priority_results;
    for (const auto& result : results()) {
      if (result->display_index() == ash::kFirstIndex &&
          (result->display_type() == ash::kChip ||
           result->display_type() == ash::kTile)) {
        priority_results.emplace_back(result.get());
      } else {
        non_relevance_results.emplace_back(result.get());
      }
    }

    if (priority_results.size() != 0) {
      non_relevance_results.insert(non_relevance_results.begin(),
                                   priority_results.begin(),
                                   priority_results.end());
    }

    // If the query is empty, every other result is a chip result identical to
    // the tile result. Skip these.
    const int increment = query.empty() ? 2 : 1;
    std::string result_str;
    for (size_t i = 0; i < non_relevance_results.size(); i += increment) {
      auto* result = non_relevance_results[i];
      if (!result_str.empty())
        result_str += ',';

      result_str += base::UTF16ToUTF8(result->title());
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
                    ManifestLocation location,
                    int init_from_value_flags) {
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
            .AddFlags(init_from_value_flags)
            .SetID(id)
            .Build();

    const syncer::StringOrdinal& page_ordinal =
        syncer::StringOrdinal::CreateInitialOrdinal();

    service()->OnExtensionInstalled(extension.get(), page_ordinal,
                                    extensions::kInstallFlagNone);
  }

  const SearchProvider::Results& results() {
    if (app_list_features::IsCategoricalSearchEnabled()) {
      return search_controller_->last_results();
    } else {
      return app_search_->results();
    }
  }

  ArcAppTest& arc_test() { return arc_test_; }

  void CallViewClosing() { app_search_->ViewClosing(); }

  sync_sessions::SyncedSessionTracker* session_tracker() {
    return session_tracker_.get();
  }

 private:
  base::SimpleTestClock clock_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  std::unique_ptr<TestSearchController> search_controller_;
  AppSearchProvider* app_search_ = nullptr;
  std::unique_ptr<::test::TestAppListControllerDelegate> controller_;
  ArcAppTest arc_test_;

  // For continue reading.
  testing::NiceMock<sync_sessions::MockSyncSessionsClient>
      mock_sync_sessions_client_;
  std::unique_ptr<sync_sessions::SyncedSessionTracker> session_tracker_;
  std::unique_ptr<sync_sessions::OpenTabsUIDelegateImpl> open_tabs_ui_delegate_;
};

TEST_F(AppSearchProviderTest, Basic) {
  arc_test().SetUp(profile());
  std::vector<arc::mojom::AppInfoPtr> arc_apps;
  for (int i = 0; i < 2; i++)
    arc_apps.emplace_back(arc_test().fake_apps()[i]->Clone());
  arc_test().app_instance()->SendRefreshAppList(arc_apps);

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

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
  arc_test().TearDown();
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
  service_->UninstallExtension(
      kPackagedApp1Id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);

  // Allow async callbacks to run.
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
  std::vector<arc::mojom::AppInfoPtr> arc_apps;
  arc_test().app_instance()->SendRefreshAppList(arc_apps);

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  CreateSearch();

  EXPECT_TRUE(results().empty());
  EXPECT_EQ("", RunQuery("fapp0"));

  arc_apps.emplace_back(arc_test().fake_apps()[0]->Clone());
  arc_test().app_instance()->SendRefreshAppList(arc_apps);

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("Fake App 0", RunQuery("fapp0"));
  EXPECT_FALSE(results().empty());

  arc_apps.clear();
  arc_test().app_instance()->SendRefreshAppList(arc_apps);

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(results().empty());
  EXPECT_EQ("", RunQuery("fapp0"));

  // Let uninstall code to clean up.
  base::RunLoop().RunUntilIdle();

  arc_test().TearDown();
}

TEST_F(AppSearchProviderTest, FetchRecommendations) {
  CreateSearch();

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile_.get());

  prefs->SetLastLaunchTime(kHostedAppId, MicrosecondsSinceEpoch(20));
  prefs->SetLastLaunchTime(kPackagedApp1Id, MicrosecondsSinceEpoch(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, MicrosecondsSinceEpoch(5));
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunQuery(""));

  prefs->SetLastLaunchTime(kHostedAppId, MicrosecondsSinceEpoch(5));
  prefs->SetLastLaunchTime(kPackagedApp1Id, MicrosecondsSinceEpoch(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, MicrosecondsSinceEpoch(20));
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("Packaged App 2,Packaged App 1,Hosted App", RunQuery(""));

  // Times in the future should just be handled as highest priority.
  prefs->SetLastLaunchTime(kHostedAppId, kTestCurrentTime + base::Seconds(5));
  prefs->SetLastLaunchTime(kPackagedApp1Id, MicrosecondsSinceEpoch(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, MicrosecondsSinceEpoch(5));
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunQuery(""));
}

TEST_F(AppSearchProviderTest, DefaultRecommendedAppRanking) {
  // Disable the pre-installed high-priority extensions. This test simulates
  // a brand new profile being added to a device, and should not include these.
  service_->UninstallExtension(
      kHostedAppId, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  service_->UninstallExtension(
      kPackagedApp1Id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  service_->UninstallExtension(
      kPackagedApp2Id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);

  base::RunLoop().RunUntilIdle();

  profile_->SetIsNewProfile(true);
  ASSERT_TRUE(profile()->IsNewProfile());

  // There are four default web apps. We use real app IDs here, as these are
  // used internally by the ranking logic. We can use arbitrary app names.
  //
  // TODO(crbug.com/1235272): There is one default ARC app (PlayStore). Figure
  // out how to test-install PlayStore as a default ARC app.
  const std::vector<std::string> kDefaultRecommendedWebAppIds = {
      web_app::kCanvasAppId, web_app::kHelpAppId, web_app::kOsSettingsAppId,
      web_app::kCameraAppId};
  const std::vector<std::string> kDefaultRecommendedWebAppNames = {
      "Canvas", "Help", "OsSettings", "Camera"};

  ASSERT_EQ(kDefaultRecommendedWebAppNames.size(),
            kDefaultRecommendedWebAppIds.size());

  // Install the default recommended web apps.
  // N.B. These are web apps and not extensions, but these installations are
  // simulated using extensions because it allows us to set the app ID.
  for (size_t i = 0; i < kDefaultRecommendedWebAppNames.size(); ++i) {
    AddExtension(kDefaultRecommendedWebAppIds[i],
                 kDefaultRecommendedWebAppNames[i],
                 ManifestLocation::kExternalPrefDownload,
                 extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
    service_->EnableExtension(kDefaultRecommendedWebAppIds[i]);
  }

  // Allow app installations to finish.
  base::RunLoop().RunUntilIdle();
  CreateSearch();

  EXPECT_EQ("OsSettings,Help,Canvas,Camera", RunQuery(""));

  // Install a normal (non-default-installed) app.
  const std::string normal_app_id =
      crx_file::id_util::GenerateId(kRankingNormalAppName);
  AddExtension(normal_app_id, kRankingNormalAppName,
               ManifestLocation::kExternalPrefDownload,
               extensions::Extension::NO_FLAGS);
  WaitTimeUpdated();

  extensions::ExtensionPrefs* const prefs =
      extensions::ExtensionPrefs::Get(profile());
  ASSERT_TRUE(prefs);

  // Simulate launching the normal app. Expect that an app with a recorded
  // launch time takes precedence over the default-installed apps.
  prefs->SetLastLaunchTime(normal_app_id, base::Time::Now());
  CreateSearch();
  EXPECT_EQ(
      std::string(kRankingNormalAppName) + ",OsSettings,Help,Canvas,Camera",
      RunQuery(""));

  // Simulate launching one of the default apps. Expect that this brings it to
  // higher precedence than all the others.
  prefs->SetLastLaunchTime(web_app::kCanvasAppId, base::Time::Now());
  CreateSearch();
  EXPECT_EQ("Canvas," + std::string(kRankingNormalAppName) +
                ",OsSettings,Help,Camera",
            RunQuery(""));
}

TEST_F(AppSearchProviderTest, FetchUnlaunchedRecommendations) {
  CreateSearch();

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile_.get());

  // The order of unlaunched recommendations should be based on the install time
  // order.
  prefs->SetLastLaunchTime(kHostedAppId, base::Time::Now());
  prefs->SetLastLaunchTime(kPackagedApp1Id, MicrosecondsSinceEpoch(0));
  prefs->SetLastLaunchTime(kPackagedApp2Id, MicrosecondsSinceEpoch(0));
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunQuery(""));
}

TEST_F(AppSearchProviderTest, FilterDuplicate) {
  arc_test().SetUp(profile());

  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(profile_.get());
  ASSERT_TRUE(extension_prefs);

  AddExtension(extension_misc::kGmailAppId, kGmailExtensionName,
               ManifestLocation::kExternalPrefDownload,
               extensions::Extension::NO_FLAGS);

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
      arc_gmail_app_info->last_launch_time - base::Seconds(1));

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  CreateSearch();
  EXPECT_EQ(kGmailArcName, RunQuery(kGmailQuery));

  extension_prefs->SetLastLaunchTime(
      extension_misc::kGmailAppId,
      arc_gmail_app_info->last_launch_time + base::Seconds(1));

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  CreateSearch();
  EXPECT_EQ(kGmailExtensionName, RunQuery(kGmailQuery));
  arc_test().TearDown();
}

TEST_F(AppSearchProviderTest, FetchInternalApp) {
  CreateSearch();

  // Search Keyboard Shortcut Helper.
  EXPECT_EQ(kKeyboardShortcutHelperInternalName, RunQuery("Keyboard"));
  EXPECT_EQ(kKeyboardShortcutHelperInternalName, RunQuery("Shortcut"));
  EXPECT_EQ(kKeyboardShortcutHelperInternalName, RunQuery("Helper"));
}

TEST_F(AppSearchProviderTest, WebApp) {
  apps::AppServiceProxyFactory::GetForProfile(testing_profile())
      ->FlushMojoCallsForTesting();

  const web_app::AppId app_id = web_app::test::InstallDummyWebApp(
      testing_profile(), kWebAppName, GURL(kWebAppUrl));

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  CreateSearch();
  EXPECT_EQ("WebApp1", RunQuery("WebA"));
}

class AppSearchProviderCrostiniTest : public AppSearchProviderTest {
 public:
  void SetUp() override {
    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
    AppSearchProviderTest::SetUp();
  }

  void TearDown() override {
    profile_.reset();
    AppSearchProviderTest::TearDown();

    // |profile_| is initialized in AppListTestBase::SetUp but not destroyed in
    // the ::TearDown method, but we need it to go away before shutting down
    // DBusThreadManager to ensure all keyed services that might rely on DBus
    // clients are destroyed.
    profile_.reset();
    ash::SeneschalClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }
};

TEST_F(AppSearchProviderCrostiniTest, CrostiniApp) {
  // This both allows Crostini UI and enables Crostini.
  crostini::CrostiniTestHelper crostini_test_helper(testing_profile());
  crostini_test_helper.ReInitializeAppServiceIntegration();
  CreateSearch();

  // Search based on keywords and name
  auto testApp = crostini_test_helper.BasicApp("goodApp");
  std::map<std::string, std::set<std::string>> keywords;
  keywords[""] = {"wow", "amazing", "excellent app"};
  crostini_test_helper.UpdateAppKeywords(testApp, keywords);
  testApp.set_executable_file_name("executable");
  crostini_test_helper.AddApp(testApp);

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("goodApp", RunQuery("wow"));
  EXPECT_EQ("goodApp", RunQuery("amazing"));
  EXPECT_EQ("goodApp", RunQuery("excellent app"));
  EXPECT_EQ("goodApp", RunQuery("good"));
  EXPECT_EQ("goodApp", RunQuery("executable"));
  EXPECT_EQ("goodApp", RunQuery("wow amazing"));
  EXPECT_EQ("", RunQuery("terrible"));
}

TEST_F(AppSearchProviderTest, AppServiceIconCache) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  ASSERT_NE(proxy, nullptr);

  apps::StubIconLoader stub_icon_loader;
  apps::IconLoader* old_icon_loader =
      proxy->OverrideInnerIconLoaderForTesting(&stub_icon_loader);

  // Insert dummy map values so that the stub_icon_loader knows of these apps.
  stub_icon_loader.timelines_by_app_id_[kPackagedApp1Id] = 1;
  stub_icon_loader.timelines_by_app_id_[kPackagedApp2Id] = 2;

  // The stub_icon_loader should start with no LoadIconFromIconKey calls.
  CreateSearch();
  EXPECT_EQ(0, stub_icon_loader.NumLoadIconFromIconKeyCalls());

  // Running the "pa" query should get two hits (for "Packaged App #"), which
  // should lead to 2 LoadIconFromIconKey calls on the stub_icon_loader.
  RunQuery("pa");
  EXPECT_EQ(2, stub_icon_loader.NumLoadIconFromIconKeyCalls());

  // Issuing the same "pa" query should hit the AppServiceDataSource's icon
  // cache, with no further calls to the wrapped stub_icon_loader.
  RunQuery("pa");
  EXPECT_EQ(2, stub_icon_loader.NumLoadIconFromIconKeyCalls());

  // The number of LoadIconFromIconKey calls should not change, when hiding the
  // UI (i.e. calling ViewClosing).
  CallViewClosing();

  EXPECT_FALSE(results().empty());
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();
  if (!app_list_features::IsCategoricalSearchEnabled()) {
    // Verify the search results are cleared async.
    EXPECT_TRUE(results().empty());
  }

  EXPECT_EQ(2, stub_icon_loader.NumLoadIconFromIconKeyCalls());

  // The icon has been added to the map, so issuing the same "pa" query should
  // not call the wrapped stub_icon_loader.
  RunQuery("pa");
  EXPECT_EQ(2, stub_icon_loader.NumLoadIconFromIconKeyCalls());

  // Update the icon key to remove the app icon from cache.
  UpdateIconKey(*proxy, kPackagedApp2Id);

  // The icon has been removed from the cache, so issuing the same "pa" query
  // should call the wrapped stub_icon_loader.
  RunQuery("pa");
  EXPECT_EQ(3, stub_icon_loader.NumLoadIconFromIconKeyCalls());

  proxy->OverrideInnerIconLoaderForTesting(old_icon_loader);
}

TEST_F(AppSearchProviderTest, FuzzyAppSearchTest) {
  CreateSearch();
  EXPECT_EQ("Packaged App 1,Packaged App 2", RunQuery("pa"));
  std::string result = RunQuery("ackaged");
  EXPECT_TRUE(result == "Packaged App 1,Packaged App 2" ||
              result == "Packaged App 2,Packaged App 1");
  EXPECT_EQ(kKeyboardShortcutHelperInternalName, RunQuery("Helper"));
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

  AppSearchProviderWithExtensionInstallType(
      const AppSearchProviderWithExtensionInstallType&) = delete;
  AppSearchProviderWithExtensionInstallType& operator=(
      const AppSearchProviderWithExtensionInstallType&) = delete;

  ~AppSearchProviderWithExtensionInstallType() override = default;
};

TEST_P(AppSearchProviderWithExtensionInstallType, OemResultsOnFirstBoot) {
  // Disable the pre-installed high-priority extensions. This test simulates
  // a brand new profile being added to a device, and should not include these.
  service_->UninstallExtension(
      kHostedAppId, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  service_->UninstallExtension(
      kPackagedApp1Id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  service_->UninstallExtension(
      kPackagedApp2Id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);

  base::RunLoop().RunUntilIdle();

  // OEM-installed apps should only appear as the first app results
  // if the profile is running for the first time on a device.
  profile_->SetIsNewProfile(true);
  ASSERT_TRUE(profile()->IsNewProfile());

  extensions::ExtensionPrefs* const prefs =
      extensions::ExtensionPrefs::Get(profile());
  ASSERT_TRUE(prefs);
  const char* kOemAppNames[] = {"OemExtension1", "OemExtension2",
                                "OemExtension3", "OemExtension4",
                                "OemExtension5"};

  for (auto* app_id : kOemAppNames) {
    const std::string internal_app_id = crx_file::id_util::GenerateId(app_id);

    AddExtension(internal_app_id, app_id,
                 ManifestLocation::kExternalPrefDownload,
                 extensions::Extension::WAS_INSTALLED_BY_OEM);

    service_->EnableExtension(internal_app_id);

    EXPECT_TRUE(prefs->WasInstalledByOem(internal_app_id));
  }

  // Allow OEM app install to finish.
  base::RunLoop().RunUntilIdle();
  CreateSearch();

  std::vector<std::string> results = base::SplitString(
      RunQuery(""), ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (auto* app : kOemAppNames) {
    EXPECT_TRUE(base::Contains(results, app));
  }
}

enum class TestArcAppInstallType {
  CONTROLLED_BY_POLICY,
  INSTALLED_BY_DEFAULT,
};

class AppSearchProviderWithArcAppInstallType
    : public AppSearchProviderTest,
      public ::testing::WithParamInterface<TestArcAppInstallType> {
 public:
  AppSearchProviderWithArcAppInstallType() = default;

  AppSearchProviderWithArcAppInstallType(
      const AppSearchProviderWithArcAppInstallType&) = delete;
  AppSearchProviderWithArcAppInstallType& operator=(
      const AppSearchProviderWithArcAppInstallType&) = delete;

  ~AppSearchProviderWithArcAppInstallType() override = default;
};

// TODO (879413): Enable this after resolving flakiness.
TEST_P(AppSearchProviderWithArcAppInstallType,
       DISABLED_InstallInternallyRanking) {
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

  // Installed internally app has runking below other apps, even if its install
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
  arc_test().TearDown();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AppSearchProviderWithExtensionInstallType,
    ::testing::ValuesIn({TestExtensionInstallType::CONTROLLED_BY_POLICY,
                         TestExtensionInstallType::CHROME_COMPONENT,
                         TestExtensionInstallType::INSTALLED_BY_DEFAULT,
                         TestExtensionInstallType::INSTALLED_BY_OEM}));

INSTANTIATE_TEST_SUITE_P(
    All,
    AppSearchProviderWithArcAppInstallType,
    ::testing::ValuesIn({TestArcAppInstallType::CONTROLLED_BY_POLICY,
                         TestArcAppInstallType::INSTALLED_BY_DEFAULT}));

}  // namespace test
}  // namespace app_list
