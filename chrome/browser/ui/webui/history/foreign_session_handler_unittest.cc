// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/foreign_session_handler.h"

#include "base/callback_list.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

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

  sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate() override {
    return nullptr;
  }

  base::CallbackListSubscription SubscribeToForeignSessionsChanged(
      const base::RepeatingClosure& cb) override {
    return subscriber_list_.Add(cb);
  }

  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate()
      override {
    return nullptr;
  }

  void ProxyTabsStateChanged(syncer::DataTypeController::State state) override {
  }

 private:
  base::RepeatingClosureList subscriber_list_;
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
        {SessionSyncServiceFactory::GetInstance(),
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

}  // namespace

}  // namespace browser_sync
