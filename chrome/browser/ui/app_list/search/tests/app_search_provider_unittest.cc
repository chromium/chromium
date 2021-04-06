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
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_default_app_list.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_search_result_ranker.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/crx_file/id_util.h"
#include "components/services/app_service/public/cpp/stub_icon_loader.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/sessions/content/content_test_helper.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_sessions/mock_sync_sessions_client.h"
#include "components/sync_sessions/open_tabs_ui_delegate_impl.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "components/sync_sessions/synced_session_tracker.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
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
      FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromMilliseconds(1));
  run_loop.Run();
}

}  // namespace

const base::Time kTestCurrentTime = base::Time::FromInternalValue(100000);

bool MoreRelevant(const ChromeSearchResult* result1,
                  const ChromeSearchResult* result2) {
  return result1->relevance() > result2->relevance();
}

void UpdateIconKey(apps::AppServiceProxyChromeOs& proxy,
                   const std::string& app_id) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_id = app_id;
  proxy.AppRegistryCache().ForOneApp(
      app_id, [&app](const apps::AppUpdate& update) {
        app->app_type = update.AppType();
        app->icon_key = apps::mojom::IconKey::New(
            update.IconKey()->timeline + 1, update.IconKey()->resource_id,
            update.IconKey()->icon_effects);
      });

  std::vector<apps::mojom::AppPtr> apps;
  apps.push_back(app.Clone());
  proxy.AppRegistryCache().OnApps(std::move(apps),
                                  apps::mojom::AppType::kUnknown,
                                  false /* should_notify_initialized */);
  proxy.FlushMojoCallsForTesting();
}

class AppSearchProviderTest : public AppListTestBase {
 public:
  AppSearchProviderTest() {
    // TODO(crbug.com/990684): disable FuzzyAppSearch because we flipped the
    // flag to be enabled by default, need to enable it after it is fully
    // launched.
    scoped_feature_list_.InitWithFeatures(
        {}, {app_list_features::kEnableFuzzyAppSearch});
  }
  ~AppSearchProviderTest() override {}

