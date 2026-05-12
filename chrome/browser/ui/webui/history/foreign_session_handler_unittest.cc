// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/foreign_session_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_metrics.h"
#include "chrome/browser/ui/webui/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_sessions/mock_open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"

namespace browser_sync {

MATCHER_P(TabHasUrl, url, "") {
  return !arg.navigations.empty() &&
         arg.navigations.back().virtual_url() == url;
}

// Partial SessionSyncService that can fake behavior for
// SubscribeToForeignSessionsChanged() including the notification to
// subscribers.
class FakeSessionSyncService : public sync_sessions::SessionSyncService {
 public:
  FakeSessionSyncService() = default;
  ~FakeSessionSyncService() override = default;

  void NotifyForeignSessionsChanged() { subscriber_list_.Notify(); }

  // SessionSyncService overrides.
  syncer::GlobalIdMapper* GetGlobalIdMapper() const override { return nullptr; }

  sync_sessions::MockOpenTabsUIDelegate* GetOpenTabsUIDelegate() override {
    return &mock_open_tabs_ui_delegate_;
  }

  void AddTabScreenshot(SessionID tab_id,
                        std::string&& screenshot_data,
                        const GURL& url) override {}

  void ReadTabScreenshot(
      const std::string& session_tag,
      SessionID tab_id,
      base::OnceCallback<void(std::optional<std::string>)> callback) override {
    std::move(callback).Run(std::nullopt);
  }

  base::CallbackListSubscription SubscribeToForeignSessionsChanged(
      const base::RepeatingClosure& cb) override {
    return subscriber_list_.Add(cb);
  }

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override {
    return nullptr;
  }

 private:
  base::RepeatingClosureList subscriber_list_;
  sync_sessions::MockOpenTabsUIDelegate mock_open_tabs_ui_delegate_;
};

class MockForeignSessionPage : public history::mojom::ForeignSessionPage {
 public:
  MockForeignSessionPage() = default;
  ~MockForeignSessionPage() override = default;

  mojo::PendingRemote<history::mojom::ForeignSessionPage> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD1(OnForeignSessionsChanged,
               void(std::vector<history::mojom::ForeignSessionPtr> sessions));

  mojo::Receiver<history::mojom::ForeignSessionPage> receiver_{this};
};

class MockEmbedder final : public TopChromeWebUIController::Embedder {
 public:
  ~MockEmbedder() = default;
  MOCK_METHOD(void, ShowUI, (), (override));
  MOCK_METHOD(void, CloseUI, (), (override));
  MOCK_METHOD(void,
              ShowContextMenu,
              (gfx::Point point, std::unique_ptr<ui::MenuModel> menu_model),
              (override));
  MOCK_METHOD(void, HideContextMenu, (), (override));

  base::WeakPtr<MockEmbedder> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockEmbedder> weak_ptr_factory_{this};
};

class ForeignSessionHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    handler_ = std::make_unique<ForeignSessionHandler>(
        handler_remote_.BindNewPipeAndPassReceiver(), profile(), web_contents(),
        restore_tab_callback_.Get(), restore_windows_callback_.Get(),
        /*side_panel_ui=*/nullptr);
    handler_->SetPage(page_.BindAndGetRemote());
  }

  void TearDown() override {
    handler_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {
        TestingProfile::TestingFactory{
            SessionSyncServiceFactory::GetInstance(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<FakeSessionSyncService>();
            })},
    };
  }

  FakeSessionSyncService* session_sync_service() {
    return static_cast<FakeSessionSyncService*>(
        SessionSyncServiceFactory::GetForProfile(profile()));
  }

  ForeignSessionHandler* handler() { return handler_.get(); }

 protected:
  MockForeignSessionPage page_;
  mojo::Remote<history::mojom::ForeignSessionPageHandler> handler_remote_;
  std::unique_ptr<ForeignSessionHandler> handler_;

  base::MockCallback<ForeignSessionHandler::RestoreForeignSessionTabCallback>
      restore_tab_callback_;
  base::MockCallback<
      ForeignSessionHandler::RestoreForeignSessionWindowsCallback>
      restore_windows_callback_;
};

TEST_F(ForeignSessionHandlerTest, ShouldFireForeignSessionsChanged) {
  EXPECT_CALL(page_, OnForeignSessionsChanged(testing::_));

  session_sync_service()->NotifyForeignSessionsChanged();
}

