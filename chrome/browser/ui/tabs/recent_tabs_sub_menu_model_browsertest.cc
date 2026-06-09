// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/recent_tabs_sub_menu_model.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/recent_tabs_builder_test_helper.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/menu_model_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/content_test_helper.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service_impl.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/mock_commit_queue.h"
#include "components/sync_sessions/session_sync_service_impl.h"
#include "components/sync_sessions/synced_session.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace {

class TestRecentTabsMenuModelDelegate : public ui::MenuModelDelegate {
 public:
  explicit TestRecentTabsMenuModelDelegate(ui::MenuModel* model)
      : model_(model) {
    model_->SetMenuModelDelegate(this);
  }

  TestRecentTabsMenuModelDelegate(const TestRecentTabsMenuModelDelegate&) =
      delete;
  TestRecentTabsMenuModelDelegate& operator=(
      const TestRecentTabsMenuModelDelegate&) = delete;

  ~TestRecentTabsMenuModelDelegate() override {
    model_->SetMenuModelDelegate(nullptr);
  }

  // ui::MenuModelDelegate implementation:

  void OnIconChanged(int index) override {}

  void OnMenuStructureChanged() override { got_changes_ = true; }

  bool got_changes() const { return got_changes_; }

 private:
  raw_ptr<ui::MenuModel> model_;
  bool got_changes_ = false;
};

struct ModelData {
  ui::MenuModel::ItemType type;
  bool enabled;
};

void VerifyModel(const ui::MenuModel& model, base::span<const ModelData> data) {
  ASSERT_EQ(data.size(), model.GetItemCount());
  for (size_t i = 0; i < data.size(); ++i) {
    SCOPED_TRACE(i);
    const ui::MenuModel::ItemType type = model.GetTypeAt(i);
    EXPECT_EQ(data[i].type, type);
    EXPECT_EQ(data[i].enabled, model.IsEnabledAt(i));
    EXPECT_EQ(type == ui::MenuModel::TYPE_TITLE, !!model.GetLabelFontListAt(i));
  }
}

void VerifyModel(const ui::MenuModel* model, base::span<const ModelData> data) {
  ASSERT_TRUE(model);
  VerifyModel(*model, std::move(data));
}

}  // namespace

class RecentTabsSubMenuModelTest : public InProcessBrowserTest {
 public:
  RecentTabsSubMenuModelTest() = default;
  RecentTabsSubMenuModelTest(const RecentTabsSubMenuModelTest&) = delete;
  RecentTabsSubMenuModelTest& operator=(const RecentTabsSubMenuModelTest&) =
      delete;
  ~RecentTabsSubMenuModelTest() override = default;

  void WaitForLoadFromLastSession() { content::RunAllTasksUntilIdle(); }

  virtual void Init() {
    auto* session_sync_service =
        SessionSyncServiceFactory::GetForProfile(browser()->profile());

    syncer::DataTypeActivationRequest activation_request;
    activation_request.cache_guid = "test_cache_guid";
    activation_request.error_handler = base::DoNothing();

    std::unique_ptr<syncer::DataTypeActivationResponse> activation_response;
    base::RunLoop loop;
    session_sync_service->GetControllerDelegate()->OnSyncStarting(
        activation_request,
        base::BindLambdaForTesting(
            [&](std::unique_ptr<syncer::DataTypeActivationResponse> response) {
              activation_response = std::move(response);
              loop.Quit();
            }));
    loop.Run();
    ASSERT_NE(nullptr, activation_response);
    ASSERT_NE(nullptr, activation_response->type_processor);
    sync_processor_ = std::move(activation_response->type_processor);

    EnableSync();
  }

  void EnableSync() {
    // ClientTagBasedDataTypeProcessor requires connecting before
    // other interactions with the worker happen.
    sync_processor_->ConnectSync(
        std::make_unique<testing::NiceMock<syncer::MockCommitQueue>>());
  }

  void DisableSync() { sync_processor_->DisconnectSync(); }

  static std::unique_ptr<KeyedService> GetTabRestoreService(
      os_crypt_async::OSCryptAsync* os_crypt_async,
      content::BrowserContext* browser_context) {
    return std::make_unique<sessions::TabRestoreServiceImpl>(
        base::WrapUnique(new ChromeTabRestoreServiceClient(
            Profile::FromBrowserContext(browser_context))),
        nullptr, nullptr, os_crypt_async);
  }