  // AppListTestBase overrides:
  void SetUp() override {
    AppListTestBase::SetUp();

    model_updater_ = std::make_unique<FakeAppListModelUpdater>();
    controller_ = std::make_unique<::test::TestAppListControllerDelegate>();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void CreateSearch() {
    clock_.SetNow(kTestCurrentTime);
    app_search_ = std::make_unique<AppSearchProvider>(
        profile_.get(), nullptr, &clock_, model_updater_.get());
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
    app_search_->Start(base::UTF8ToUTF16(query));

    // Sort results by relevance.
    std::vector<ChromeSearchResult*> sorted_results;
    for (const auto& result : app_search_->results())
      sorted_results.emplace_back(result.get());
    std::sort(sorted_results.begin(), sorted_results.end(), &MoreRelevant);

    // If the query is empty, every other result is a chip result identical to
    // the tile result. Skip these.
    const int increment = query.empty() ? 2 : 1;
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
    app_search_->Start(base::UTF8ToUTF16(query));

    std::vector<ChromeSearchResult*> non_relevance_results;
    std::vector<ChromeSearchResult*> priority_results;
    for (const auto& result : app_search_->results()) {
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

  const SearchProvider::Results& results() { return app_search_->results(); }
  ArcAppTest& arc_test() { return arc_test_; }

  void CallViewClosing() { app_search_->ViewClosing(); }

  sync_sessions::SyncedSessionTracker* session_tracker() {
    return session_tracker_.get();
  }

 private:
  base::SimpleTestClock clock_;
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  std::unique_ptr<AppSearchProvider> app_search_;
  std::unique_ptr<::test::TestAppListControllerDelegate> controller_;
  ArcAppTest arc_test_;

  // For continue reading.
  testing::NiceMock<sync_sessions::MockSyncSessionsClient>
      mock_sync_sessions_client_;
  std::unique_ptr<sync_sessions::SyncedSessionTracker> session_tracker_;
  std::unique_ptr<sync_sessions::OpenTabsUIDelegateImpl> open_tabs_ui_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppSearchProviderTest);
};

TEST_F(AppSearchProviderTest, Basic) {
  arc_test().SetUp(profile());
  std::vector<arc::mojom::AppInfo> arc_apps(arc_test().fake_apps().begin(),
                                            arc_test().fake_apps().begin() + 2);
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
  std::vector<arc::mojom::AppInfo> arc_apps;
  arc_test().app_instance()->SendRefreshAppList(arc_apps);

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  CreateSearch();

  EXPECT_TRUE(results().empty());
  EXPECT_EQ("", RunQuery("fapp0"));

  arc_apps.push_back(arc_test().fake_apps()[0]);
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
}

TEST_F(AppSearchProviderTest, FetchRecommendations) {
  CreateSearch();

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile_.get());

  prefs->SetLastLaunchTime(kHostedAppId, base::Time::FromInternalValue(20));
  prefs->SetLastLaunchTime(kPackagedApp1Id, base::Time::FromInternalValue(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, base::Time::FromInternalValue(5));
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunQuery(""));

  prefs->SetLastLaunchTime(kHostedAppId, base::Time::FromInternalValue(5));
  prefs->SetLastLaunchTime(kPackagedApp1Id, base::Time::FromInternalValue(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, base::Time::FromInternalValue(20));
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("Packaged App 2,Packaged App 1,Hosted App", RunQuery(""));

  // Times in the future should just be handled as highest priority.
  prefs->SetLastLaunchTime(kHostedAppId,
                           kTestCurrentTime + base::TimeDelta::FromSeconds(5));
  prefs->SetLastLaunchTime(kPackagedApp1Id, base::Time::FromInternalValue(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, base::Time::FromInternalValue(5));
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunQuery(""));
}

TEST_F(AppSearchProviderTest, FetchRecommendationsWithContinueReading) {
  constexpr char kLocalSessionTag[] = "local";
  constexpr char kLocalSessionName[] = "LocalSessionName";
  constexpr char kForeignSessionTag1[] = "foreign1";
  constexpr char kForeignSessionTag2[] = "foreign2";
  constexpr char kForeignSessionTag3[] = "foreign3";
  constexpr SessionID kWindowId1 = SessionID::FromSerializedValue(1);
  constexpr SessionID kWindowId2 = SessionID::FromSerializedValue(2);
  constexpr SessionID kWindowId3 = SessionID::FromSerializedValue(3);
  constexpr SessionID kTabId1 = SessionID::FromSerializedValue(111);
  constexpr SessionID kTabId2 = SessionID::FromSerializedValue(222);
  constexpr SessionID kTabId3 = SessionID::FromSerializedValue(333);

  const base::Time now = base::Time::Now();

  // Case 1: test that ContinueReading is recommended for the latest foreign
  // tab.
  {
    CreateSearchWithContinueReading();
    session_tracker()->InitLocalSession(kLocalSessionTag, kLocalSessionName,
                                        sync_pb::SyncEnums::TYPE_CROS);
    const base::Time kTimestamp1 = now - base::TimeDelta::FromMinutes(2);
    const base::Time kTimestamp2 = now - base::TimeDelta::FromMinutes(1);
    const base::Time kTimestamp3 = now - base::TimeDelta::FromMinutes(3);

    session_tracker()->PutWindowInSession(kForeignSessionTag1, kWindowId1);
    session_tracker()->PutTabInWindow(kForeignSessionTag1, kWindowId1, kTabId1);
    session_tracker()
        ->GetTab(kForeignSessionTag1, kTabId1)
        ->navigations.push_back(sessions::ContentTestHelper::CreateNavigation(
            "http://url1", "title1"));
    session_tracker()->GetTab(kForeignSessionTag1, kTabId1)->timestamp =
        kTimestamp1;
    session_tracker()->GetSession(kForeignSessionTag1)->modified_time =
        kTimestamp1;
    session_tracker()->GetSession(kForeignSessionTag1)->device_type =
        sync_pb::SyncEnums::TYPE_PHONE;

    session_tracker()->PutWindowInSession(kForeignSessionTag2, kWindowId2);
    session_tracker()->PutTabInWindow(kForeignSessionTag2, kWindowId2, kTabId2);
    session_tracker()
        ->GetTab(kForeignSessionTag2, kTabId2)
        ->navigations.push_back(sessions::ContentTestHelper::CreateNavigation(
            "http://url2", "title2"));
    session_tracker()->GetTab(kForeignSessionTag2, kTabId2)->timestamp =
        kTimestamp2;
    session_tracker()->GetSession(kForeignSessionTag2)->modified_time =
        kTimestamp2;
    session_tracker()->GetSession(kForeignSessionTag2)->device_type =
        sync_pb::SyncEnums::TYPE_PHONE;

    session_tracker()->PutWindowInSession(kForeignSessionTag3, kWindowId3);
    session_tracker()->PutTabInWindow(kForeignSessionTag3, kWindowId3, kTabId3);
    session_tracker()
        ->GetTab(kForeignSessionTag3, kTabId3)
        ->navigations.push_back(sessions::ContentTestHelper::CreateNavigation(
            "http://url3", "title3"));
    session_tracker()->GetTab(kForeignSessionTag3, kTabId3)->timestamp =
        kTimestamp3;
    session_tracker()->GetSession(kForeignSessionTag3)->modified_time =
        kTimestamp3;
    session_tracker()->GetSession(kForeignSessionTag3)->device_type =
        sync_pb::SyncEnums::TYPE_PHONE;

    EXPECT_EQ("title2,Hosted App,Packaged App 1,Packaged App 2",
              RunQueryNotSortingByRelevance(""));
  }

  // Case 2: test that ContinueReading is not recommended for local session.
  {
    CreateSearchWithContinueReading();
    session_tracker()->InitLocalSession(kLocalSessionTag, kLocalSessionName,
                                        sync_pb::SyncEnums::TYPE_CROS);
    const base::Time kTimestamp1 = now - base::TimeDelta::FromMinutes(1);

    session_tracker()->PutWindowInSession(kLocalSessionTag, kWindowId1);
    session_tracker()->PutTabInWindow(kLocalSessionTag, kWindowId1, kTabId1);
    session_tracker()
        ->GetTab(kLocalSessionTag, kTabId1)
        ->navigations.push_back(sessions::ContentTestHelper::CreateNavigation(
            "http://url1", "title1"));
    session_tracker()->GetTab(kLocalSessionTag, kTabId1)->timestamp =
        kTimestamp1;
    session_tracker()->GetSession(kLocalSessionTag)->modified_time =
        kTimestamp1;
    session_tracker()->GetSession(kLocalSessionTag)->device_type =
        sync_pb::SyncEnums::TYPE_PHONE;

    EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2",
              RunQueryNotSortingByRelevance(""));
  }

  // Case 3: test that ContinueReading is not recommended for foreign tab more
  // than 120 minutes ago.
  {
    CreateSearchWithContinueReading();
    session_tracker()->InitLocalSession(kLocalSessionTag, kLocalSessionName,
                                        sync_pb::SyncEnums::TYPE_CROS);
    const base::Time kTimestamp1 = now - base::TimeDelta::FromMinutes(121);

    session_tracker()->PutWindowInSession(kForeignSessionTag1, kWindowId1);
    session_tracker()->PutTabInWindow(kForeignSessionTag1, kWindowId1, kTabId1);
    session_tracker()
        ->GetTab(kForeignSessionTag1, kTabId1)
        ->navigations.push_back(sessions::ContentTestHelper::CreateNavigation(
            "http://url1", "title1"));
    session_tracker()->GetTab(kForeignSessionTag1, kTabId1)->timestamp =
        kTimestamp1;
    session_tracker()->GetSession(kForeignSessionTag1)->modified_time =
        kTimestamp1;
    session_tracker()->GetSession(kForeignSessionTag1)->device_type =
        sync_pb::SyncEnums::TYPE_PHONE;

    EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2",
              RunQueryNotSortingByRelevance(""));
  }

