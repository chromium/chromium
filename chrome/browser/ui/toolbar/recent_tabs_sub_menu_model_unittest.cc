// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/recent_tabs_builder_test_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/menu_model_test.h"
#include "components/sessions/content/content_test_helper.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service_impl.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync_sessions/session_sync_service_impl.h"
#include "components/sync_sessions/synced_session.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This copies parts of MenuModelTest::Delegate and combines them with the
// RecentTabsSubMenuModel since RecentTabsSubMenuModel is a
// SimpleMenuModel::Delegate and not just derived from SimpleMenuModel.
class TestRecentTabsSubMenuModel : public RecentTabsSubMenuModel {
 public:
  TestRecentTabsSubMenuModel(ui::AcceleratorProvider* provider,
                             Browser* browser)
      : RecentTabsSubMenuModel(provider, browser),
        execute_count_(0),
        enable_count_(0) {}

  // Testing overrides to ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override {
    bool val = RecentTabsSubMenuModel::IsCommandIdEnabled(command_id);
    if (val)
      ++enable_count_;
    return val;
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    ++execute_count_;
  }

  int execute_count() const { return execute_count_; }
  int enable_count() const { return enable_count_; }

 private:
  int execute_count_;
  int mutable enable_count_;  // Mutable because IsCommandIdEnabledAt is const.

  DISALLOW_COPY_AND_ASSIGN(TestRecentTabsSubMenuModel);
};

class TestRecentTabsMenuModelDelegate : public ui::MenuModelDelegate {
 public:
  explicit TestRecentTabsMenuModelDelegate(ui::MenuModel* model)
      : model_(model),
        got_changes_(false) {
    model_->SetMenuModelDelegate(this);
  }

  ~TestRecentTabsMenuModelDelegate() override {
    model_->SetMenuModelDelegate(nullptr);
  }

  // ui::MenuModelDelegate implementation:

  void OnIconChanged(int index) override {}

  void OnMenuStructureChanged() override { got_changes_ = true; }

  bool got_changes() const { return got_changes_; }

 private:
  ui::MenuModel* model_;
  bool got_changes_;

  DISALLOW_COPY_AND_ASSIGN(TestRecentTabsMenuModelDelegate);
};

}  // namespace

class RecentTabsSubMenuModelTest
    : public BrowserWithTestWindowTest {
 public:
  RecentTabsSubMenuModelTest() {}

  void WaitForLoadFromLastSession() { content::RunAllTasksUntilIdle(); }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    session_sync_service_ = SessionSyncServiceFactory::GetForProfile(profile());

    syncer::DataTypeActivationRequest activation_request;
    activation_request.cache_guid = "test_cache_guid";
    activation_request.error_handler = base::DoNothing();

    std::unique_ptr<syncer::DataTypeActivationResponse> activation_response;
    base::RunLoop loop;
    session_sync_service_->GetControllerDelegate()->OnSyncStarting(
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
    session_sync_service_->ProxyTabsStateChanged(
        syncer::DataTypeController::RUNNING);
  }

  void DisableSync() {
    session_sync_service_->ProxyTabsStateChanged(
        syncer::DataTypeController::NOT_RUNNING);
  }

  static std::unique_ptr<KeyedService> GetTabRestoreService(
      content::BrowserContext* browser_context) {
    return std::make_unique<sessions::TabRestoreServiceImpl>(
        base::WrapUnique(new ChromeTabRestoreServiceClient(
            Profile::FromBrowserContext(browser_context))),
        nullptr, nullptr);
  }

  void RegisterRecentTabs(RecentTabsBuilderTestHelper* helper) {
    helper->ExportToSessionSync(sync_processor_.get());
    helper->VerifyExport(static_cast<sync_sessions::SessionSyncServiceImpl*>(
                             session_sync_service_)
                             ->GetUnderlyingOpenTabsUIDelegateForTest());
  }

 private:
  sync_sessions::SessionSyncService* session_sync_service_;
  std::unique_ptr<syncer::ModelTypeProcessor> sync_processor_;

  DISALLOW_COPY_AND_ASSIGN(RecentTabsSubMenuModelTest);
};

