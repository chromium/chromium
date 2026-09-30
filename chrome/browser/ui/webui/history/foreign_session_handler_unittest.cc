// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/foreign_session_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/fake_open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/window_open_disposition.h"

namespace browser_sync {

namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

MATCHER_P(HasSessionTag, tag, "") {
  return arg && arg->GetSessionTag() == tag;
}

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

  sync_sessions::FakeOpenTabsUIDelegate* GetOpenTabsUIDelegate() override {
    return &open_tabs_ui_delegate_;
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
  sync_sessions::FakeOpenTabsUIDelegate open_tabs_ui_delegate_;
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
        handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
        profile(), web_contents(), restore_tab_callback_.Get(),
        restore_windows_callback_.Get());
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
  session_sync_service()->GetOpenTabsUIDelegate()->AddTabToForeignSession(
      "my_session_tag");

  const sessions::SessionWindow* window_ptr =
      session_sync_service()->GetOpenTabsUIDelegate()->GetForeignSession(
          "my_session_tag")[0];

  EXPECT_CALL(restore_windows_callback_,
              Run(profile(), ElementsAre(window_ptr)));

  handler()->OpenForeignSessionAllTabs("my_session_tag");
}

TEST_F(ForeignSessionHandlerTest, OpenForeignSessionTabLeftClick) {
  sessions::SessionTab* tab =
      session_sync_service()->GetOpenTabsUIDelegate()->AddTabToForeignSession(
          "my_session_tag", GURL("https://www.google.com"));

  EXPECT_CALL(restore_tab_callback_,
              Run(web_contents(), TabHasUrl(GURL("https://www.google.com")),
                  WindowOpenDisposition::CURRENT_TAB));

  ui::mojom::ClickModifiersPtr modifiers = ui::mojom::ClickModifiers::New();
  handler_->OpenForeignSessionTab("my_session_tag", tab->tab_id.id(),
                                  std::move(modifiers));
}

TEST_F(ForeignSessionHandlerTest, OpenForeignSessionTabMiddleClick) {
  sessions::SessionTab* tab =
      session_sync_service()->GetOpenTabsUIDelegate()->AddTabToForeignSession(
          "my_session_tag", GURL("https://www.google.com"));

  EXPECT_CALL(restore_tab_callback_,
              Run(web_contents(), TabHasUrl(GURL("https://www.google.com")),
                  WindowOpenDisposition::NEW_BACKGROUND_TAB));

  ui::mojom::ClickModifiersPtr modifiers = ui::mojom::ClickModifiers::New();
  modifiers->middle_button = true;
  handler_->OpenForeignSessionTab("my_session_tag", tab->tab_id.id(),
                                  std::move(modifiers));
}

TEST_F(ForeignSessionHandlerTest, DeleteForeignSession) {
  sync_sessions::FakeOpenTabsUIDelegate* delegate =
      session_sync_service()->GetOpenTabsUIDelegate();
  delegate->AddForeignSession("session_to_delete");
  delegate->AddForeignSession("session_to_keep");

  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;
  ASSERT_TRUE(delegate->GetAllForeignSessions(&sessions));
  ASSERT_THAT(sessions, UnorderedElementsAre(HasSessionTag("session_to_delete"),
                                             HasSessionTag("session_to_keep")));

  handler()->DeleteForeignSession("session_to_delete");

  sessions.clear();
  delegate->GetAllForeignSessions(&sessions);
  EXPECT_THAT(sessions, ElementsAre(HasSessionTag("session_to_keep")));
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

}  // namespace

}  // namespace browser_sync
