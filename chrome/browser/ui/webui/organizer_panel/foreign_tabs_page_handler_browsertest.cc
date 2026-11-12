// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/foreign_tabs_page_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/organizer_panel/foreign_tabs.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_sessions/fake_open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/sessions_global_id_mapper.h"
#include "components/sync_sessions/synced_session.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/cross_device_signin_promo_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager_test_api.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/test_device_info_builder.h"
#include "content/public/test/browser_test_utils.h"
#endif

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
    DeviceInfoSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::FakeDeviceInfoSyncService>();
        }));
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::TestSyncService>();
        }));
  }

  FakeSessionSyncService* session_sync_service() {
    return static_cast<FakeSessionSyncService*>(
        SessionSyncServiceFactory::GetInstance()->GetForProfile(
            browser()->GetProfile()));
  }

  syncer::FakeDeviceInfoTracker* device_info_tracker() {
    auto* service = static_cast<syncer::FakeDeviceInfoSyncService*>(
        DeviceInfoSyncServiceFactory::GetForProfile(browser()->GetProfile()));
    return static_cast<syncer::FakeDeviceInfoTracker*>(
        service->GetDeviceInfoTracker());
  }

  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(browser()->GetProfile()));
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

  base::test::TestFuture<std::vector<organizer_panel::mojom::ForeignTabPtr>,
                         bool>
      future;
  remote->GetForeignTabs(future.GetCallback());

  auto [tabs, can_show_promo] = future.Take();
  EXPECT_TRUE(tabs.empty());
  EXPECT_FALSE(can_show_promo);
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

  base::test::TestFuture<std::vector<organizer_panel::mojom::ForeignTabPtr>,
                         bool>
      future;
  remote->GetForeignTabs(future.GetCallback());

  auto tabs = std::get<0>(future.Take());
  ASSERT_EQ(3u, tabs.size());

  // 1st: YouTube (MacBook Pro, 2 mins ago)
  EXPECT_EQ("session_laptop", tabs[0]->session_tag);
  EXPECT_EQ(tab_laptop1->tab_id.id(), tabs[0]->session_tab_id);
  EXPECT_EQ("YouTube", tabs[0]->title);
  EXPECT_EQ("https://www.youtube.com/", tabs[0]->url.spec());
  EXPECT_EQ("MacBook Pro", tabs[0]->device_name);
  EXPECT_FALSE(tabs[0]->last_active_elapsed_text.empty());

  // 2nd: Google (Pixel 8, 15 mins ago)
  EXPECT_EQ("session_phone", tabs[1]->session_tag);
  EXPECT_EQ(tab_phone->tab_id.id(), tabs[1]->session_tab_id);
  EXPECT_EQ("Google", tabs[1]->title);
  EXPECT_EQ("https://www.google.com/", tabs[1]->url.spec());
  EXPECT_EQ("Pixel 8", tabs[1]->device_name);
  EXPECT_FALSE(tabs[1]->last_active_elapsed_text.empty());

  // 3rd: GitHub (MacBook Pro, 30 mins ago)
  EXPECT_EQ("session_laptop", tabs[2]->session_tag);
  EXPECT_EQ(tab_laptop2->tab_id.id(), tabs[2]->session_tab_id);
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

  base::test::TestFuture<std::vector<organizer_panel::mojom::ForeignTabPtr>,
                         bool>
      future;
  remote->GetForeignTabs(future.GetCallback());

  auto tabs = std::get<0>(future.Take());
  ASSERT_EQ(1u, tabs.size());
  EXPECT_EQ("session_device", tabs[0]->session_tag);
  EXPECT_EQ(tab->tab_id.id(), tabs[0]->session_tab_id);
  EXPECT_EQ("https://example.com/page", tabs[0]->title);
  EXPECT_EQ("Chromebook", tabs[0]->device_name);
}