  // Case 4: test that ContinueReading is recommended for foreign tab with
  // TYPE_TABLET.
  {
    CreateSearchWithContinueReading();
    session_tracker()->InitLocalSession(kLocalSessionTag, kLocalSessionName,
                                        sync_pb::SyncEnums::TYPE_CROS);
    const base::Time kTimestamp1 = now - base::TimeDelta::FromMinutes(1);

    session_tracker()->PutWindowInSession(kForeignSessionTag1, kWindowId1);
    session_tracker()->PutTabInWindow(kForeignSessionTag1, kWindowId1, kTabId1);
    session_tracker()
        ->GetTab(kForeignSessionTag1, kTabId1)
        ->navigations.push_back(sessions::ContentTestHelper::CreateNavigation(
            "http://url1", "title1"));
    session_tracker()->GetTab(kForeignSessionTag1, kTabId1)->timestamp =
        kTimestamp1;
    session_tracker()->GetSession(kForeignSessionTag1)->modified_time =
        kTimestamp1;
    session_tracker()->GetSession(kForeignSessionTag1)->device_type =
        sync_pb::SyncEnums::TYPE_TABLET;

    EXPECT_EQ("title1,Hosted App,Packaged App 1,Packaged App 2",
              RunQueryNotSortingByRelevance(""));
  }

