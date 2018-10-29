// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegates_getter.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/recent_tabs_builder_test_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/menu_model_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sessions/core/persistent_tab_restore_service.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_types.h"
#include "components/sync/device_info/local_device_info_provider_mock.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/sessions_sync_manager.h"
#include "components/sync_sessions/synced_session.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;

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
    model_->SetMenuModelDelegate(NULL);
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

class FakeSyncServiceObserverList {
 public:
  FakeSyncServiceObserverList() {}
  ~FakeSyncServiceObserverList() {}

  void AddObserver(syncer::SyncServiceObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(syncer::SyncServiceObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  void NotifyConfigureDone() {
    for (auto& observer : observers_)
      observer.OnSyncConfigurationCompleted(nullptr);
  }

  void NotifyForeignSessionUpdated() {
    for (auto& observer : observers_)
      observer.OnForeignSessionUpdated(nullptr);
  }

 private:
  base::ObserverList<syncer::SyncServiceObserver, true>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeSyncServiceObserverList);
};

}  // namespace

class RecentTabsSubMenuModelTest
    : public BrowserWithTestWindowTest {
 public:
  RecentTabsSubMenuModelTest() {
    override_features_.InitAndDisableFeature(switches::kSyncUSSSessions);
  }

  void SetUp() override {
    // Set up our mock sync service factory before the sync service (and any
    // other services that depend on it) gets created.
    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::Bind(OnWillCreateBrowserContextServices));

    BrowserWithTestWindowTest::SetUp();

    mock_sync_service_ = static_cast<browser_sync::ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetForProfile(profile()));
    manager_ = static_cast<sync_sessions::SessionsSyncManager*>(
        SessionSyncServiceFactory::GetForProfile(profile())
            ->GetSyncableService());

    // Needed because ProfileSyncService::Initialize() is not exercised.
    mock_sync_service_->SetLocalDeviceInfoProviderForTest(
        std::make_unique<syncer::LocalDeviceInfoProviderMock>(
            "RecentTabsSubMenuModelTest", "Test Machine", "Chromium 10k",
            "Chrome 10k", sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
            "device_id"));

    ON_CALL(*mock_sync_service_, AddObserver(_))
        .WillByDefault(Invoke(&fake_sync_service_observer_list_,
                              &FakeSyncServiceObserverList::AddObserver));
    ON_CALL(*mock_sync_service_, RemoveObserver(_))
        .WillByDefault(Invoke(&fake_sync_service_observer_list_,
                              &FakeSyncServiceObserverList::RemoveObserver));
    ON_CALL(*mock_sync_service_, NotifyForeignSessionUpdated())
        .WillByDefault(
            Invoke(&fake_sync_service_observer_list_,
                   &FakeSyncServiceObserverList::NotifyForeignSessionUpdated));

    manager_->MergeDataAndStartSyncing(
        syncer::SESSIONS, syncer::SyncDataList(),
        std::unique_ptr<syncer::SyncChangeProcessor>(
            new syncer::FakeSyncChangeProcessor),
        std::unique_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock));
  }

  void WaitForLoadFromLastSession() { content::RunAllTasksUntilIdle(); }

  void DisableSync() {
    EXPECT_CALL(*mock_sync_service_, GetDisableReasons())
        .WillRepeatedly(
            Return(syncer::SyncService::DISABLE_REASON_USER_CHOICE));
    EXPECT_CALL(*mock_sync_service_, GetTransportState())
        .WillRepeatedly(Return(syncer::SyncService::TransportState::DISABLED));
    EXPECT_CALL(*mock_sync_service_, IsDataTypeControllerRunning(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_sync_service_, GetOpenTabsUIDelegateMock())
        .WillRepeatedly(Return(nullptr));
  }

  void EnableSync() {
    EXPECT_CALL(*mock_sync_service_, GetDisableReasons())
        .WillRepeatedly(Return(syncer::SyncService::DISABLE_REASON_NONE));
    EXPECT_CALL(*mock_sync_service_, GetTransportState())
        .WillRepeatedly(Return(syncer::SyncService::TransportState::ACTIVE));
    EXPECT_CALL(*mock_sync_service_, IsFirstSetupComplete())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_sync_service_,
                IsDataTypeControllerRunning(syncer::SESSIONS))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_sync_service_,
                IsDataTypeControllerRunning(syncer::PROXY_TABS))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_sync_service_, GetOpenTabsUIDelegateMock())
        .WillRepeatedly(Return(manager_->GetOpenTabsUIDelegate()));
  }

  void NotifySyncEnabled() {
    fake_sync_service_observer_list_.NotifyConfigureDone();
  }

  static std::unique_ptr<KeyedService> GetTabRestoreService(
      content::BrowserContext* browser_context) {
    return std::make_unique<sessions::PersistentTabRestoreService>(
        base::WrapUnique(new ChromeTabRestoreServiceClient(
            Profile::FromBrowserContext(browser_context))),
        nullptr);
  }

  void RegisterRecentTabs(RecentTabsBuilderTestHelper* helper) {
    helper->ExportToSessionsSyncManager(manager_);
  }

 private:
  static void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {
    ProfileSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildMockProfileSyncService));
  }

  base::test::ScopedFeatureList override_features_;

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;

  FakeSyncServiceObserverList fake_sync_service_observer_list_;
  browser_sync::ProfileSyncServiceMock* mock_sync_service_ = nullptr;
  sync_sessions::SessionsSyncManager* manager_;

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

// TODO(sail): enable this test when dynamic model is enabled in
// RecentTabsSubMenuModel.
#if defined(OS_MACOSX)
#define MAYBE_RecentlyClosedTabsAndWindowsFromLastSession \
    DISABLED_RecentlyClosedTabsAndWindowsFromLastSession
#else
#define MAYBE_RecentlyClosedTabsAndWindowsFromLastSession \
    RecentlyClosedTabsAndWindowsFromLastSession
#endif
TEST_F(RecentTabsSubMenuModelTest,
       MAYBE_RecentlyClosedTabsAndWindowsFromLastSession) {
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
  session_service->SetWindowType(window_id,
                                 Browser::TYPE_TABBED,
                                 SessionService::TYPE_NORMAL);
  session_service->SetTabWindow(window_id, tab_id);
  session_service->SetTabIndexInWindow(window_id, tab_id, 0);
  session_service->SetSelectedTabInWindow(window_id, 0);
  session_service->UpdateTabNavigation(
      window_id, tab_id,
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://wnd1/tab0", "title"));
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
  EnableSync();

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

// Mac doesn't support the dynamic menu.
#if defined(OS_MACOSX)
#define MAYBE_OtherDevicesDynamicUpdate DISABLED_OtherDevicesDynamicUpdate
#else
#define MAYBE_OtherDevicesDynamicUpdate OtherDevicesDynamicUpdate
#endif
TEST_F(RecentTabsSubMenuModelTest, MAYBE_OtherDevicesDynamicUpdate) {
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
  NotifySyncEnabled();

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
  EnableSync();

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
