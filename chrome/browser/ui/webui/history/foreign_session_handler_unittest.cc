// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/foreign_session_handler.h"

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

class MockOpenTabsUIDelegate : public sync_sessions::OpenTabsUIDelegate {
 public:
  MockOpenTabsUIDelegate() = default;

  MOCK_METHOD1(GetAllForeignSessions,
               bool(std::vector<raw_ptr<const sync_sessions::SyncedSession,
                                        VectorExperimental>>* sessions));

  MOCK_METHOD3(GetForeignTab,
               bool(const std::string& tag,
                    const SessionID tab_id,
                    const sessions::SessionTab** tab));

  MOCK_METHOD1(DeleteForeignSession, void(const std::string& tag));

  MOCK_METHOD1(
      GetForeignSession,
      std::vector<const sessions::SessionWindow*>(const std::string& tag));

  MOCK_METHOD2(GetForeignSessionTabs,
               bool(const std::string& tag,
                    std::vector<const sessions::SessionTab*>* tabs));

  MOCK_METHOD1(GetLocalSession,
               bool(const sync_sessions::SyncedSession** local_session));
};

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

  MockOpenTabsUIDelegate* GetOpenTabsUIDelegate() override {
    return &mock_open_tabs_ui_delegate_;
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
  MockOpenTabsUIDelegate mock_open_tabs_ui_delegate_;
};

class ForeignSessionHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents());

    handler_ = std::make_unique<ForeignSessionHandler>();
    handler_->SetWebUIForTesting(web_ui_.get());
  }

  void TearDown() override {
    handler_.reset();
    web_ui_.reset();
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

  content::TestWebUI* web_ui() { return web_ui_.get(); }

  ForeignSessionHandler* handler() { return handler_.get(); }

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<ForeignSessionHandler> handler_;
};

TEST_F(ForeignSessionHandlerTest,
       ShouldFireForeignSessionsChangedWhileJavascriptAllowed) {
  handler()->AllowJavascriptForTesting();
  ASSERT_TRUE(handler()->IsJavascriptAllowed());

  web_ui()->ClearTrackedCalls();
  session_sync_service()->NotifyForeignSessionsChanged();

  ASSERT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& call_data = *web_ui()->call_data()[0];
  EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
  EXPECT_EQ("foreign-sessions-changed", call_data.arg1()->GetString());
}

TEST_F(ForeignSessionHandlerTest,
       ShouldNotFireForeignSessionsChangedBeforeJavascriptAllowed) {
  ASSERT_FALSE(handler()->IsJavascriptAllowed());

  web_ui()->ClearTrackedCalls();
  session_sync_service()->NotifyForeignSessionsChanged();

  EXPECT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(ForeignSessionHandlerTest,
       ShouldNotFireForeignSessionsChangedAfterJavascriptDisallowed) {
  handler()->AllowJavascriptForTesting();
  ASSERT_TRUE(handler()->IsJavascriptAllowed());
  handler()->DisallowJavascript();
  ASSERT_FALSE(handler()->IsJavascriptAllowed());

  web_ui()->ClearTrackedCalls();
  session_sync_service()->NotifyForeignSessionsChanged();

  EXPECT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(ForeignSessionHandlerTest, HandleOpenForeignSessionAllTabs) {
  EXPECT_CALL(*session_sync_service()->GetOpenTabsUIDelegate(),
              GetForeignSession("my_session_tag"))
      .Times(testing::AtLeast(1));

  base::Value::List list_args;
  list_args.Append("my_session_tag");
  handler()->HandleOpenForeignSessionAllTabs(list_args);
}

TEST_F(ForeignSessionHandlerTest, HandleOpenForeignSessionTab) {
  EXPECT_CALL(*session_sync_service()->GetOpenTabsUIDelegate(),
              GetForeignTab("my_session_tag",
                            SessionID::FromSerializedValue(456), testing::_))
      .Times(testing::AtLeast(1));

  base::Value::List list_args;
  list_args.Append("my_session_tag");
  list_args.Append("456");
  list_args.Append(1.0);
  list_args.Append(false);
  list_args.Append(false);
  list_args.Append(false);
  list_args.Append(false);
  handler()->HandleOpenForeignSessionTab(list_args);
}

}  // namespace browser_sync
