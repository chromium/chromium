// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/foreign_session_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/mock_open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"

namespace browser_sync {

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

class ForeignSessionHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    handler_ = std::make_unique<ForeignSessionHandler>(
        handler_remote_.BindNewPipeAndPassReceiver(), profile(),
        web_contents());
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
};

TEST_F(ForeignSessionHandlerTest, ShouldFireForeignSessionsChanged) {
  EXPECT_CALL(page_, OnForeignSessionsChanged(testing::_));

  session_sync_service()->NotifyForeignSessionsChanged();
}

TEST_F(ForeignSessionHandlerTest, OpenForeignSessionAllTabs) {
  EXPECT_CALL(*session_sync_service()->GetOpenTabsUIDelegate(),
              GetForeignSession("my_session_tag"))
      .Times(testing::AtLeast(1));

  handler()->OpenForeignSessionAllTabs("my_session_tag");
}

TEST_F(ForeignSessionHandlerTest, OpenForeignSessionTab) {
  EXPECT_CALL(*session_sync_service()->GetOpenTabsUIDelegate(),
              GetForeignTab("my_session_tag",
                            SessionID::FromSerializedValue(456), testing::_))
      .Times(testing::AtLeast(1));

  handler()->OpenForeignSessionTab("my_session_tag", 456,
                                   ui::mojom::ClickModifiers::New());
}

}  // namespace browser_sync