  void RegisterRecentTabs(RecentTabsBuilderTestHelper* helper) {
    helper->ExportToSessionSync(sync_processor_.get());
    helper->VerifyExport(
        SessionSyncServiceFactory::GetForProfile(browser()->profile())
            ->GetOpenTabsUIDelegate());
  }

  void AddTabToBrowser(const GURL& tab_url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), tab_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  Browser* AddBrowser(Browser* browser, const GURL& url) {
    ui_test_utils::BrowserCreatedObserver browser_created_observer;
    ui_test_utils::NavigateToURLWithDisposition(
        browser, url, WindowOpenDisposition::NEW_WINDOW,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
    Browser* new_browser = browser_created_observer.Wait();
    ui_test_utils::WaitUntilBrowserBecomeActive(new_browser);
    return new_browser;
  }

 private:
  std::unique_ptr<syncer::DataTypeProcessor> sync_processor_;
};

class TestLogMetricsAppMenuModel : public AppMenuModel {
 public:
  using AppMenuModel::AppMenuModel;

  void LogMenuAction(AppMenuAction action_id) override {
    ++log_metrics_call_count_;
  }

  void CallLogMenuMetrics(int command_id) { LogMenuMetrics(command_id); }

  int log_metrics_call_count() const { return log_metrics_call_count_; }

 private:
  int log_metrics_call_count_ = 0;
};

class FakeIconDelegate : public AppMenuIconController::Delegate {
 public:
  void UpdateTypeAndSeverity(
      AppMenuIconController::TypeAndSeverity type_and_severity) override {}
};

IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelTest,
                       LogMenuMetricsForShowHistory) {
  Init();
  FakeIconDelegate fake_delegate;
  AppMenuIconController app_menu_icon_controller(browser()->profile(),
                                                 &fake_delegate);
  TestLogMetricsAppMenuModel app_menu_model(nullptr, browser(),
                                            &app_menu_icon_controller);
  app_menu_model.Init();
  RecentTabsSubMenuModel recent_tab_sub_menu_model(nullptr, browser());
  recent_tab_sub_menu_model.RegisterLogMenuMetricsCallback(
      base::BindRepeating(&TestLogMetricsAppMenuModel::CallLogMenuMetrics,
                          base::Unretained(&app_menu_model)));
  recent_tab_sub_menu_model.ExecuteCommand(IDC_SHOW_HISTORY, 0);
  EXPECT_EQ(1, app_menu_model.log_metrics_call_count());
}

IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelTest,
                       LogMenuMetricsForShowGroupedHistory) {
  Init();
  FakeIconDelegate fake_delegate;
  AppMenuIconController app_menu_icon_controller(browser()->profile(),
                                                 &fake_delegate);
  TestLogMetricsAppMenuModel app_menu_model(nullptr, browser(),
                                            &app_menu_icon_controller);
  app_menu_model.Init();
  RecentTabsSubMenuModel recent_tab_sub_menu_model(nullptr, browser());
  recent_tab_sub_menu_model.RegisterLogMenuMetricsCallback(
      base::BindRepeating(&TestLogMetricsAppMenuModel::CallLogMenuMetrics,
                          base::Unretained(&app_menu_model)));
  recent_tab_sub_menu_model.ExecuteCommand(IDC_SHOW_HISTORY_CLUSTERS_SIDE_PANEL,
                                           0);
  EXPECT_EQ(1, app_menu_model.log_metrics_call_count());
}

IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelTest,
                       LogMenuMetricsForRecentTabsLoginForDeviceTabs) {
  Init();
  FakeIconDelegate fake_delegate;
  AppMenuIconController app_menu_icon_controller(browser()->profile(),
                                                 &fake_delegate);
  TestLogMetricsAppMenuModel app_menu_model(nullptr, browser(),
                                            &app_menu_icon_controller);
  app_menu_model.Init();
  RecentTabsSubMenuModel recent_tab_sub_menu_model(nullptr, browser());
  recent_tab_sub_menu_model.RegisterLogMenuMetricsCallback(
      base::BindRepeating(&TestLogMetricsAppMenuModel::CallLogMenuMetrics,
                          base::Unretained(&app_menu_model)));
  recent_tab_sub_menu_model.ExecuteCommand(
      IDC_RECENT_TABS_LOGIN_FOR_DEVICE_TABS, 0);
  EXPECT_EQ(1, app_menu_model.log_metrics_call_count());

  // Check that we arrive at the expected page.
  EXPECT_EQ(GURL(chrome::kChromeUISettingsURL).Resolve(chrome::kPeopleSubPage),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelTest,
                       LogMenuMetricsForRecentTabsSeeDeviceTabs) {
  Init();
  FakeIconDelegate fake_delegate;
  AppMenuIconController app_menu_icon_controller(browser()->profile(),
                                                 &fake_delegate);
  TestLogMetricsAppMenuModel app_menu_model(nullptr, browser(),
                                            &app_menu_icon_controller);
  app_menu_model.Init();
  RecentTabsSubMenuModel recent_tab_sub_menu_model(nullptr, browser());
  recent_tab_sub_menu_model.RegisterLogMenuMetricsCallback(
      base::BindRepeating(&TestLogMetricsAppMenuModel::CallLogMenuMetrics,
                          base::Unretained(&app_menu_model)));
  recent_tab_sub_menu_model.ExecuteCommand(IDC_RECENT_TABS_SEE_DEVICE_TABS, 0);
  EXPECT_EQ(1, app_menu_model.log_metrics_call_count());

  // Check that we arrive at the expected page.
  EXPECT_EQ(GURL(chrome::kChromeUIHistoryURL)
                .Resolve(chrome::kChromeUIHistorySyncedTabs),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Test disabled "Recently closed" header with no foreign tabs.
IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelTest, NoTabs) {
  Init();
  DisableSync();

  RecentTabsSubMenuModel model(nullptr, browser());

  std::vector<ModelData> kData;
  kData = {
      {ui::MenuModel::TYPE_COMMAND, true},    // History
      {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, false},   // Recently closed
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Your devices
      {ui::MenuModel::TYPE_COMMAND, true},    // See tabs from other devices

  };

  VerifyModel(model, kData);
}

// Test enabled "Recently closed" header with no foreign tabs.
IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelTest,
                       RecentlyClosedTabsFromCurrentSession) {
  Init();
  DisableSync();

  TabRestoreServiceFactory::GetForProfile(browser()->profile());

  // Add 2 tabs and close them.
  content::WebContents* tab1 =
      chrome::AddAndReturnTabAt(browser(), GURL("http://foo/1"), 0, true);
  content::WebContents* tab2 =
      chrome::AddAndReturnTabAt(browser(), GURL("http://foo/2"), 1, true);

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("http://foo/0"), 1);

  chrome::CloseWebContents(browser(), tab1, true);
  chrome::CloseWebContents(browser(), tab2, true);

  RecentTabsSubMenuModel model(nullptr, browser());

  std::vector<ModelData> kData;
  EXPECT_TRUE(browser()->GetFeatures().side_panel_ui());
  kData = {
      {ui::MenuModel::TYPE_COMMAND, true},    // History
      {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Recently closed
      {ui::MenuModel::TYPE_COMMAND, true},    // <tab for http://foo/2>
      {ui::MenuModel::TYPE_COMMAND, true},    // <tab for http://foo/1>
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Your devices
      {ui::MenuModel::TYPE_COMMAND, true}     // recent tabs login
  };

  VerifyModel(model, kData);
}

// Test recently closed groups with no foreign tabs.
IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelTest,
                       RecentlyClosedGroupsFromCurrentSession) {
  Init();
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  DisableSync();

  TabRestoreServiceFactory::GetForProfile(browser()->profile());

  AddTabToBrowser(GURL("http://foo/1"));
  AddTabToBrowser(GURL("http://foo/2"));
  AddTabToBrowser(GURL("http://foo/3"));
  tab_groups::TabGroupId group1 =
      browser()->tab_strip_model()->AddToNewGroup({0});
  tab_groups::TabGroupId group2 =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});
  browser()->tab_strip_model()->CloseAllTabsInGroup(group1);
  browser()->tab_strip_model()->CloseAllTabsInGroup(group2);

  RecentTabsSubMenuModel model(nullptr, browser());

  std::vector<ModelData> kData;

  kData = {
      {ui::MenuModel::TYPE_COMMAND, true},    // History
      {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Recently closed
      {ui::MenuModel::TYPE_SUBMENU, true},    // <group 1>
      {ui::MenuModel::TYPE_SUBMENU, true},    // <group 0>
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Your devices
      {ui::MenuModel::TYPE_COMMAND, true}     // recent tabs login
  };

  VerifyModel(model, kData);

  // Expected group 1 menu items:
  constexpr ModelData kGroup1Data[] = {
      {ui::MenuModel::TYPE_COMMAND, true},    // Restore group
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, true},    // <tab for http://foo/2>
      {ui::MenuModel::TYPE_COMMAND, true},    // <tab for http://foo/3>
  };

  VerifyModel(model.GetSubmenuModelAt(4), kGroup1Data);

  // Expected group 0 menu items:
  constexpr ModelData kGroup0Data[] = {
      {ui::MenuModel::TYPE_COMMAND, true},    // Restore group
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, true},    // <tab for http://foo/1>
  };

  VerifyModel(model.GetSubmenuModelAt(5), kGroup0Data);
}