  // Case 5: test that ContinueReading is not recommended for foreign tab with
  // TYPE_CROS.
  {
    CreateSearchWithContinueReading();
    session_tracker()->InitLocalSession(kLocalSessionTag, kLocalSessionName,
                                        sync_pb::SyncEnums::TYPE_CROS);
    const base::Time kTimestamp1 = now - base::TimeDelta::FromMinutes(1);

    session_tracker()->PutWindowInSession(kForeignSessionTag1, kWindowId1);
    session_tracker()->PutTabInWindow(kForeignSessionTag1, kWindowId1, kTabId1);
    session_tracker()
        ->GetTab(kForeignSessionTag1, kTabId1)
        ->navigations.push_back(sessions::ContentTestHelper::CreateNavigation(
            "http://url1", "title1"));
    session_tracker()->GetTab(kForeignSessionTag1, kTabId1)->timestamp =
        kTimestamp1;
    session_tracker()->GetSession(kForeignSessionTag1)->modified_time =
        kTimestamp1;
    session_tracker()->GetSession(kForeignSessionTag1)->device_type =
        sync_pb::SyncEnums::TYPE_CROS;

    EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2",
              RunQueryNotSortingByRelevance(""));
  }

  // Case 6: test that ContinueReading is not recommended for foreign tab which
  // is not SchemeIsHTTPOrHTTPS.
  {
    CreateSearchWithContinueReading();
    session_tracker()->InitLocalSession(kLocalSessionTag, kLocalSessionName,
                                        sync_pb::SyncEnums::TYPE_CROS);
    const base::Time kTimestamp1 = now - base::TimeDelta::FromMinutes(1);

    session_tracker()->PutWindowInSession(kForeignSessionTag1, kWindowId1);
    session_tracker()->PutTabInWindow(kForeignSessionTag1, kWindowId1, kTabId1);
    session_tracker()
        ->GetTab(kForeignSessionTag1, kTabId1)
        ->navigations.push_back(sessions::ContentTestHelper::CreateNavigation(
            "data://url1", "title1"));
    session_tracker()->GetTab(kForeignSessionTag1, kTabId1)->timestamp =
        kTimestamp1;
    session_tracker()->GetSession(kForeignSessionTag1)->modified_time =
        kTimestamp1;
    session_tracker()->GetSession(kForeignSessionTag1)->device_type =
        sync_pb::SyncEnums::TYPE_CROS;

    EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2",
              RunQueryNotSortingByRelevance(""));
  }

  // Case 7: test that ContinueReading is not recommended when searching.
  {
    CreateSearchWithContinueReading();
    session_tracker()->InitLocalSession(kLocalSessionTag, kLocalSessionName,
                                        sync_pb::SyncEnums::TYPE_CROS);
    const base::Time kTimestamp1 = now - base::TimeDelta::FromMinutes(1);

    session_tracker()->PutWindowInSession(kForeignSessionTag1, kWindowId1);
    session_tracker()->PutTabInWindow(kForeignSessionTag1, kWindowId1, kTabId1);
    session_tracker()
        ->GetTab(kForeignSessionTag1, kTabId1)
        ->navigations.push_back(sessions::ContentTestHelper::CreateNavigation(
            "http://url1", "title1"));
    session_tracker()->GetTab(kForeignSessionTag1, kTabId1)->timestamp =
        kTimestamp1;
    session_tracker()->GetSession(kForeignSessionTag1)->modified_time =
        kTimestamp1;
    session_tracker()->GetSession(kForeignSessionTag1)->device_type =
        sync_pb::SyncEnums::TYPE_PHONE;
    EXPECT_EQ("", RunQueryNotSortingByRelevance("ti"));
  }
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
      arc_gmail_app_info->last_launch_time - base::TimeDelta::FromSeconds(1));

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  CreateSearch();
  EXPECT_EQ(kGmailArcName, RunQuery(kGmailQuery));

  extension_prefs->SetLastLaunchTime(
      extension_misc::kGmailAppId,
      arc_gmail_app_info->last_launch_time + base::TimeDelta::FromSeconds(1));

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  CreateSearch();
  EXPECT_EQ(kGmailExtensionName, RunQuery(kGmailQuery));
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
    chromeos::DBusThreadManager::Initialize();
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
    chromeos::DBusThreadManager::Shutdown();
  }
};