IN_PROC_BROWSER_TEST_F(ForeignTabsPageHandlerBrowserTest, OpenTab) {
  auto* open_tabs = session_sync_service()->GetOpenTabsUIDelegate();
  open_tabs->AddForeignSession("session_laptop", base::Time::Now());
  const GURL test_url("https://www.example.com/");
  sessions::SessionTab* session_tab =
      open_tabs->AddTabToForeignSession("session_laptop", test_url);

  mojo::Remote<organizer_panel::mojom::ForeignTabsPageHandler> remote;
  ForeignTabsPageHandler handler(
      remote.BindNewPipeAndPassReceiver(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  int initial_tab_count = browser()->GetTabStripModel()->count();

  remote->OpenTab("session_laptop", session_tab->tab_id.id());
  remote.FlushForTesting();

  EXPECT_EQ(initial_tab_count + 1, browser()->GetTabStripModel()->count());
  EXPECT_EQ(
      test_url,
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(ForeignTabsPageHandlerBrowserTest,
                       OpenTabInvalidTabIdDoesNothing) {
  auto* open_tabs = session_sync_service()->GetOpenTabsUIDelegate();
  open_tabs->AddForeignSession("session_laptop", base::Time::Now());
  open_tabs->AddTabToForeignSession("session_laptop",
                                    GURL("https://www.example.com/"));

  mojo::Remote<organizer_panel::mojom::ForeignTabsPageHandler> remote;
  ForeignTabsPageHandler handler(
      remote.BindNewPipeAndPassReceiver(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  int initial_tab_count = browser()->GetTabStripModel()->count();

  remote->OpenTab("session_laptop", -1);
  remote.FlushForTesting();

  EXPECT_EQ(initial_tab_count, browser()->GetTabStripModel()->count());

  remote->OpenTab("nonexistent_session", 12345);
  remote.FlushForTesting();

  EXPECT_EQ(initial_tab_count, browser()->GetTabStripModel()->count());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class ForeignTabsPageHandlerPromoBrowserTest
    : public ForeignTabsPageHandlerBrowserTest {
 public:
  ForeignTabsPageHandlerPromoBrowserTest() {
    // Intentionally leave organizer_panel::kOrganizerPanelForeignTabs disabled
    // so OrganizerPanelUI does not construct a second ForeignTabsPageHandler
    // when SetOrganizerVisible(true) creates the panel WebContents.
    scoped_feature_list_.InitWithFeatures(
        {organizer_panel::kOrganizerPanel,
         switches::kCrossDeviceSigninFromDesktop,
         switches::kCrossDeviceSigninFromDesktopFollowup},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ForeignTabsPageHandlerPromoBrowserTest,
                       ShowCrossDeviceSignInPromo) {
  base::HistogramTester histogram_tester;

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  signin::MakePrimaryAccountAvailable(identity_manager, "user@example.com",
                                      signin::ConsentLevel::kSignin);

  mojo::Remote<organizer_panel::mojom::ForeignTabsPageHandler> remote;
  ForeignTabsPageHandler handler(
      remote.BindNewPipeAndPassReceiver(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  remote->ShowCrossDeviceSignInPromo();
  remote.FlushForTesting();

  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.OpenedQrCodeBubble",
      CrossDeviceSigninPromoEntryPoint::kForeignTabs, 1);
  EXPECT_TRUE(IsCrossDeviceSigninPromoBubbleOpen(browser()->GetProfile()));
}

IN_PROC_BROWSER_TEST_F(ForeignTabsPageHandlerPromoBrowserTest,
                       ChecksPromoEligibilityOnEachPanelOpen) {
  base::HistogramTester histogram_tester;
  auto* const controller = OrganizerPanelController::From(browser());
  ASSERT_TRUE(controller);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  signin::MakePrimaryAccountAvailable(identity_manager, "user@example.com",
                                      signin::ConsentLevel::kSignin);
  sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, /*is_type_on=*/true);

  mojo::Remote<organizer_panel::mojom::ForeignTabsPageHandler> remote;
  ForeignTabsPageHandler handler(
      remote.BindNewPipeAndPassReceiver(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  // Before the organizer panel opens, promo eligibility is not checked.
  {
    base::test::TestFuture<std::vector<organizer_panel::mojom::ForeignTabPtr>,
                           bool>
        future;
    remote->GetForeignTabs(future.GetCallback());
    EXPECT_FALSE(std::get<1>(future.Take()));
    histogram_tester.ExpectTotalCount(
        "Signin.CrossDeviceSigninPromo.ShouldShowResult.ForeignTabs", 0);
  }

  // Opening the panel checks promo eligibility lazily on the first empty
  // GetForeignTabs() call and caches it while the panel remains open.
  controller->SetOrganizerVisible(true, /*immediate=*/true);
  histogram_tester.ExpectTotalCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.ForeignTabs", 0);
  {
    base::test::TestFuture<std::vector<organizer_panel::mojom::ForeignTabPtr>,
                           bool>
        future;
    remote->GetForeignTabs(future.GetCallback());
    EXPECT_TRUE(std::get<1>(future.Take()));
  }
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.ForeignTabs",
      CrossDeviceSigninPromoShouldShowResult::kCanShow, 1);
  {
    base::test::TestFuture<std::vector<organizer_panel::mojom::ForeignTabPtr>,
                           bool>
        future;
    remote->GetForeignTabs(future.GetCallback());
    EXPECT_TRUE(std::get<1>(future.Take()));
  }
  histogram_tester.ExpectTotalCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.ForeignTabs", 1);

  // Closing the panel does not check promo eligibility and resets the cache.
  controller->SetOrganizerVisible(false, /*immediate=*/true);
  histogram_tester.ExpectTotalCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.ForeignTabs", 1);
  {
    base::test::TestFuture<std::vector<organizer_panel::mojom::ForeignTabPtr>,
                           bool>
        future;
    remote->GetForeignTabs(future.GetCallback());
    EXPECT_FALSE(std::get<1>(future.Take()));
  }
  histogram_tester.ExpectTotalCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.ForeignTabs", 1);

  // When foreign tabs exist (e.g. from another desktop), reopening does not
  // evaluate promo eligibility or record the histogram.
  auto* open_tabs = session_sync_service()->GetOpenTabsUIDelegate();
  open_tabs->AddForeignSession("session_desktop", base::Time::Now());
  open_tabs->AddTabToForeignSession("session_desktop",
                                    GURL("https://www.example.com/"));
  controller->SetOrganizerVisible(true, /*immediate=*/true);
  {
    base::test::TestFuture<std::vector<organizer_panel::mojom::ForeignTabPtr>,
                           bool>
        future;
    remote->GetForeignTabs(future.GetCallback());
    auto [tabs, can_show_promo] = future.Take();
    EXPECT_EQ(1u, tabs.size());
    EXPECT_FALSE(can_show_promo);
  }
  histogram_tester.ExpectTotalCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.ForeignTabs", 1);

  // Removing the foreign tabs and adding a synced phone: reopening with empty
  // tabs re-checks eligibility and returns false.
  controller->SetOrganizerVisible(false, /*immediate=*/true);
  open_tabs->DeleteForeignSession("session_desktop");
  device_info_tracker()->Add(
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kAndroid)
          .WithGuid("phone_guid")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kPhone)
          .WithLastUpdatedTimestamp(base::Time::Now())
          .Build());

  controller->SetOrganizerVisible(true, /*immediate=*/true);
  {
    base::test::TestFuture<std::vector<organizer_panel::mojom::ForeignTabPtr>,
                           bool>
        future;
    remote->GetForeignTabs(future.GetCallback());
    EXPECT_FALSE(std::get<1>(future.Take()));
  }
  histogram_tester.ExpectTotalCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.ForeignTabs", 2);
  histogram_tester.ExpectBucketCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.ForeignTabs",
      CrossDeviceSigninPromoShouldShowResult::kHasMobileDevice, 1);
}

IN_PROC_BROWSER_TEST_F(ForeignTabsPageHandlerPromoBrowserTest,
                       EvaluatesPromoWhenConstructedAfterPanelAlreadyOpen) {
  base::HistogramTester histogram_tester;
  auto* const controller = OrganizerPanelController::From(browser());
  ASSERT_TRUE(controller);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  signin::MakePrimaryAccountAvailable(identity_manager, "user@example.com",
                                      signin::ConsentLevel::kSignin);
  sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, /*is_type_on=*/true);

  controller->SetOrganizerVisible(true, /*immediate=*/true);

  mojo::Remote<organizer_panel::mojom::ForeignTabsPageHandler> remote;
  ForeignTabsPageHandler handler(
      remote.BindNewPipeAndPassReceiver(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  base::test::TestFuture<std::vector<organizer_panel::mojom::ForeignTabPtr>,
                         bool>
      future;
  remote->GetForeignTabs(future.GetCallback());
  EXPECT_TRUE(std::get<1>(future.Take()));
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.ForeignTabs",
      CrossDeviceSigninPromoShouldShowResult::kCanShow, 1);
}

class ForeignTabsPageHandlerPreloadPromoBrowserTest
    : public ForeignTabsPageHandlerBrowserTest {
 public:
  ForeignTabsPageHandlerPreloadPromoBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {organizer_panel::kOrganizerPanel,
         organizer_panel::kOrganizerPanelForeignTabs,
         features::kPreloadTopChromeWebUI,
         features::kPreloadTopChromeWebUIHideWhenReady,
         switches::kCrossDeviceSigninFromDesktop,
         switches::kCrossDeviceSigninFromDesktopFollowup},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ForeignTabsPageHandlerPreloadPromoBrowserTest,
                       PreloadDoesNotCheckPromoUntilPanelOpens) {
  base::HistogramTester histogram_tester;
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  signin::MakePrimaryAccountAvailable(identity_manager, "user@example.com",
                                      signin::ConsentLevel::kSignin);
  sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, /*is_type_on=*/true);

  WebUIContentsPreloadManagerTestAPI preload_api;
  preload_api.PreloadUrl(browser()->GetProfile(),
                         GURL(chrome::kChromeUIOrganizerPanelURL));
  content::WebContents* const preloaded_contents =
      preload_api.GetPreloadedWebContents();
  ASSERT_TRUE(preloaded_contents);
  ASSERT_TRUE(content::WaitForLoadStop(preloaded_contents));

  constexpr char kWaitForHiddenJs[] = R"(
    new Promise(resolve => {
      if (document.visibilityState === 'hidden') {
        resolve(document.visibilityState);
        return;
      }
      document.addEventListener(
          'visibilitychange', () => resolve(document.visibilityState),
          {once: true});
    });
  )";
  constexpr char kWaitForVisibleAndRefreshForeignTabsJs[] = R"(
    (async () => {
      if (document.visibilityState !== 'visible') {
        await new Promise(resolve => {
          document.addEventListener('visibilitychange', () => {
            if (document.visibilityState === 'visible') {
              resolve();
            }
          }, {once: true});
        });
      }
      const app = document.querySelector('organizer-panel-app');
      const sections = Array.from(
          app.$.list.shadowRoot.querySelectorAll('organizer-list-section'));
      const foreignSection =
          sections.find(s => s.delegate?.getId() === 'cross-device-tabs');
      if (!foreignSection) {
        return false;
      }
      await foreignSection.delegate.getItems();
      return true;
    })();
  )";

  // Wait until preload completes its initial load and hides the WebContents.
  // Preloading must not trigger ShouldStartShowingCrossDeviceSigninPromo.
  ASSERT_EQ("hidden", content::EvalJs(preloaded_contents, kWaitForHiddenJs));
  histogram_tester.ExpectTotalCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.ForeignTabs", 0);

  auto* const controller = OrganizerPanelController::From(browser());
  ASSERT_TRUE(controller);
  controller->SetOrganizerVisible(true, /*immediate=*/true);
  ASSERT_EQ(true, content::EvalJs(preloaded_contents,
                                  kWaitForVisibleAndRefreshForeignTabsJs));

  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.ForeignTabs",
      CrossDeviceSigninPromoShouldShowResult::kCanShow, 1);

  // Minimizing/hiding the WebContents directly while the panel stays open and
  // then making it visible again triggers visibilitychange -> GetForeignTabs(),
  // which must not re-record the open check.
  preloaded_contents->WasHidden();
  ASSERT_EQ("hidden", content::EvalJs(preloaded_contents, kWaitForHiddenJs));
  preloaded_contents->WasShown();
  ASSERT_EQ(true, content::EvalJs(preloaded_contents,
                                  kWaitForVisibleAndRefreshForeignTabsJs));

  histogram_tester.ExpectTotalCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.ForeignTabs", 1);
}
#endif

class ForeignTabsPageHandlerCloseMetricsBrowserTest
    : public ForeignTabsPageHandlerBrowserTest {
 public:
  ForeignTabsPageHandlerCloseMetricsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {organizer_panel::kOrganizerPanel,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
         switches::kCrossDeviceSigninFromDesktopFollowup
#endif
        },
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ForeignTabsPageHandlerCloseMetricsBrowserTest,
                       OpenTabRecordsOrganizerCloseAction) {
  base::HistogramTester histogram_tester;
  auto* const controller = OrganizerPanelController::From(browser());
  ASSERT_TRUE(controller);

  auto* open_tabs = session_sync_service()->GetOpenTabsUIDelegate();
  open_tabs->AddForeignSession("session_laptop", base::Time::Now());
  sessions::SessionTab* session_tab = open_tabs->AddTabToForeignSession(
      "session_laptop", GURL("https://www.example.com/"));

  mojo::Remote<organizer_panel::mojom::ForeignTabsPageHandler> remote;
  ForeignTabsPageHandler handler(
      remote.BindNewPipeAndPassReceiver(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  // Invalid OpenTab must leave close action at kNoAction.
  controller->SetOrganizerVisible(true, /*immediate=*/true);
  remote->OpenTab("session_laptop", -1);
  remote.FlushForTesting();
  if (controller->IsOrganizerPanelVisible()) {
    controller->SetOrganizerVisible(false, /*immediate=*/true);
  }
  histogram_tester.ExpectBucketCount("Tabs.Organizer.CloseAction",
                                     OrganizerPanelCloseAction::kNoAction, 1);

  // Valid OpenTab records kOpenCrossDeviceTab.
  controller->SetOrganizerVisible(true, /*immediate=*/true);
  remote->OpenTab("session_laptop", session_tab->tab_id.id());
  remote.FlushForTesting();
  if (controller->IsOrganizerPanelVisible()) {
    controller->SetOrganizerVisible(false, /*immediate=*/true);
  }
  histogram_tester.ExpectBucketCount(
      "Tabs.Organizer.CloseAction",
      OrganizerPanelCloseAction::kOpenCrossDeviceTab, 1);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
IN_PROC_BROWSER_TEST_F(ForeignTabsPageHandlerCloseMetricsBrowserTest,
                       ShowCrossDeviceSignInPromoRecordsOrganizerCloseAction) {
  base::HistogramTester histogram_tester;
  auto* const controller = OrganizerPanelController::From(browser());
  ASSERT_TRUE(controller);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  signin::MakePrimaryAccountAvailable(identity_manager, "user@example.com",
                                      signin::ConsentLevel::kSignin);

  mojo::Remote<organizer_panel::mojom::ForeignTabsPageHandler> remote;
  ForeignTabsPageHandler handler(
      remote.BindNewPipeAndPassReceiver(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  controller->SetOrganizerVisible(true, /*immediate=*/true);
  remote->ShowCrossDeviceSignInPromo();
  remote.FlushForTesting();
  if (controller->IsOrganizerPanelVisible()) {
    controller->SetOrganizerVisible(false, /*immediate=*/true);
  }

  histogram_tester.ExpectUniqueSample(
      "Tabs.Organizer.CloseAction",
      OrganizerPanelCloseAction::kOpenCrossDevicePromo, 1);
}
#endif

}  // namespace