// Test disabled "Recently closed" header with no foreign tabs.
TEST_F(RecentTabsSubMenuModelTest, NoTabs) {
  DisableSync();

  TestRecentTabsSubMenuModel model(nullptr, browser());

  // Expected menu:
  // Menu index  Menu items
  // ---------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header (disabled)
  // 3           <separator>
  // 4           No tabs from other Devices

  int num_items = model.GetItemCount();
  EXPECT_EQ(5, num_items);
  EXPECT_FALSE(model.IsEnabledAt(2));
  EXPECT_FALSE(model.IsEnabledAt(4));
  EXPECT_EQ(0, model.enable_count());

  EXPECT_EQ(NULL, model.GetLabelFontListAt(0));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(1));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(2));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(3));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(4));

  std::string url;
  base::string16 title;
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(0, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(1, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(2, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(3, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(4, &url, &title));
}

// Test enabled "Recently closed" header with no foreign tabs.
TEST_F(RecentTabsSubMenuModelTest, RecentlyClosedTabsFromCurrentSession) {
  DisableSync();

  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&RecentTabsSubMenuModelTest::GetTabRestoreService));

  // Add 2 tabs and close them.
  AddTab(browser(), GURL("http://foo/1"));
  AddTab(browser(), GURL("http://foo/2"));
  browser()->tab_strip_model()->CloseAllTabs();

  TestRecentTabsSubMenuModel model(nullptr, browser());
  // Expected menu:
  // Menu index  Menu items
  // --------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header
  // 3           <tab for http://foo/2>
  // 4           <tab for http://foo/1>
  // 5           <separator>
  // 6           No tabs from other Devices
  int num_items = model.GetItemCount();
  EXPECT_EQ(7, num_items);
  EXPECT_TRUE(model.IsEnabledAt(0));
  model.ActivatedAt(0);
  EXPECT_TRUE(model.IsEnabledAt(1));
  EXPECT_FALSE(model.IsEnabledAt(2));
  EXPECT_TRUE(model.IsEnabledAt(3));
  EXPECT_TRUE(model.IsEnabledAt(4));
  model.ActivatedAt(3);
  model.ActivatedAt(4);
  EXPECT_FALSE(model.IsEnabledAt(6));
  EXPECT_EQ(3, model.enable_count());
  EXPECT_EQ(3, model.execute_count());

  EXPECT_EQ(NULL, model.GetLabelFontListAt(0));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(1));
  EXPECT_TRUE(model.GetLabelFontListAt(2) != nullptr);
  EXPECT_EQ(NULL, model.GetLabelFontListAt(3));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(4));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(5));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(6));

  std::string url;
  base::string16 title;
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(0, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(1, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(2, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(3, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(4, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(5, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(6, &url, &title));
}

TEST_F(RecentTabsSubMenuModelTest,
       RecentlyClosedTabsAndWindowsFromLastSession) {
  DisableSync();

  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&RecentTabsSubMenuModelTest::GetTabRestoreService));

  // Add 2 tabs and close them.
  AddTab(browser(), GURL("http://wnd/tab0"));
  AddTab(browser(), GURL("http://wnd/tab1"));
  browser()->tab_strip_model()->CloseAllTabs();

  // Create a SessionService for the profile (profile owns the service) and add
  // a window with a tab to this session.
  SessionService* session_service = new SessionService(profile());
  SessionServiceFactory::SetForTestProfile(profile(),
                                           base::WrapUnique(session_service));
  SessionID tab_id = SessionID::FromSerializedValue(1);
  SessionID window_id = SessionID::FromSerializedValue(2);
  session_service->SetWindowType(window_id, Browser::TYPE_NORMAL);
  session_service->SetTabWindow(window_id, tab_id);
  session_service->SetTabIndexInWindow(window_id, tab_id, 0);
  session_service->SetSelectedTabInWindow(window_id, 0);
  session_service->UpdateTabNavigation(
      window_id, tab_id,
      sessions::ContentTestHelper::CreateNavigation("http://wnd1/tab0",
                                                    "title"));
  // Set this, otherwise previous session won't be loaded.
  profile()->set_last_session_exited_cleanly(false);
  // Move this session to the last so that TabRestoreService will load it as the
  // last session.
  SessionServiceFactory::GetForProfile(profile())->
      MoveCurrentSessionToLastSession();

  // Create a new TabRestoreService so that it'll load the recently closed tabs
  // and windows afresh.
  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&RecentTabsSubMenuModelTest::GetTabRestoreService));
  // Let the shutdown of previous TabRestoreService run.
  content::RunAllTasksUntilIdle();

  TestRecentTabsSubMenuModel model(nullptr, browser());
  TestRecentTabsMenuModelDelegate delegate(&model);
  EXPECT_FALSE(delegate.got_changes());

  // Expected menu before tabs/windows from last session are loaded:
  // Menu index  Menu items
  // ----------------------------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header
  // 3           <separator>
  // 4           No tabs from other Devices

  int num_items = model.GetItemCount();
  EXPECT_EQ(5, num_items);
  EXPECT_TRUE(model.IsEnabledAt(0));
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR, model.GetTypeAt(1));
  EXPECT_FALSE(model.IsEnabledAt(2));
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR, model.GetTypeAt(3));
  EXPECT_FALSE(model.IsEnabledAt(4));
  EXPECT_EQ(1, model.enable_count());

  // Wait for tabs from last session to be loaded.
  WaitForLoadFromLastSession();

  // Expected menu after tabs/windows from last session are loaded:
  // Menu index  Menu items
  // --------------------------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header
  // 3           <window for the tab http://wnd1/tab0>
  // 4           <tab for http://wnd0/tab1>
  // 5           <tab for http://wnd0/tab0>
  // 6           <separator>
  // 7           No tabs from other Devices

  EXPECT_TRUE(delegate.got_changes());

  num_items = model.GetItemCount();
  EXPECT_EQ(8, num_items);

  EXPECT_TRUE(model.IsEnabledAt(0));
  model.ActivatedAt(0);
  EXPECT_TRUE(model.IsEnabledAt(1));
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR, model.GetTypeAt(1));
  EXPECT_FALSE(model.IsEnabledAt(2));
  EXPECT_TRUE(model.IsEnabledAt(3));
  EXPECT_TRUE(model.IsEnabledAt(4));
  EXPECT_TRUE(model.IsEnabledAt(5));
  model.ActivatedAt(3);
  model.ActivatedAt(4);
  model.ActivatedAt(5);
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR, model.GetTypeAt(6));
  EXPECT_FALSE(model.IsEnabledAt(7));
  EXPECT_EQ(5, model.enable_count());
  EXPECT_EQ(4, model.execute_count());

  EXPECT_EQ(NULL, model.GetLabelFontListAt(0));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(1));
  EXPECT_TRUE(model.GetLabelFontListAt(2) != nullptr);
  EXPECT_EQ(NULL, model.GetLabelFontListAt(3));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(4));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(5));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(6));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(7));

  std::string url;
  base::string16 title;
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(0, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(1, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(2, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(3, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(4, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(5, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(6, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(7, &url, &title));
}

// Test disabled "Recently closed" header with multiple sessions, multiple
// windows, and multiple enabled tabs from other devices.
TEST_F(RecentTabsSubMenuModelTest, OtherDevices) {
  // Tabs are populated in decreasing timestamp.
  base::Time timestamp = base::Time::Now();
  const base::TimeDelta time_delta = base::TimeDelta::FromMinutes(10);

  RecentTabsBuilderTestHelper recent_tabs_builder;

  // Create 1st session : 1 window, 3 tabs
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(0);
  for (int i = 0; i < 3; ++i) {
    timestamp -= time_delta;
    recent_tabs_builder.AddTabWithInfo(0, 0, timestamp, base::string16());
  }

  // Create 2nd session : 2 windows, 1 tab in 1st window, 2 tabs in 2nd window
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(1);
  recent_tabs_builder.AddWindow(1);
  timestamp -= time_delta;
  recent_tabs_builder.AddTabWithInfo(1, 0, timestamp, base::string16());
  timestamp -= time_delta;
  recent_tabs_builder.AddTabWithInfo(1, 1, timestamp, base::string16());
  timestamp -= time_delta;
  recent_tabs_builder.AddTabWithInfo(1, 1, timestamp, base::string16());

  RegisterRecentTabs(&recent_tabs_builder);

  // Verify that data is populated correctly in RecentTabsSubMenuModel.
  // Expected menu:
  // - first inserted tab is most recent and hence is top
  // Menu index  Menu items
  // -----------------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header (disabled)
  // 3           <separator>
  // 4           <section header for 1st session>
  // 5-7         <3 tabs of the only window of session 0>
  // 8           <separator>
  // 9           <section header for 2nd session>
  // 10          <the only tab of window 0 of session 1>
  // 11-12       <2 tabs of window 1 of session 2>

  TestRecentTabsSubMenuModel model(nullptr, browser());
  int num_items = model.GetItemCount();
  EXPECT_EQ(13, num_items);
  model.ActivatedAt(0);
  EXPECT_TRUE(model.IsEnabledAt(0));
  model.ActivatedAt(1);
  EXPECT_TRUE(model.IsEnabledAt(1));
  model.ActivatedAt(2);
  EXPECT_FALSE(model.IsEnabledAt(2));
  model.ActivatedAt(3);
  EXPECT_TRUE(model.IsEnabledAt(3));
  model.ActivatedAt(5);
  EXPECT_TRUE(model.IsEnabledAt(5));
  model.ActivatedAt(6);
  EXPECT_TRUE(model.IsEnabledAt(6));
  model.ActivatedAt(7);
  EXPECT_TRUE(model.IsEnabledAt(7));
  model.ActivatedAt(10);
  EXPECT_TRUE(model.IsEnabledAt(10));
  model.ActivatedAt(11);
  EXPECT_TRUE(model.IsEnabledAt(11));
  model.ActivatedAt(12);
  EXPECT_TRUE(model.IsEnabledAt(12));

  EXPECT_EQ(7, model.enable_count());
  EXPECT_EQ(10, model.execute_count());

  EXPECT_EQ(NULL, model.GetLabelFontListAt(0));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(1));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(2));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(3));
  EXPECT_TRUE(model.GetLabelFontListAt(4) != nullptr);
  EXPECT_EQ(NULL, model.GetLabelFontListAt(5));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(6));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(7));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(8));
  EXPECT_TRUE(model.GetLabelFontListAt(9) != nullptr);
  EXPECT_EQ(NULL, model.GetLabelFontListAt(10));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(11));
  EXPECT_EQ(NULL, model.GetLabelFontListAt(12));

  std::string url;
  base::string16 title;
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(0, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(1, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(2, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(3, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(4, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(5, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(6, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(7, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(8, &url, &title));
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(9, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(10, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(11, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(12, &url, &title));
}

TEST_F(RecentTabsSubMenuModelTest, OtherDevicesDynamicUpdate) {
  // Create menu with disabled synchronization.
  DisableSync();

  // Before creating menu fill foreign sessions.
  base::Time update_timestamp =
      base::Time::Now() - base::TimeDelta::FromMinutes(10);

  RecentTabsBuilderTestHelper recent_tabs_builder;

  // Create one session with one window and one tab.
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(0);
  recent_tabs_builder.AddTabWithInfo(0, 0, update_timestamp, base::string16());

  RegisterRecentTabs(&recent_tabs_builder);

  // Verify that data is populated correctly in RecentTabsSubMenuModel.
  // Expected menu:
  // Menu index  Menu items
  // -----------------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header (disabled)
  // 3           <separator>
  // 4           No tabs from other Devices

  TestRecentTabsSubMenuModel model(nullptr, browser());
  EXPECT_EQ(5, model.GetItemCount());
  model.ActivatedAt(4);
  EXPECT_FALSE(model.IsEnabledAt(4));

  EXPECT_EQ(0, model.enable_count());
  EXPECT_EQ(1, model.execute_count());

  EXPECT_EQ(nullptr, model.GetLabelFontListAt(4));

  std::string url;
  base::string16 title;
  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(4, &url, &title));

  // Enable synchronization and notify menu that synchronization was enabled.
  int previous_enable_count = model.enable_count();
  int previous_execute_count = model.execute_count();

  EnableSync();

  // Verify that data is populated correctly in RecentTabsSubMenuModel.
  // Expected menu:
  // Menu index  Menu items
  // -----------------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header (disabled)
  // 3           <separator>
  // 4           <section header for 1st session>
  // 5           <tab of the only window of session 0>

  EXPECT_EQ(6, model.GetItemCount());
  model.ActivatedAt(4);
  EXPECT_FALSE(model.IsEnabledAt(4));
  model.ActivatedAt(5);
  EXPECT_TRUE(model.IsEnabledAt(5));

  EXPECT_EQ(previous_enable_count + 1, model.enable_count());
  EXPECT_EQ(previous_execute_count + 2, model.execute_count());

  EXPECT_NE(nullptr, model.GetLabelFontListAt(4));
  EXPECT_EQ(nullptr, model.GetLabelFontListAt(5));

  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(4, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(5, &url, &title));

  // Make changes dynamically.
  previous_enable_count = model.enable_count();
  previous_execute_count = model.execute_count();

  update_timestamp = base::Time::Now() - base::TimeDelta::FromMinutes(5);

  // Add tab to the only window.
  recent_tabs_builder.AddTabWithInfo(0, 0, update_timestamp, base::string16());

  RegisterRecentTabs(&recent_tabs_builder);

  // Verify that data is populated correctly in RecentTabsSubMenuModel.
  // Expected menu:
  // Menu index  Menu items
  // -----------------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header (disabled)
  // 3           <separator>
  // 4           <section header for 1st session>
  // 5           <new added tab of the only window of session 0>
  // 6           <tab of the only window of session 0>

  EXPECT_EQ(7, model.GetItemCount());
  model.ActivatedAt(4);
  EXPECT_FALSE(model.IsEnabledAt(4));
  model.ActivatedAt(5);
  EXPECT_TRUE(model.IsEnabledAt(5));
  model.ActivatedAt(6);
  EXPECT_TRUE(model.IsEnabledAt(6));

  EXPECT_EQ(previous_enable_count + 2, model.enable_count());
  EXPECT_EQ(previous_execute_count + 3, model.execute_count());

  EXPECT_NE(nullptr, model.GetLabelFontListAt(4));
  EXPECT_EQ(nullptr, model.GetLabelFontListAt(5));
  EXPECT_EQ(nullptr, model.GetLabelFontListAt(6));

  EXPECT_FALSE(model.GetURLAndTitleForItemAtIndex(4, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(5, &url, &title));
  EXPECT_TRUE(model.GetURLAndTitleForItemAtIndex(6, &url, &title));
}

TEST_F(RecentTabsSubMenuModelTest, MaxSessionsAndRecency) {
  // Create 4 sessions : each session has 1 window with 1 tab each.
  RecentTabsBuilderTestHelper recent_tabs_builder;
  for (int s = 0; s < 4; ++s) {
    recent_tabs_builder.AddSession();
    recent_tabs_builder.AddWindow(s);
    recent_tabs_builder.AddTab(s, 0);
  }
  RegisterRecentTabs(&recent_tabs_builder);

  // Verify that data is populated correctly in RecentTabsSubMenuModel.
  // Expected menu:
  // - max sessions is 3, so only 3 most-recent sessions will show.
  // Menu index  Menu items
  // ----------------------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header (disabled)
  // 3           <separator>
  // 4           <section header for 1st session>
  // 5           <the only tab of the only window of session 3>
  // 6           <separator>
  // 7           <section header for 2nd session>
  // 8           <the only tab of the only window of session 2>
  // 9           <separator>
  // 10          <section header for 3rd session>
  // 11          <the only tab of the only window of session 1>

  TestRecentTabsSubMenuModel model(nullptr, browser());
  int num_items = model.GetItemCount();
  EXPECT_EQ(12, num_items);

  std::vector<base::string16> tab_titles =
      recent_tabs_builder.GetTabTitlesSortedByRecency();
  EXPECT_EQ(tab_titles[0], model.GetLabelAt(5));
  EXPECT_EQ(tab_titles[1], model.GetLabelAt(8));
  EXPECT_EQ(tab_titles[2], model.GetLabelAt(11));
}

TEST_F(RecentTabsSubMenuModelTest, MaxTabsPerSessionAndRecency) {
  EnableSync();

  // Create a session: 2 windows with 5 tabs each.
  RecentTabsBuilderTestHelper recent_tabs_builder;
  recent_tabs_builder.AddSession();
  for (int w = 0; w < 2; ++w) {
    recent_tabs_builder.AddWindow(0);
    for (int t = 0; t < 5; ++t)
      recent_tabs_builder.AddTab(0, w);
  }
  RegisterRecentTabs(&recent_tabs_builder);

  // Verify that data is populated correctly in RecentTabsSubMenuModel.
  // Expected menu:
  // - max tabs per session is 4, so only 4 most-recent tabs will show,
  //   independent of which window they came from.
  // Menu index  Menu items
  // ---------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header (disabled)
  // 3           <separator>
  // 4           <section header for session>
  // 5-8         <4 most-recent tabs of session>

  TestRecentTabsSubMenuModel model(nullptr, browser());
  int num_items = model.GetItemCount();
  EXPECT_EQ(9, num_items);

  std::vector<base::string16> tab_titles =
      recent_tabs_builder.GetTabTitlesSortedByRecency();
  for (int i = 0; i < 4; ++i)
    EXPECT_EQ(tab_titles[i], model.GetLabelAt(i + 5));
}

TEST_F(RecentTabsSubMenuModelTest, MaxWidth) {
  EnableSync();

  // Create 1 session with 1 window and 1 tab.
  RecentTabsBuilderTestHelper recent_tabs_builder;
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(0);
  recent_tabs_builder.AddTab(0, 0);
  RegisterRecentTabs(&recent_tabs_builder);

  // Menu index  Menu items
  // ----------------------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed header (disabled)
  // 3           <separator>
  // 4           <section header for 1st session>
  // 5           <the only tab of the only window of session 1>

  TestRecentTabsSubMenuModel model(nullptr, browser());
  EXPECT_EQ(6, model.GetItemCount());
  EXPECT_EQ(-1, model.GetMaxWidthForItemAtIndex(2));
  EXPECT_NE(-1, model.GetMaxWidthForItemAtIndex(3));
  EXPECT_NE(-1, model.GetMaxWidthForItemAtIndex(4));
  EXPECT_NE(-1, model.GetMaxWidthForItemAtIndex(5));
}

TEST_F(RecentTabsSubMenuModelTest, MaxWidthNoDevices) {
  DisableSync();

  // Expected menu:
  // Menu index  Menu items
  // --------------------------------------------
  // 0           History
  // 1           <separator>
  // 2           Recently closed heaer (disabled)
  // 3           <separator>
  // 4           No tabs from other Devices

  TestRecentTabsSubMenuModel model(nullptr, browser());
  EXPECT_EQ(5, model.GetItemCount());
  EXPECT_EQ(-1, model.GetMaxWidthForItemAtIndex(2));
  EXPECT_NE(-1, model.GetMaxWidthForItemAtIndex(3));
  EXPECT_EQ(-1, model.GetMaxWidthForItemAtIndex(4));
}