TEST_F(AppSearchProviderCrostiniTest, CrostiniTerminal) {
  CreateSearch();

  // Crostini UI is not allowed yet.
  EXPECT_EQ("", RunQuery("terminal"));
  EXPECT_EQ("", RunQuery("linux"));

  // This both allows Crostini UI and enables Crostini.
  crostini::CrostiniTestHelper crostini_test_helper(testing_profile());
  crostini_test_helper.ReInitializeAppServiceIntegration();
  CreateSearch();
  EXPECT_EQ("Terminal,Hosted App", RunQuery("te"));
  EXPECT_EQ("Terminal", RunQuery("ter"));
  EXPECT_EQ("Terminal", RunQuery("terminal"));
  EXPECT_EQ("Terminal", RunQuery("li"));
  EXPECT_EQ("Terminal", RunQuery("linux"));
  EXPECT_EQ("Terminal", RunQuery("crosti"));

  // If Crostini UI is allowed but disabled (i.e. not installed), a match score
  // of 0.8 is required before surfacing search results.
  crostini::CrostiniTestHelper::DisableCrostini(testing_profile());
  CreateSearch();
  EXPECT_EQ("Hosted App", RunQuery("te"));
  EXPECT_EQ("Terminal", RunQuery("ter"));
  EXPECT_EQ("Terminal", RunQuery("terminal"));
  EXPECT_EQ("", RunQuery("li"));
  EXPECT_EQ("Terminal", RunQuery("lin"));
  EXPECT_EQ("Terminal", RunQuery("linux"));
  EXPECT_EQ("", RunQuery("cr"));
  EXPECT_EQ("Terminal", RunQuery("cro"));
  EXPECT_EQ("Terminal", RunQuery("cros"));
}

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
  EXPECT_EQ("", RunQuery("wow amazing"));
  EXPECT_EQ("", RunQuery("terrible"));
}

TEST_F(AppSearchProviderTest, AppServiceIconCache) {
  apps::AppServiceProxyChromeOs* proxy =
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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(app_list_features::kEnableFuzzyAppSearch);
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
  ~AppSearchProviderWithExtensionInstallType() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppSearchProviderWithExtensionInstallType);
};

TEST_P(AppSearchProviderWithExtensionInstallType, InstallInternallyRanking) {
  extensions::ExtensionPrefs* const prefs =
      extensions::ExtensionPrefs::Get(profile());
  ASSERT_TRUE(prefs);

  // Install normal app.
  const std::string normal_app_id =
      crx_file::id_util::GenerateId(kRankingNormalAppName);
  AddExtension(normal_app_id, kRankingNormalAppName,
               ManifestLocation::kExternalPrefDownload,
               extensions::Extension::NO_FLAGS);

  // Wait a bit to make sure time is updated.
  WaitTimeUpdated();

  // Install app internally.
  const std::string internal_app_id =
      crx_file::id_util::GenerateId(kRankingInternalAppName);
  switch (GetParam()) {
    case TestExtensionInstallType::CONTROLLED_BY_POLICY:
      AddExtension(internal_app_id, kRankingInternalAppName,
                   ManifestLocation::kExternalPolicyDownload,
                   extensions::Extension::NO_FLAGS);
      break;
    case TestExtensionInstallType::CHROME_COMPONENT:
      AddExtension(internal_app_id, kRankingInternalAppName,
                   ManifestLocation::kComponent,
                   extensions::Extension::NO_FLAGS);
      break;
    case TestExtensionInstallType::INSTALLED_BY_DEFAULT:
      AddExtension(internal_app_id, kRankingInternalAppName,
                   ManifestLocation::kExternalPrefDownload,
                   extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
      break;
    case TestExtensionInstallType::INSTALLED_BY_OEM:
      AddExtension(internal_app_id, kRankingInternalAppName,
                   ManifestLocation::kExternalPrefDownload,
                   extensions::Extension::WAS_INSTALLED_BY_OEM);
      break;
  }

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_LT(prefs->GetInstallTime(normal_app_id),
            prefs->GetInstallTime(internal_app_id));

  // Installed internally app has runking below other apps, even if it's install
  // time is later.
  CreateSearch();
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::string(kRankingNormalAppName) + "," +
                std::string(kRankingInternalAppName),
            RunQuery(kRankingAppQuery));

  // Using installed internally app moves it in ranking up.
  WaitTimeUpdated();
  prefs->SetLastLaunchTime(internal_app_id, base::Time::Now());
  CreateSearch();
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::string(kRankingInternalAppName) + "," +
                std::string(kRankingNormalAppName),
            RunQuery(kRankingAppQuery));
}

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