TEST_F(ForeignSessionHandlerTest, OpenForeignSessionAllTabs) {
  ::sessions::SessionWindow window1;
  std::vector<const ::sessions::SessionWindow*> windows = {&window1};

  EXPECT_CALL(*session_sync_service()->GetOpenTabsUIDelegate(),
              GetForeignSession("my_session_tag"))
      .WillOnce(testing::Return(windows));

  EXPECT_CALL(restore_windows_callback_, Run(profile(), windows));

  handler()->OpenForeignSessionAllTabs("my_session_tag");
}

TEST_F(ForeignSessionHandlerTest, OpenForeignSessionTabLeftClick) {
  ::sessions::SessionTab session_tab;
  session_tab.navigations.emplace_back();
  session_tab.navigations.back().set_virtual_url(
      GURL("https://www.google.com"));

  EXPECT_CALL(*session_sync_service()->GetOpenTabsUIDelegate(),
              GetForeignTab("my_session_tag",
                            SessionID::FromSerializedValue(456), testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(&session_tab),
                               testing::Return(true)));

  EXPECT_CALL(restore_tab_callback_,
              Run(web_contents(), TabHasUrl(GURL("https://www.google.com")),
                  WindowOpenDisposition::CURRENT_TAB));

  ui::mojom::ClickModifiersPtr modifiers = ui::mojom::ClickModifiers::New();
  handler_->OpenForeignSessionTab("my_session_tag", 456, std::move(modifiers));
}

TEST_F(ForeignSessionHandlerTest, OpenForeignSessionTabMiddleClick) {
  ::sessions::SessionTab session_tab;
  session_tab.navigations.emplace_back();
  session_tab.navigations.back().set_virtual_url(
      GURL("https://www.google.com"));

  EXPECT_CALL(*session_sync_service()->GetOpenTabsUIDelegate(),
              GetForeignTab("my_session_tag",
                            SessionID::FromSerializedValue(456), testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(&session_tab),
                               testing::Return(true)));

  EXPECT_CALL(restore_tab_callback_,
              Run(web_contents(), TabHasUrl(GURL("https://www.google.com")),
                  WindowOpenDisposition::NEW_BACKGROUND_TAB));

  ui::mojom::ClickModifiersPtr modifiers = ui::mojom::ClickModifiers::New();
  modifiers->middle_button = true;
  handler_->OpenForeignSessionTab("my_session_tag", 456, std::move(modifiers));
}

TEST_F(ForeignSessionHandlerTest, DeleteForeignSession) {
  EXPECT_CALL(*session_sync_service()->GetOpenTabsUIDelegate(),
              DeleteForeignSession("my_session_tag"))
      .Times(testing::AtLeast(1));

  handler()->DeleteForeignSession("my_session_tag");
}

TEST_F(ForeignSessionHandlerTest, SetForeignSessionCollapsed) {
  EXPECT_FALSE(profile()
                   ->GetPrefs()
                   ->GetDict(prefs::kNtpCollapsedForeignSessions)
                   .FindBool("my_session_tag")
                   .value_or(false));
  handler()->SetForeignSessionCollapsed("my_session_tag", true);
  EXPECT_TRUE(profile()
                  ->GetPrefs()
                  ->GetDict(prefs::kNtpCollapsedForeignSessions)
                  .FindBool("my_session_tag")
                  .value_or(false));
  handler()->SetForeignSessionCollapsed("my_session_tag", false);
  EXPECT_FALSE(profile()
                   ->GetPrefs()
                   ->GetDict(prefs::kNtpCollapsedForeignSessions)
                   .FindBool("my_session_tag")
                   .value_or(false));
}

class ForeignSessionHandlerSidePanelTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    tab_strip_model_delegate_.SetBrowserWindowInterface(
        &mock_browser_window_interface_);
    tab_strip_model_ =
        std::make_unique<TabStripModel>(&tab_strip_model_delegate_, profile());

    ON_CALL(mock_browser_window_interface_, GetTabStripModel())
        .WillByDefault(testing::Return(tab_strip_model_.get()));

    // Need to have a web contents for the window.
    tab_strip_model_->AppendWebContents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr),
        true);
  }

  void CreateSidePanelUI(
      base::WeakPtr<TopChromeWebUIController::Embedder> embedder = nullptr) {
    webui_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(webui_web_contents_.get());
    side_panel_ui_ =
        std::make_unique<TabsFromOtherDevicesSidePanelUI>(web_ui_.get());
    side_panel_ui_->SetBrowserWindowInterface(&mock_browser_window_interface_);
    side_panel_ui_->set_embedder(embedder);

    handler_ = std::make_unique<ForeignSessionHandler>(
        handler_remote_.BindNewPipeAndPassReceiver(), profile(),
        web_ui_->GetWebContents(), restore_tab_callback_.Get(),
        base::DoNothing(), side_panel_ui_.get());
    handler_->SetPage(page_.BindAndGetRemote());
  }

  void TearDown() override {
    handler_.reset();
    side_panel_ui_.reset();
    web_ui_.reset();
    webui_web_contents_.reset();
    tab_strip_model_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {
        TestingProfile::TestingFactory{
            SessionSyncServiceFactory::GetInstance(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<FakeSessionSyncService>();
            })},
        TestingProfile::TestingFactory{
            DeviceInfoSyncServiceFactory::GetInstance(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::FakeDeviceInfoSyncService>();
            })},
    };
  }

  FakeSessionSyncService* session_sync_service() {
    return static_cast<FakeSessionSyncService*>(
        SessionSyncServiceFactory::GetForProfile(profile()));
  }

 protected:
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;

  const tabs::TabModel::PreventFeatureInitializationForTesting
      prevent_feature_initialization_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;

  std::unique_ptr<content::WebContents> webui_web_contents_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<TabsFromOtherDevicesSidePanelUI> side_panel_ui_;
  MockForeignSessionPage page_;
  mojo::Remote<history::mojom::ForeignSessionPageHandler> handler_remote_;
  std::unique_ptr<ForeignSessionHandler> handler_;

  base::MockCallback<ForeignSessionHandler::RestoreForeignSessionTabCallback>
      restore_tab_callback_;
};

TEST_F(ForeignSessionHandlerSidePanelTest, SetPageShowsSidePanelUI) {
  auto embedder = std::make_unique<MockEmbedder>();
  EXPECT_CALL(*embedder, ShowUI());
  CreateSidePanelUI(embedder->GetWeakPtr());
}

TEST_F(ForeignSessionHandlerSidePanelTest,
       OpenForeignSessionTabWithSidePanelLeftClick) {
  CreateSidePanelUI();

  // Create a fake session.
  ::sessions::SessionTab session_tab;
  session_tab.navigations.emplace_back();
  session_tab.navigations.back().set_virtual_url(
      GURL("https://www.google.com"));

  const ::sessions::SessionTab* returned_session_tab = &session_tab;
  EXPECT_CALL(*session_sync_service()->GetOpenTabsUIDelegate(),
              GetForeignTab("my_session_tag",
                            SessionID::FromSerializedValue(456), testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(returned_session_tab),
                               testing::Return(true)));

  // Perform a left click so that it replaces the current tab.
  ui::mojom::ClickModifiersPtr modifiers = ui::mojom::ClickModifiers::New();

  // The restore callback should be run with the active WebContents (*not* the
  // WebContents hosting the side panel), and with CURRENT_TAB corresponding to
  // left-click.
  EXPECT_CALL(restore_tab_callback_,
              Run(tab_strip_model_->GetActiveWebContents(),
                  TabHasUrl(GURL("https://www.google.com")),
                  WindowOpenDisposition::CURRENT_TAB));

  handler_->OpenForeignSessionTab("my_session_tag", 456, std::move(modifiers));
}

TEST_F(ForeignSessionHandlerSidePanelTest,
       OpenForeignSessionTabWithSidePanelMiddleClick) {
  CreateSidePanelUI();

  // Create a fake session.
  ::sessions::SessionTab session_tab;
  session_tab.navigations.emplace_back();
  session_tab.navigations.back().set_virtual_url(
      GURL("https://www.google.com"));

  const ::sessions::SessionTab* returned_session_tab = &session_tab;
  EXPECT_CALL(*session_sync_service()->GetOpenTabsUIDelegate(),
              GetForeignTab("my_session_tag",
                            SessionID::FromSerializedValue(456), testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(returned_session_tab),
                               testing::Return(true)));

  // Perform a middle click so that it adds a background tab.
  ui::mojom::ClickModifiersPtr modifiers = ui::mojom::ClickModifiers::New();
  modifiers->middle_button = true;

  // The restore callback should be run with the active WebContents (*not* the
  // WebContents hosting the side panel), and with NEW_BACKGROUND_TAB
  // corresponding to middle-click.
  EXPECT_CALL(restore_tab_callback_,
              Run(tab_strip_model_->GetActiveWebContents(),
                  TabHasUrl(GURL("https://www.google.com")),
                  WindowOpenDisposition::NEW_BACKGROUND_TAB));

  handler_->OpenForeignSessionTab("my_session_tag", 456, std::move(modifiers));
}

