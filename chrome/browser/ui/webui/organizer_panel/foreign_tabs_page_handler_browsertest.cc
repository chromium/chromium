// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/foreign_tabs_page_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/organizer_panel/foreign_tabs.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync_sessions/fake_open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/sessions_global_id_mapper.h"
#include "components/sync_sessions/synced_session.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class FakeSessionSyncService : public sync_sessions::SessionSyncService {
 public:
  FakeSessionSyncService() = default;
  ~FakeSessionSyncService() override = default;

  syncer::GlobalIdMapper* GetGlobalIdMapper() const override {
    return &global_id_mapper_;
  }

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
  mutable sync_sessions::SessionsGlobalIdMapper global_id_mapper_;
};

class ForeignTabsPageHandlerBrowserTest : public InProcessBrowserTest {
 public:
  ForeignTabsPageHandlerBrowserTest() {
    dependency_manager_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ForeignTabsPageHandlerBrowserTest::RegisterFakeServices,
                base::Unretained(this)));
  }

  void RegisterFakeServices(content::BrowserContext* context) {
    SessionSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<FakeSessionSyncService>();
        }));
  }

  FakeSessionSyncService* session_sync_service() {
    return static_cast<FakeSessionSyncService*>(
        SessionSyncServiceFactory::GetInstance()->GetForProfile(
            browser()->GetProfile()));
  }

 private:
  base::CallbackListSubscription dependency_manager_subscription_;
};

IN_PROC_BROWSER_TEST_F(ForeignTabsPageHandlerBrowserTest,
                       EmptyWhenNoForeignSessions) {
  mojo::Remote<organizer_panel::mojom::ForeignTabsPageHandler> remote;
  ForeignTabsPageHandler handler(
      remote.BindNewPipeAndPassReceiver(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  base::test::TestFuture<std::vector<organizer_panel::mojom::ForeignTabPtr>>
      future;
  remote->GetForeignTabs(future.GetCallback());

  EXPECT_TRUE(future.Take().empty());
}

IN_PROC_BROWSER_TEST_F(ForeignTabsPageHandlerBrowserTest,
                       ReturnsTabsSortedByRecency) {
  auto* open_tabs = session_sync_service()->GetOpenTabsUIDelegate();

  base::Time now = base::Time::Now();

  // Create session 1: Phone, older
  sync_sessions::SyncedSession* session1 =
      open_tabs->AddForeignSession("session_phone", now - base::Minutes(20));
  session1->SetSessionName("Pixel 8");

  // Create session 2: Laptop, newer
  sync_sessions::SyncedSession* session2 =
      open_tabs->AddForeignSession("session_laptop", now - base::Minutes(5));
  session2->SetSessionName("MacBook Pro");

  // Tab on Phone: 15 minutes ago
  sessions::SessionTab* tab_phone = open_tabs->AddTabToForeignSession(
      "session_phone", GURL("https://www.google.com"));
  tab_phone->timestamp = now - base::Minutes(15);
  tab_phone->navigations[0].set_title(u"Google");

  // Tab 1 on Laptop: 2 minutes ago
  sessions::SessionTab* tab_laptop1 = open_tabs->AddTabToForeignSession(
      "session_laptop", GURL("https://www.youtube.com"));
  tab_laptop1->timestamp = now - base::Minutes(2);
  tab_laptop1->navigations[0].set_title(u"YouTube");

  // Tab 2 on Laptop: 30 minutes ago
  sessions::SessionTab* tab_laptop2 = open_tabs->AddTabToForeignSession(
      "session_laptop", GURL("https://www.github.com"));
  tab_laptop2->timestamp = now - base::Minutes(30);
  tab_laptop2->navigations[0].set_title(u"GitHub");

  mojo::Remote<organizer_panel::mojom::ForeignTabsPageHandler> remote;
  ForeignTabsPageHandler handler(
      remote.BindNewPipeAndPassReceiver(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  base::test::TestFuture<std::vector<organizer_panel::mojom::ForeignTabPtr>>
      future;
  remote->GetForeignTabs(future.GetCallback());

  auto tabs = future.Take();
  ASSERT_EQ(3u, tabs.size());

  // 1st: YouTube (MacBook Pro, 2 mins ago)
  EXPECT_EQ("YouTube", tabs[0]->title);
  EXPECT_EQ("https://www.youtube.com/", tabs[0]->url.spec());
  EXPECT_EQ("MacBook Pro", tabs[0]->device_name);
  EXPECT_FALSE(tabs[0]->last_active_elapsed_text.empty());

  // 2nd: Google (Pixel 8, 15 mins ago)
  EXPECT_EQ("Google", tabs[1]->title);
  EXPECT_EQ("https://www.google.com/", tabs[1]->url.spec());
  EXPECT_EQ("Pixel 8", tabs[1]->device_name);
  EXPECT_FALSE(tabs[1]->last_active_elapsed_text.empty());

  // 3rd: GitHub (MacBook Pro, 30 mins ago)
  EXPECT_EQ("GitHub", tabs[2]->title);
  EXPECT_EQ("https://www.github.com/", tabs[2]->url.spec());
  EXPECT_EQ("MacBook Pro", tabs[2]->device_name);
  EXPECT_FALSE(tabs[2]->last_active_elapsed_text.empty());
}

IN_PROC_BROWSER_TEST_F(ForeignTabsPageHandlerBrowserTest,
                       UsesUrlAsTitleWhenTitleIsEmpty) {
  auto* open_tabs = session_sync_service()->GetOpenTabsUIDelegate();

  sync_sessions::SyncedSession* session =
      open_tabs->AddForeignSession("session_device", base::Time::Now());
  session->SetSessionName("Chromebook");

  sessions::SessionTab* tab = open_tabs->AddTabToForeignSession(
      "session_device", GURL("https://example.com/page"));
  tab->timestamp = base::Time::Now();
  tab->navigations[0].set_title(u"");

  mojo::Remote<organizer_panel::mojom::ForeignTabsPageHandler> remote;
  ForeignTabsPageHandler handler(
      remote.BindNewPipeAndPassReceiver(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  base::test::TestFuture<std::vector<organizer_panel::mojom::ForeignTabPtr>>
      future;
  remote->GetForeignTabs(future.GetCallback());

  auto tabs = future.Take();
  ASSERT_EQ(1u, tabs.size());
  EXPECT_EQ("https://example.com/page", tabs[0]->title);
  EXPECT_EQ("Chromebook", tabs[0]->device_name);
}

}  // namespace