class RecentTabsSubMenuModelSplitTest : public RecentTabsSubMenuModelTest {
 public:
  RecentTabsSubMenuModelSplitTest() {
    scoped_feature_list_.InitAndEnableFeature(tabs::kSplitViewTabRestore);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelSplitTest,
                       RecentlyClosedSplitsFromCurrentSession) {
  Init();
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  DisableSync();

  TabRestoreServiceFactory::GetForProfile(browser()->profile());

  AddTabToBrowser(GURL("http://foo/1"));
  AddTabToBrowser(GURL("http://foo/2"));
  AddTabToBrowser(GURL("http://foo/3"));

  // Pair tabs at index 1 and 2 into a split view.
  browser()->tab_strip_model()->ActivateTabAt(1);
  browser()->tab_strip_model()->AddToNewSplit(
      {2}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  // Close the split view.
  browser()->tab_strip_model()->CloseSelectedTabs();

  RecentTabsSubMenuModel model(nullptr, browser());

  std::vector<ModelData> kData;

  kData = {
      {ui::MenuModel::TYPE_COMMAND, true},    // History
      {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Recently closed
      {ui::MenuModel::TYPE_SUBMENU, true},    // <split view submenu>
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Your devices
      {ui::MenuModel::TYPE_COMMAND, true}     // recent tabs login
  };

  VerifyModel(model, kData);

  // Expected split view menu items:
  constexpr ModelData kSplitData[] = {
      {ui::MenuModel::TYPE_COMMAND, true},    // Restore split view
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, true},    // <tab for http://foo/1>
      {ui::MenuModel::TYPE_COMMAND, true},    // <tab for http://foo/2>
  };

  VerifyModel(model.GetSubmenuModelAt(4), kSplitData);
}

IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelSplitTest,
                       RecentlyClosedWindowWithSplit) {
  Init();
  DisableSync();

  Browser* new_browser = AddBrowser(browser(), {GURL("about:blank?0")});
  ui_test_utils::NavigateToURLWithDisposition(
      new_browser, GURL("about:blank?1"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  new_browser->tab_strip_model()->ActivateTabAt(0);
  new_browser->tab_strip_model()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  chrome::CloseWindow(new_browser);

  RecentTabsSubMenuModel model(nullptr, browser());

  std::vector<ModelData> kData = {
      {ui::MenuModel::TYPE_COMMAND, true},    // History
      {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Recently closed
      {ui::MenuModel::TYPE_SUBMENU, true},    // <window>
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Your devices
      {ui::MenuModel::TYPE_COMMAND, true}     // recent tabs login
  };

  VerifyModel(model, kData);

  constexpr ModelData kWindowSubmenuData[] = {
      {ui::MenuModel::TYPE_COMMAND, true},    // Restore window
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_SUBMENU, true},    // <split view>
  };
  const ui::MenuModel* const window_submenu = model.GetSubmenuModelAt(4);
  ASSERT_NO_FATAL_FAILURE(VerifyModel(window_submenu, kWindowSubmenuData));

  constexpr ModelData kSplitSubmenuData[] = {
      {ui::MenuModel::TYPE_COMMAND, true},    // Restore split view
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, true},    // <tab for http://wnd/split0>
      {ui::MenuModel::TYPE_COMMAND, true},    // <tab for http://wnd/split1>
  };
  VerifyModel(window_submenu->GetSubmenuModelAt(2), kSplitSubmenuData);
}

IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelSplitTest,
                       RecentlyClosedWindowWithGroupsAndSplits) {
  Init();
  DisableSync();

  Browser* new_browser = AddBrowser(browser(), {GURL("about:blank?0")});
  for (int i = 1; i <= 7; ++i) {
    ui_test_utils::NavigateToURLWithDisposition(
        new_browser, GURL("about:blank?" + base::NumberToString(i)),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  // Put them into groups and splits.
  // Tab 0: Group 1
  // Tab 1: Group 1
  // Tab 2: Split 1
  // Tab 3: Split 1
  // Tab 4: Group 2
  // Tab 5: Group 2
  // Tab 6: Split 2
  // Tab 7: Split 2
  new_browser->tab_strip_model()->AddToNewGroup({0, 1});

  new_browser->tab_strip_model()->ActivateTabAt(2);
  new_browser->tab_strip_model()->AddToNewSplit(
      {3}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  new_browser->tab_strip_model()->AddToNewGroup({4, 5});

  new_browser->tab_strip_model()->ActivateTabAt(6);
  new_browser->tab_strip_model()->AddToNewSplit(
      {7}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  chrome::CloseWindow(new_browser);

  RecentTabsSubMenuModel model(nullptr, browser());

  std::vector<ModelData> kData = {
      {ui::MenuModel::TYPE_COMMAND, true},    // History
      {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Recently closed
      {ui::MenuModel::TYPE_SUBMENU, true},    // <window>
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Your devices
      {ui::MenuModel::TYPE_COMMAND, true}     // recent tabs login
  };

  VerifyModel(model, kData);

  constexpr ModelData kWindowSubmenuData[] = {
      {ui::MenuModel::TYPE_COMMAND, true},    // Restore window
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_SUBMENU, true},    // Group 1
      {ui::MenuModel::TYPE_SUBMENU, true},    // Split 1
      {ui::MenuModel::TYPE_SUBMENU, true},    // Group 2
      {ui::MenuModel::TYPE_SUBMENU, true},    // Split 2
  };
  const ui::MenuModel* const window_submenu = model.GetSubmenuModelAt(4);
  ASSERT_NO_FATAL_FAILURE(VerifyModel(window_submenu, kWindowSubmenuData));

  // Verify Group 1 contents (index 2)
  constexpr ModelData kGroup1Data[] = {
      {ui::MenuModel::TYPE_COMMAND, true},    // Restore Group 1
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, true},    // Tab 0
      {ui::MenuModel::TYPE_COMMAND, true},    // Tab 1
  };
  VerifyModel(window_submenu->GetSubmenuModelAt(2), kGroup1Data);

  // Verify Split 1 contents (index 3)
  constexpr ModelData kSplit1Data[] = {
      {ui::MenuModel::TYPE_COMMAND, true},    // Restore Split 1
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, true},    // Tab 2
      {ui::MenuModel::TYPE_COMMAND, true},    // Tab 3
  };
  VerifyModel(window_submenu->GetSubmenuModelAt(3), kSplit1Data);

  // Verify Group 2 contents (index 4)
  constexpr ModelData kGroup2Data[] = {
      {ui::MenuModel::TYPE_COMMAND, true},    // Restore Group 2
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, true},    // Tab 4
      {ui::MenuModel::TYPE_COMMAND, true},    // Tab 5
  };
  VerifyModel(window_submenu->GetSubmenuModelAt(4), kGroup2Data);

  // Verify Split 2 contents (index 5)
  constexpr ModelData kSplit2Data[] = {
      {ui::MenuModel::TYPE_COMMAND, true},    // Restore Split 2
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, true},    // Tab 6
      {ui::MenuModel::TYPE_COMMAND, true},    // Tab 7
  };
  VerifyModel(window_submenu->GetSubmenuModelAt(5), kSplit2Data);
}

IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelSplitTest,
                       RecentlyClosedWindowWithSplitAndRegularTabs) {
  Init();
  DisableSync();

  Browser* new_browser = AddBrowser(browser(), {GURL("about:blank?0")});
  for (int i = 1; i <= 3; ++i) {
    ui_test_utils::NavigateToURLWithDisposition(
        new_browser, GURL("about:blank?" + base::NumberToString(i)),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  // Put Tab 1 and Tab 2 into a split. Tab 0 and Tab 3 remain regular tabs.
  new_browser->tab_strip_model()->ActivateTabAt(1);
  new_browser->tab_strip_model()->AddToNewSplit(
      {2}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  chrome::CloseWindow(new_browser);

  RecentTabsSubMenuModel model(nullptr, browser());

  std::vector<ModelData> kData = {
      {ui::MenuModel::TYPE_COMMAND, true},    // History
      {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Recently closed
      {ui::MenuModel::TYPE_SUBMENU, true},    // <window>
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Your devices
      {ui::MenuModel::TYPE_COMMAND, true}     // recent tabs login
  };

  VerifyModel(model, kData);

  constexpr ModelData kWindowSubmenuData[] = {
      {ui::MenuModel::TYPE_COMMAND, true},    // Restore window
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, true},    // Tab 0
      {ui::MenuModel::TYPE_SUBMENU, true},    // Split 1
      {ui::MenuModel::TYPE_COMMAND, true},    // Tab 3
  };
  const ui::MenuModel* const window_submenu = model.GetSubmenuModelAt(4);
  ASSERT_NO_FATAL_FAILURE(VerifyModel(window_submenu, kWindowSubmenuData));

  // Verify Split 1 contents (index 3)
  constexpr ModelData kSplitSubmenuData[] = {
      {ui::MenuModel::TYPE_COMMAND, true},    // Restore split view
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, true},    // Tab 1
      {ui::MenuModel::TYPE_COMMAND, true},    // Tab 2
  };
  VerifyModel(window_submenu->GetSubmenuModelAt(3), kSplitSubmenuData);
}

IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelSplitTest,
                       RecentlyClosedGroupWithSplit) {
  Init();
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  DisableSync();

  TabRestoreServiceFactory::GetForProfile(browser()->profile());

  AddTabToBrowser(GURL("http://foo/1"));
  AddTabToBrowser(GURL("http://foo/2"));
  AddTabToBrowser(GURL("http://foo/3"));

  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2, 3});

  // Pair tabs at index 1 and 2 into a split view.
  browser()->tab_strip_model()->ActivateTabAt(1);
  browser()->tab_strip_model()->AddToNewSplit(
      {2}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  // Close the group.
  browser()->tab_strip_model()->CloseAllTabsInGroup(group);

  RecentTabsSubMenuModel model(nullptr, browser());

  std::vector<ModelData> kData = {
      {ui::MenuModel::TYPE_COMMAND, true},    // History
      {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Recently closed
      {ui::MenuModel::TYPE_SUBMENU, true},    // <group>
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Your devices
      {ui::MenuModel::TYPE_COMMAND, true}     // recent tabs login
  };

  VerifyModel(model, kData);

  constexpr ModelData kGroupSubmenuData[] = {
      {ui::MenuModel::TYPE_COMMAND, true},    // Restore group
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_SUBMENU, true},    // <split view>
      {ui::MenuModel::TYPE_COMMAND, true},    // <tab for http://foo/3>
  };
  const ui::MenuModel* const group_submenu = model.GetSubmenuModelAt(4);
  ASSERT_NO_FATAL_FAILURE(VerifyModel(group_submenu, kGroupSubmenuData));

  constexpr ModelData kSplitSubmenuData[] = {
      {ui::MenuModel::TYPE_COMMAND, true},    // Restore split view
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, true},    // <tab for http://foo/1>
      {ui::MenuModel::TYPE_COMMAND, true},    // <tab for http://foo/2>
  };
  VerifyModel(group_submenu->GetSubmenuModelAt(2), kSplitSubmenuData);
}

IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelTest,
                       RecentlyClosedTabsAndWindowsFromLastSessionWithRefresh) {
  Init();
  DisableSync();

  TabRestoreServiceFactory::GetForProfile(browser()->profile());

  RecentTabsSubMenuModel model(nullptr, browser());
  TestRecentTabsMenuModelDelegate delegate(&model);
  EXPECT_FALSE(delegate.got_changes());

  // Expected menu items before tabs/windows from last session are loaded:
  std::vector<ModelData> kDataBeforeLoad = {
      {ui::MenuModel::TYPE_COMMAND, true},    // History
      {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, false},   // Recently closed
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Your devices
      {ui::MenuModel::TYPE_COMMAND, true}     // recent tabs login
  };

  VerifyModel(model, kDataBeforeLoad);

  // Add 2 tabs and close them.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://wnd/tab0"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://wnd/tab1"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  browser()->tab_strip_model()->CloseSelectedTabs();
  browser()->tab_strip_model()->CloseSelectedTabs();

  Browser* new_browser = AddBrowser(browser(), {GURL("http://wnd1/tab0")});
  ui_test_utils::NavigateToURLWithDisposition(
      new_browser, GURL("http://wnd1/tab1"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  chrome::GroupTab(new_browser);
  chrome::CloseWindow(new_browser);

  EXPECT_TRUE(delegate.got_changes());

  // Expected menu items after tabs/windows from last session are loaded:
  std::vector<ModelData> kDataAfterLoad = {
      {ui::MenuModel::TYPE_COMMAND, true},    // History
      {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Recently closed
      {ui::MenuModel::TYPE_SUBMENU, true},    // <window>
      {ui::MenuModel::TYPE_COMMAND, true},    // <tab for http://wnd0/tab1>
      {ui::MenuModel::TYPE_COMMAND, true},    // <tab for http://wnd0/tab0>
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Your devices
      {ui::MenuModel::TYPE_COMMAND, true}     // recent tabs login
  };

  VerifyModel(model, kDataAfterLoad);

  constexpr ModelData kWindowSubmenuData[] = {
      {ui::MenuModel::TYPE_COMMAND, true},    // Restore window
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, true},    // <tab for http://wnd1/tab0>
      {ui::MenuModel::TYPE_SUBMENU, true},    // <group>
  };
  const ui::MenuModel* const window_submenu = model.GetSubmenuModelAt(4);
  ASSERT_NO_FATAL_FAILURE(VerifyModel(window_submenu, kWindowSubmenuData));

  constexpr ModelData kGroupSubmenuData[] = {
      {ui::MenuModel::TYPE_COMMAND, true},    // Restore group
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, true},    // <tab for http://wnd1/tab1>
  };

  VerifyModel(window_submenu->GetSubmenuModelAt(3), kGroupSubmenuData);
}

// Test disabled "Recently closed" header with multiple sessions, multiple
// windows, and multiple enabled tabs from other devices.
IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelTest, OtherDevices) {
  Init();
  EnableSync();

  // Tabs are populated in decreasing timestamp.
  base::Time timestamp = base::Time::Now();
  const base::TimeDelta time_delta = base::Minutes(10);

  RecentTabsBuilderTestHelper recent_tabs_builder;

  // Create session 0: 1 window, 3 tabs
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(0);
  for (int i = 0; i < 3; ++i) {
    timestamp -= time_delta;
    recent_tabs_builder.AddTabWithInfo(0, 0, timestamp, std::u16string());
  }

  // Create session 1: 2 windows, 1 tab in 1st window, 2 tabs in 2nd window
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(1);
  recent_tabs_builder.AddWindow(1);
  timestamp -= time_delta;
  recent_tabs_builder.AddTabWithInfo(1, 0, timestamp, std::u16string());
  timestamp -= time_delta;
  recent_tabs_builder.AddTabWithInfo(1, 1, timestamp, std::u16string());
  timestamp -= time_delta;
  recent_tabs_builder.AddTabWithInfo(1, 1, timestamp, std::u16string());

  RegisterRecentTabs(&recent_tabs_builder);

  RecentTabsSubMenuModel model(nullptr, browser());

  std::vector<ModelData> kData;

  kData = {
      {ui::MenuModel::TYPE_COMMAND, true},    // History
      {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, false},   // Recently closed
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Your devices
      {ui::MenuModel::TYPE_SUBMENU, true},    // session 0 submenu
      {ui::MenuModel::TYPE_SUBMENU, true},    // session 1 submenu
  };

  VerifyModel(model, kData);
}

IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelTest, OtherDevicesDynamicUpdate) {
  Init();
  EnableSync();

  // Before creating menu fill foreign sessions.
  base::Time update_timestamp = base::Time::Now() - base::Minutes(10);

  // Create one foreign session with one window and one tab.
  RecentTabsBuilderTestHelper recent_tabs_builder;
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(0);
  recent_tabs_builder.AddTabWithInfo(0, 0, update_timestamp, std::u16string());
  RegisterRecentTabs(&recent_tabs_builder);

  RecentTabsSubMenuModel model(nullptr, browser());

  std::vector<ModelData> kDataSyncEnabled;

  kDataSyncEnabled = {
      {ui::MenuModel::TYPE_COMMAND, true},    // History
      {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, false},   // Recently closed
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Your devices
      {ui::MenuModel::TYPE_SUBMENU, true},    // session 0 submenu
  };

  VerifyModel(model, kDataSyncEnabled);

  // Make changes dynamically.
  update_timestamp = base::Time::Now() - base::Minutes(5);

  // Add tab to the only window.
  recent_tabs_builder.AddTabWithInfo(0, 0, update_timestamp, std::u16string());

  RegisterRecentTabs(&recent_tabs_builder);

  // Expected menu items after update:
  std::vector<ModelData> kDataAfterUpdate;

  kDataAfterUpdate = {
      {ui::MenuModel::TYPE_COMMAND, true},    // History
      {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, false},   // Recently closed
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Your devices
      {ui::MenuModel::TYPE_SUBMENU, true},    // session 0 submenu
  };

  VerifyModel(model, kDataAfterUpdate);
}

IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelTest, MaxSessionsAndRecency) {
  Init();
  EnableSync();

  // Create 4 sessions. Each session has 1 window with 1 tab.
  RecentTabsBuilderTestHelper recent_tabs_builder;
  for (int s = 0; s < 4; ++s) {
    recent_tabs_builder.AddSession();
    recent_tabs_builder.AddWindow(s);
    recent_tabs_builder.AddTab(s, 0);
  }
  RegisterRecentTabs(&recent_tabs_builder);

  RecentTabsSubMenuModel model(nullptr, browser());

  std::vector<ModelData> kData;
  kData = {{ui::MenuModel::TYPE_COMMAND, true},    // History
           {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
           {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
           {ui::MenuModel::TYPE_COMMAND, false},   // Recently closed
           {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
           {ui::MenuModel::TYPE_TITLE, false},     // Your devices
           {ui::MenuModel::TYPE_SUBMENU, true},
           {ui::MenuModel::TYPE_SUBMENU, true},
           {ui::MenuModel::TYPE_SUBMENU, true},
           {ui::MenuModel::TYPE_SUBMENU, true}};

  VerifyModel(model, kData);

  EXPECT_THAT(base::span<const std::u16string>(
                  recent_tabs_builder.GetTabTitlesSortedByRecency())
                  .first(4u),
              ElementsAre(model.GetSubmenuModelAt(6)->GetLabelAt(0),
                          model.GetSubmenuModelAt(7)->GetLabelAt(0),
                          model.GetSubmenuModelAt(8)->GetLabelAt(0),
                          model.GetSubmenuModelAt(9)->GetLabelAt(0)));
}

IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelTest,
                       MaxTabsPerSessionAndRecency) {
  Init();
  EnableSync();

  // Create a session: 2 windows with 5 tabs each.
  RecentTabsBuilderTestHelper recent_tabs_builder;
  recent_tabs_builder.AddSession();
  for (int w = 0; w < 2; ++w) {
    recent_tabs_builder.AddWindow(0);
    for (int t = 0; t < 5; ++t) {
      recent_tabs_builder.AddTab(0, w);
    }
  }
  RegisterRecentTabs(&recent_tabs_builder);

  RecentTabsSubMenuModel model(nullptr, browser());

  std::vector<ModelData> kData;
  // Expected menu items:

  kData = {
      {ui::MenuModel::TYPE_COMMAND, true},    // History
      {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, false},   // Recently closed
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Your devices
      {ui::MenuModel::TYPE_SUBMENU, true}     // session 0 submenu
  };

  VerifyModel(model, kData);

  EXPECT_THAT(
      base::span<const std::u16string>(
          recent_tabs_builder.GetTabTitlesSortedByRecency())
          .first(4u),
      ElementsAre(
          model.GetSubmenuModelAt(model.GetItemCount() - 1)->GetLabelAt(0),
          model.GetSubmenuModelAt(model.GetItemCount() - 1)->GetLabelAt(1),
          model.GetSubmenuModelAt(model.GetItemCount() - 1)->GetLabelAt(2),
          model.GetSubmenuModelAt(model.GetItemCount() - 1)->GetLabelAt(3)));
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(RecentTabsSubMenuModelTest, OtherDevicesAvailability) {
  if (!syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    GTEST_SKIP();
  }

  std::vector<ModelData> kDataWithOtherDevices = {
      {ui::MenuModel::TYPE_COMMAND, true},    // History
      {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, false},   // Recent Tabs
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_TITLE, false},     // Your devices
      {ui::MenuModel::TYPE_SUBMENU, true}     // session 0 submenu
  };

  std::vector<ModelData> kDataWithoutOtherDevices = {
      {ui::MenuModel::TYPE_COMMAND, true},    // History
      {ui::MenuModel::TYPE_COMMAND, true},    // History Cluster
      {ui::MenuModel::TYPE_SEPARATOR, true},  // <separator>
      {ui::MenuModel::TYPE_COMMAND, false}    // Recent Tabs
  };

  Init();
  EnableSync();
  RecentTabsBuilderTestHelper recent_tabs_builder;
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(0);
  recent_tabs_builder.AddTab(0, 0);
  RegisterRecentTabs(&recent_tabs_builder);

  // Default state.
  VerifyModel(RecentTabsSubMenuModel(nullptr, browser()),
              kDataWithOtherDevices);

  // Signed in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  signin::MakePrimaryAccountAvailable(identity_manager, "test@gmail.com",
                                      signin::ConsentLevel::kSignin);
  VerifyModel(RecentTabsSubMenuModel(nullptr, browser()),
              kDataWithOtherDevices);

  // History sync explicitly disabled: tabs from other devices are not shown.
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(browser()->profile());
  sync_service->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, false);
  VerifyModel(RecentTabsSubMenuModel(nullptr, browser()),
              kDataWithoutOtherDevices);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