TEST_F(ForeignSessionHandlerSidePanelTest, RecordMetricsOnTabOpen) {
  CreateSidePanelUI();

  TabsFromOtherDevicesSidePanelMetrics metrics;
  metrics.OnEntryShown(nullptr);
  side_panel_ui_->SetMetricsRecorder(metrics.GetWeakPtr());

  ::sessions::SessionTab session_tab;
  session_tab.navigations.emplace_back();
  session_tab.navigations.back().set_virtual_url(
      GURL("https://www.google.com"));

  const ::sessions::SessionTab* returned_session_tab = &session_tab;
  EXPECT_CALL(*session_sync_service()->GetOpenTabsUIDelegate(),
              GetForeignTab("my_session_tag",
                            SessionID::FromSerializedValue(456), testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(returned_session_tab),
                               testing::Return(true)));

  ui::mojom::ClickModifiersPtr modifiers = ui::mojom::ClickModifiers::New();

  base::HistogramTester histogram_tester;

  handler_->OpenForeignSessionTab("my_session_tag", 456, std::move(modifiers));

  histogram_tester.ExpectBucketCount(
      "Sync.TabsFromOtherDevicesSidePanel.List.Events", 3,
      1);  // 3 is kTabOpened

  histogram_tester.ExpectTotalCount(
      "Sync.TabsFromOtherDevicesSidePanel.List.TimeToFirstTab", 1);
}

TEST_F(ForeignSessionHandlerSidePanelTest,
       GetForeignSessions_DuplicateNamesWithSuffixes) {
  CreateSidePanelUI();

  syncer::DeviceInfoSyncService* device_info_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile());
  syncer::FakeDeviceInfoTracker* device_info_tracker =
      static_cast<syncer::FakeDeviceInfoTracker*>(
          device_info_sync_service->GetDeviceInfoTracker());

  // Create two devices with the same name but different channels.
  auto device1 = std::make_unique<syncer::DeviceInfo>(
      "tag1", "My Device", "1.0", "Mozilla/5.0 channel(stable)",
      syncer::DeviceInfo::DeviceType::kPhone,
      syncer::DeviceInfo::OsType::kAndroid,
      syncer::DeviceInfo::FormFactor::kPhone, "id1", "Manufacturer", "Model",
      "FullHWClass", base::Time::Now(), base::TimeDelta(), false,
      syncer::DeviceInfo::SendTabReceivingType::kChromeOrUnspecified,
      std::nullopt, std::nullopt, "fcm1", syncer::DataTypeSet{}, std::nullopt,
      false, MobilePromoOnDesktopPromoTypeSet{},
      syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable);

  auto device2 = std::make_unique<syncer::DeviceInfo>(
      "tag2", "My Device", "1.0", "Mozilla/5.0 channel(canary)",
      syncer::DeviceInfo::DeviceType::kPhone,
      syncer::DeviceInfo::OsType::kAndroid,
      syncer::DeviceInfo::FormFactor::kPhone, "id2", "Manufacturer", "Model",
      "FullHWClass", base::Time::Now(), base::TimeDelta(), false,
      syncer::DeviceInfo::SendTabReceivingType::kChromeOrUnspecified,
      std::nullopt, std::nullopt, "fcm2", syncer::DataTypeSet{}, std::nullopt,
      false, MobilePromoOnDesktopPromoTypeSet{},
      syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable);

  device_info_tracker->Add(std::move(device1));
  device_info_tracker->Add(std::move(device2));

  // Set up fake sessions.
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;
  auto session1 = std::make_unique<sync_sessions::SyncedSession>();
  session1->SetSessionTag("tag1");
  session1->SetSessionName("My Device");

  auto session2 = std::make_unique<sync_sessions::SyncedSession>();
  session2->SetSessionTag("tag2");
  session2->SetSessionName("My Device");

  sessions.push_back(session1.get());
  sessions.push_back(session2.get());

  EXPECT_CALL(*session_sync_service()->GetOpenTabsUIDelegate(),
              GetAllForeignSessions)
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(sessions),
                               testing::Return(true)));

  base::MockCallback<ForeignSessionHandler::GetForeignSessionsCallback>
      callback;

  std::vector<history::mojom::ForeignSessionPtr> result_sessions;
  EXPECT_CALL(callback, Run)
      .WillOnce([&result_sessions](
                    std::vector<history::mojom::ForeignSessionPtr> sessions) {
        result_sessions = std::move(sessions);
      });

  handler_->GetForeignSessions(callback.Get());

  ASSERT_EQ(result_sessions.size(), 2u);
  EXPECT_EQ(result_sessions[0]->name, "My Device");  // Stable gets no suffix
  EXPECT_EQ(result_sessions[1]->name, "My Device (Canary)");
}

TEST_F(ForeignSessionHandlerSidePanelTest,
       GetForeignSessions_ExcludeStableChannel) {
  base::test::ScopedFeatureList feature_list{
      features::kTabsFromOtherDevicesSidePanelExcludeStableChannel};

  CreateSidePanelUI();

  syncer::DeviceInfoSyncService* device_info_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile());
  syncer::FakeDeviceInfoTracker* device_info_tracker =
      static_cast<syncer::FakeDeviceInfoTracker*>(
          device_info_sync_service->GetDeviceInfoTracker());

  // Create one stable and one canary device.
  auto device1 = std::make_unique<syncer::DeviceInfo>(
      "tag1", "Stable Device", "1.0", "Mozilla/5.0 channel(stable)",
      syncer::DeviceInfo::DeviceType::kPhone,
      syncer::DeviceInfo::OsType::kAndroid,
      syncer::DeviceInfo::FormFactor::kPhone, "id1", "Manufacturer", "Model",
      "FullHWClass", base::Time::Now(), base::TimeDelta(), false,
      syncer::DeviceInfo::SendTabReceivingType::kChromeOrUnspecified,
      std::nullopt, std::nullopt, "fcm1", syncer::DataTypeSet{}, std::nullopt,
      false, MobilePromoOnDesktopPromoTypeSet{},
      syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable);

  auto device2 = std::make_unique<syncer::DeviceInfo>(
      "tag2", "Canary Device", "1.0", "Mozilla/5.0 channel(canary)",
      syncer::DeviceInfo::DeviceType::kPhone,
      syncer::DeviceInfo::OsType::kAndroid,
      syncer::DeviceInfo::FormFactor::kPhone, "id2", "Manufacturer", "Model",
      "FullHWClass", base::Time::Now(), base::TimeDelta(), false,
      syncer::DeviceInfo::SendTabReceivingType::kChromeOrUnspecified,
      std::nullopt, std::nullopt, "fcm2", syncer::DataTypeSet{}, std::nullopt,
      false, MobilePromoOnDesktopPromoTypeSet{},
      syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable);

  device_info_tracker->Add(std::move(device1));
  device_info_tracker->Add(std::move(device2));

  // Set up fake sessions.
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;
  auto session1 = std::make_unique<sync_sessions::SyncedSession>();
  session1->SetSessionTag("tag1");
  session1->SetSessionName("Stable Device");

  auto session2 = std::make_unique<sync_sessions::SyncedSession>();
  session2->SetSessionTag("tag2");
  session2->SetSessionName("Canary Device");

  sessions.push_back(session1.get());
  sessions.push_back(session2.get());

  EXPECT_CALL(*session_sync_service()->GetOpenTabsUIDelegate(),
              GetAllForeignSessions)
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(sessions),
                               testing::Return(true)));

  base::MockCallback<ForeignSessionHandler::GetForeignSessionsCallback>
      callback;

  std::vector<history::mojom::ForeignSessionPtr> result_sessions;
  EXPECT_CALL(callback, Run)
      .WillOnce([&result_sessions](
                    std::vector<history::mojom::ForeignSessionPtr> sessions) {
        result_sessions = std::move(sessions);
      });

  handler_->GetForeignSessions(callback.Get());

  // Only the canary device should be returned.
  ASSERT_EQ(result_sessions.size(), 1u);
  EXPECT_EQ(result_sessions[0]->name, "Canary Device");
}

}  // namespace browser_sync
