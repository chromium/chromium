// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/navigation_capturing_settings.h"
#include "chrome/browser/web_applications/scheduler/update_validated_origin_associations_result.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/url_formatter/url_formatter.h"
#include "components/webapps/services/web_app_origin_association/test/test_web_app_origin_association_fetcher.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#endif

namespace web_app {
namespace {

constexpr char kRevalidationHistogram[] =
    "WebApp.ValidatedOriginAssociations.Updated";
using ClientMode = blink::mojom::ManifestLaunchHandler_ClientMode;
using CreationSource = BrowserWindowCreateParams::CreationSource;

class ControllableAssociationFetcher
    : public webapps::TestWebAppOriginAssociationFetcher {
 public:
  using TestWebAppOriginAssociationFetcher::FetchWebAppOriginAssociationFile;

  void FetchWebAppOriginAssociationFile(
      const url::Origin& origin,
      network::mojom::IPAddressSpace initiator_address_space,
      webapps::FetchFileCallback callback) override {
    ++request_count_;
    if (hold_next_request_) {
      hold_next_request_ = false;
      pending_request_.SetValue(origin, initiator_address_space,
                                std::move(callback));
      return;
    }
    TestWebAppOriginAssociationFetcher::FetchWebAppOriginAssociationFile(
        origin, initiator_address_space, std::move(callback));
  }

  void HoldNextRequest() { hold_next_request_ = true; }
  bool WaitForRequest() { return pending_request_.Wait(); }
  void CompleteRequest() {
    auto [origin, initiator_address_space, callback] = pending_request_.Take();
    TestWebAppOriginAssociationFetcher::FetchWebAppOriginAssociationFile(
        origin, initiator_address_space, std::move(callback));
  }
  int request_count() const { return request_count_; }

 private:
  bool hold_next_request_ = false;
  int request_count_ = 0;
  base::test::TestFuture<url::Origin,
                         network::mojom::IPAddressSpace,
                         webapps::FetchFileCallback>
      pending_request_;
};

class WebAppOriginAssociationLifecycleTest : public WebAppBrowserTestBase {
 public:
  WebAppOriginAssociationLifecycleTest()
      : WebAppBrowserTestBase({features::kPwaNavigationCapturing}, {}) {}

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    clock_.SetNow(base::Time::Now());
    provider().SetClockForTesting(&clock_);
    auto fetcher = std::make_unique<ControllableAssociationFetcher>();
    fetcher_ = fetcher.get();
    provider().origin_association_manager().SetFetcherForTest(
        std::move(fetcher));
  }

  void TearDownOnMainThread() override {
    fetcher_ = nullptr;
    provider().SetClockForTesting(base::DefaultClock::GetInstance());
    WebAppBrowserTestBase::TearDownOnMainThread();
  }

 protected:
  GURL AppUrl() {
    return embedded_https_test_server().GetURL("app.com", "/title1.html");
  }
  GURL ExtendedUrl() {
    return embedded_https_test_server().GetURL("extended.com", "/title1.html");
  }

  webapps::AppId InstallApp(
      std::optional<ClientMode> client_mode = std::nullopt,
      bool tabbed = false) {
    fetcher_->SetData(
        {{url::Origin::Create(ExtendedUrl()),
          base::StringPrintf(R"({"%s": {"scope": "/"}})", AppUrl().spec())}});
    auto info = WebAppInstallInfo::CreateForTesting(
        AppUrl(), blink::mojom::DisplayMode::kStandalone,
        mojom::UserDisplayMode::kStandalone, client_mode);
    info->title = u"Lifecycle App";
    if (tabbed) {
      info->display_override = {
          DisplayOverride::Create(blink::mojom::DisplayMode::kTabbed)};
      info->tab_strip = blink::Manifest::TabStrip();
      info->tab_strip->home_tab = blink::Manifest::HomeTabParams();
    }
    info->scope_extensions = {ScopeExtensionInfo::CreateForOrigin(
        url::Origin::Create(ExtendedUrl()))};
    auto app_id = test::InstallWebApp(profile(), std::move(info));
    EXPECT_FALSE(provider()
                     .registrar_unsafe()
                     .GetAppById(app_id)
                     ->validated_scope_extensions()
                     .empty());
    return app_id;
  }

  void RevokeAndExpire() {
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
    fetcher_->SetData({});
    clock_.Advance(base::Days(1));
  }

  CustomTabBarView* TabBar(BrowserWindowInterface* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->toolbar()
        ->custom_tab_bar();
  }

  void ExpectRevocation(BrowserWindowInterface* app_browser,
                        const webapps::AppId& app_id) {
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
    ASSERT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id));
    EXPECT_TRUE(provider()
                    .registrar_unsafe()
                    .GetAppById(app_id)
                    ->validated_scope_extensions()
                    .empty());
    auto* bar = TabBar(app_browser);
    ASSERT_TRUE(bar);
    EXPECT_TRUE(bar->GetVisible());
    EXPECT_TRUE(bar->IsShowingOriginForTesting());
    EXPECT_EQ(AppBrowserController::FormatUrlOrigin(
                  app_browser->GetTabStripModel()
                      ->GetActiveWebContents()
                      ->GetLastCommittedURL(),
                  url_formatter::kFormatUrlOmitDefaults),
              bar->location_for_testing());
  }

  base::SimpleTestClock clock_;
  raw_ptr<ControllableAssociationFetcher> fetcher_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationLifecycleTest,
                       ReparentRevealsRevokedOrigin) {
  const auto app_id = InstallApp();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ExtendedUrl()));
  RevokeAndExpire();
  base::HistogramTester histograms;
  fetcher_->HoldNextRequest();

  auto* app_browser = ReparentWebContentsIntoAppBrowser(
      browser()->GetTabStripModel()->GetActiveWebContents(), app_id);
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(fetcher_->WaitForRequest());
  ASSERT_TRUE(
      base::test::RunUntil([&] { return !TabBar(app_browser)->GetVisible(); }));

  fetcher_->CompleteRequest();
  ExpectRevocation(app_browser, app_id);
  histograms.ExpectBucketCount(
      kRevalidationHistogram,
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationLifecycleTest,
                       LongLivedWindowRevalidatesOnNavigation) {
  const auto app_id = InstallApp();
  auto* app_browser = LaunchWebAppBrowserAndWait(app_id);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, ExtendedUrl()));
  // Navigation completion does not wait for the toolbar's hide animation.
  ASSERT_TRUE(
      base::test::RunUntil([&] { return !TabBar(app_browser)->GetVisible(); }));
  RevokeAndExpire();
  base::HistogramTester histograms;

  const GURL next_page =
      embedded_https_test_server().GetURL("extended.com", "/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, next_page));
  ExpectRevocation(app_browser, app_id);
  histograms.ExpectBucketCount(
      kRevalidationHistogram,
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
}

class WebAppOriginAssociationWindowAppTest
    : public WebAppOriginAssociationLifecycleTest,
      public testing::WithParamInterface<bool> {};

IN_PROC_BROWSER_TEST_P(WebAppOriginAssociationWindowAppTest,
                       NavigationRevalidatesWindowApp) {
  const bool navigate_to_other_app = GetParam();
  const auto app_id = InstallApp();
  const GURL other_app_url =
      embedded_https_test_server().GetURL("other-app.com", "/title1.html");
  const GURL other_extended_url =
      embedded_https_test_server().GetURL("other-extended.com", "/title1.html");
  fetcher_->SetData({{url::Origin::Create(other_extended_url),
                      base::StringPrintf(R"({"%s": {"scope": "/"}})",
                                         other_app_url.spec())}});
  auto info = WebAppInstallInfo::CreateForTesting(
      other_app_url, blink::mojom::DisplayMode::kStandalone);
  info->scope_extensions = {ScopeExtensionInfo::CreateForOrigin(
      url::Origin::Create(other_extended_url))};
  const auto other_app_id = test::InstallWebApp(profile(), std::move(info));
  ASSERT_FALSE(provider()
                   .registrar_unsafe()
                   .GetAppById(other_app_id)
                   ->validated_scope_extensions()
                   .empty());

  auto* app_browser = LaunchWebAppBrowserAndWait(app_id);
  auto* contents = app_browser->GetTabStripModel()->GetActiveWebContents();
  RevokeAndExpire();
  const auto last_check = provider()
                              .registrar_unsafe()
                              .GetAppById(app_id)
                              ->origin_association_last_validation_check_time();
  const auto other_last_check =
      provider()
          .registrar_unsafe()
          .GetAppById(other_app_id)
          ->origin_association_last_validation_check_time();
  ASSERT_TRUE(last_check.has_value());
  ASSERT_TRUE(other_last_check.has_value());
  ASSERT_LE(*last_check + base::Days(1), clock_.Now());
  ASSERT_LE(*other_last_check + base::Days(1), clock_.Now());
  const int before = fetcher_->request_count();
  base::HistogramTester histograms;

  const GURL target = navigate_to_other_app
                          ? other_app_url
                          : embedded_https_test_server().GetURL("unrelated.com",
                                                                "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, target));
  ASSERT_EQ(contents, app_browser->GetTabStripModel()->GetActiveWebContents());
  ASSERT_EQ(target, contents->GetLastCommittedURL());
  auto* tab_helper = WebAppTabHelper::FromWebContents(contents);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(app_id, tab_helper->window_app_id());
  EXPECT_EQ(
      navigate_to_other_app ? std::make_optional(other_app_id) : std::nullopt,
      tab_helper->app_id());

  ExpectRevocation(app_browser, app_id);
  EXPECT_GT(provider()
                .registrar_unsafe()
                .GetAppById(app_id)
                ->origin_association_last_validation_check_time(),
            last_check);
  EXPECT_EQ(other_last_check,
            provider()
                .registrar_unsafe()
                .GetAppById(other_app_id)
                ->origin_association_last_validation_check_time());
  EXPECT_FALSE(provider()
                   .registrar_unsafe()
                   .GetAppById(other_app_id)
                   ->validated_scope_extensions()
                   .empty());
  EXPECT_EQ(before + 1, fetcher_->request_count());
  histograms.ExpectBucketCount(
      kRevalidationHistogram,
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppOriginAssociationWindowAppTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "OtherAppScope"
                                             : "OutsideAllAppScopes";
                         });

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationLifecycleTest,
                       BrowserTabLaunchStillRevalidates) {
  const auto app_id = InstallApp();
  RevokeAndExpire();
  base::HistogramTester histograms;
  auto* browser = LaunchBrowserForWebAppInTab(app_id);
  ASSERT_TRUE(browser);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser->GetTabStripModel()->GetActiveWebContents()));
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(provider()
                  .registrar_unsafe()
                  .GetAppById(app_id)
                  ->validated_scope_extensions()
                  .empty());
  histograms.ExpectBucketCount(
      kRevalidationHistogram,
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationLifecycleTest,
                       RecentValidationDoesNotFetch) {
  const auto app_id = InstallApp();
  base::HistogramTester histograms;
  const int request_count = fetcher_->request_count();
  auto* app_browser = LaunchWebAppBrowserAndWait(app_id);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, ExtendedUrl()));
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  const int throttled = histograms.GetBucketCount(
      kRevalidationHistogram,
      UpdateValidatedOriginAssociationsResult::kThrottled);
  EXPECT_GT(throttled, 0);
  histograms.ExpectUniqueSample(
      kRevalidationHistogram,
      UpdateValidatedOriginAssociationsResult::kThrottled, throttled);
  EXPECT_EQ(request_count, fetcher_->request_count());
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationLifecycleTest,
                       MultipleWindowsSerializeRevalidation) {
  const auto app_id = InstallApp(ClientMode::kNavigateNew);
  auto* first = LaunchWebAppBrowserAndWait(app_id);
  auto* second = LaunchWebAppBrowserAndWait(app_id);
  ASSERT_NE(first, second);
  RevokeAndExpire();
  base::HistogramTester histograms;
  const int before = fetcher_->request_count();
  fetcher_->HoldNextRequest();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(first, ExtendedUrl()));
  ASSERT_TRUE(fetcher_->WaitForRequest());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(second, ExtendedUrl()));
  EXPECT_EQ(before + 1, fetcher_->request_count());

  fetcher_->CompleteRequest();
  ExpectRevocation(first, app_id);
  ExpectRevocation(second, app_id);
  EXPECT_EQ(before + 1, fetcher_->request_count());
  histograms.ExpectBucketCount(
      kRevalidationHistogram,
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
  EXPECT_GT(histograms.GetBucketCount(
                kRevalidationHistogram,
                UpdateValidatedOriginAssociationsResult::kThrottled),
            0);
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationLifecycleTest,
                       LeavingAppWindowDoesNotRevalidate) {
  const auto app_id = InstallApp();
  auto* app_browser = LaunchWebAppBrowserAndWait(app_id);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, ExtendedUrl()));
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  RevokeAndExpire();
  base::HistogramTester histograms;
  const int before = fetcher_->request_count();

  auto contents =
      app_browser->GetTabStripModel()->DetachWebContentsAtForInsertion(0);
  browser()->GetTabStripModel()->InsertWebContentsAt(0, std::move(contents),
                                                     AddTabTypes::ADD_ACTIVE);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), AppUrl()));
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  histograms.ExpectTotalCount(kRevalidationHistogram, 0);
  EXPECT_EQ(before, fetcher_->request_count());
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationLifecycleTest,
                       CapturedLinkRevealsRevokedOrigin) {
  const auto app_id = InstallApp();
#if BUILDFLAG(IS_CHROMEOS)
  apps::AppReadinessWaiter(profile(), app_id).Await();
#endif
  ASSERT_OK(apps::test::EnableLinkCapturingByUser(profile(), app_id));
  ASSERT_EQ(app_id, NavigationCapturingSettings::Create(*profile())
                        ->GetCapturingWebAppForUrl(ExtendedUrl()));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("other.com", "/title1.html")));
  RevokeAndExpire();
  base::HistogramTester histograms;

  ui_test_utils::BrowserCreatedObserver window_created;
  content::TestNavigationObserver navigation(ExtendedUrl());
  navigation.StartWatchingNewWebContents();
  ASSERT_TRUE(content::ExecJs(
      browser()->GetTabStripModel()->GetActiveWebContents(),
      content::JsReplace("const a = document.createElement('a');"
                         "a.href = $1; a.target = '_blank'; a.rel = 'noopener';"
                         "document.body.appendChild(a); a.click();",
                         ExtendedUrl())));
  navigation.Wait();
  auto* app_browser = window_created.Wait();
  ExpectRevocation(app_browser, app_id);
  histograms.ExpectBucketCount(
      kRevalidationHistogram,
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationLifecycleTest,
                       SessionRestoreRevealsRevokedOrigin) {
  const auto app_id = InstallApp();
  auto* app_browser = LaunchWebAppBrowserAndWait(app_id);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, ExtendedUrl()));
  ASSERT_TRUE(
      base::test::RunUntil([&] { return !TabBar(app_browser)->GetVisible(); }));

  // Preserve the app session on disk before closing the window, just as on
  // browser shutdown. Keep the regular browser open to retain the profile.
  AppSessionServiceFactory::ShutdownForProfile(profile());
  CloseBrowserSynchronously(app_browser);
  RevokeAndExpire();
  AppSessionServiceFactory::GetForProfileForSessionRestore(profile());
  base::HistogramTester histograms;

  ui_test_utils::BrowserCreatedObserver window_created;
  SessionRestore::RestoreSession(
      profile(), nullptr,
      SessionRestore::SYNCHRONOUS | SessionRestore::RESTORE_APPS, {});
  auto* restored = window_created.Wait();
  ASSERT_TRUE(restored);
  ASSERT_EQ(1, restored->GetTabStripModel()->count());
  auto* contents = restored->GetTabStripModel()->GetActiveWebContents();
  contents->GetController().LoadIfNecessary();
  ASSERT_TRUE(content::WaitForLoadStop(contents));
  EXPECT_EQ(ExtendedUrl(), contents->GetLastCommittedURL());
  ExpectRevocation(restored, app_id);
  histograms.ExpectBucketCount(
      kRevalidationHistogram,
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationLifecycleTest,
                       MovingLoadedTabIntoAppRevalidates) {
  const auto app_id = InstallApp();
  ASSERT_TRUE(AddTabAtIndex(1, ExtendedUrl(), ui::PAGE_TRANSITION_TYPED));
  auto params = BrowserWindowCreateParams::CreateForApp(
      GenerateApplicationNameFromAppId(app_id), true, gfx::Rect(), profile(),
      false);
  auto* app_browser = CreateBrowserWindow(std::move(params));
  ASSERT_TRUE(app_browser);
  RevokeAndExpire();
  base::HistogramTester histograms;

  // Move an already-loaded document without a launch command or navigation.
  auto contents =
      browser()->GetTabStripModel()->DetachWebContentsAtForInsertion(1);
  auto* original_contents = contents.get();
  app_browser->GetTabStripModel()->InsertWebContentsAt(0, std::move(contents),
                                                       AddTabTypes::ADD_ACTIVE);
  EXPECT_EQ(original_contents,
            app_browser->GetTabStripModel()->GetActiveWebContents());
  ExpectRevocation(app_browser, app_id);
  histograms.ExpectBucketCount(
      kRevalidationHistogram,
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationLifecycleTest,
                       CancelledLaunchNavigationStillRevalidates) {
  const auto app_id = InstallApp(ClientMode::kNavigateExisting);
  auto* app_browser = LaunchWebAppBrowserAndWait(app_id);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, ExtendedUrl()));
  auto* contents = app_browser->GetTabStripModel()->GetActiveWebContents();
  RevokeAndExpire();
  base::HistogramTester histograms;

  const GURL launch_url = AppUrl();
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [launch_url](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url != launch_url) {
          return false;
        }
        params->client->OnComplete(
            network::URLLoaderCompletionStatus(net::ERR_ABORTED));
        return true;
      }));
  content::TestNavigationObserver navigation(contents);
  EXPECT_EQ(app_browser, LaunchWebAppBrowser(app_id));
  navigation.Wait();
  EXPECT_FALSE(navigation.last_navigation_succeeded());
  ASSERT_TRUE(content::WaitForLoadStop(contents));
  EXPECT_EQ(ExtendedUrl(), contents->GetLastCommittedURL());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(provider()
                  .registrar_unsafe()
                  .GetAppById(app_id)
                  ->validated_scope_extensions()
                  .empty());
  EXPECT_TRUE(TabBar(app_browser)->GetVisible());
  histograms.ExpectBucketCount(
      kRevalidationHistogram,
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
}

class WebAppOriginAssociationTabbedTest
    : public WebAppOriginAssociationLifecycleTest {
 public:
  WebAppOriginAssociationTabbedTest() {
    features_.InitWithFeatures(
        {blink::features::kDesktopPWAsTabStrip,
         blink::features::kDesktopPWAsTabStripCustomizations,
         features::kDesktopPWAsTabStripSettings},
        {});
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationTabbedTest,
                       PinnedHomeTabRelaunchRevalidates) {
  const auto app_id = InstallApp(std::nullopt, /*tabbed=*/true);
  auto* app_browser = LaunchWebAppBrowserAndWait(app_id);
  auto* tabs = app_browser->GetTabStripModel();
  ASSERT_TRUE(tabs->IsTabPinned(0));
  auto* home = tabs->GetWebContentsAt(0);
  const auto home_document = home->GetPrimaryMainFrame()->GetGlobalId();
  ASSERT_TRUE(AddTabAtIndexToBrowser(app_browser, 1, ExtendedUrl(),
                                     ui::PAGE_TRANSITION_TYPED));
  RevokeAndExpire();
  base::HistogramTester histograms;

  EXPECT_EQ(app_browser, LaunchWebAppBrowser(app_id));
  EXPECT_EQ(home, tabs->GetActiveWebContents());
  EXPECT_EQ(home_document, home->GetPrimaryMainFrame()->GetGlobalId());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  tabs->ActivateTabAt(1);
  ExpectRevocation(app_browser, app_id);
  histograms.ExpectBucketCount(
      kRevalidationHistogram,
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
}

class WebAppOriginAssociationDirectReparentTest
    : public WebAppOriginAssociationTabbedTest {
 public:
  WebAppOriginAssociationDirectReparentTest() {
    // Exercise the direct launch path instead of the Intent Picker shortcut.
    features_.InitAndDisableFeature(features::kPwaNavigationCapturing);
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationDirectReparentTest,
                       PinnedHomeTabReparentRevalidates) {
  const auto app_id = InstallApp(ClientMode::kFocusExisting, /*tabbed=*/true);
  auto* app_browser = LaunchWebAppBrowserAndWait(app_id);
  auto* tabs = app_browser->GetTabStripModel();
  ASSERT_TRUE(tabs->IsTabPinned(0));
  auto* home = tabs->GetWebContentsAt(0);
  ASSERT_EQ(AppUrl(), home->GetLastCommittedURL());
  const auto home_document = home->GetPrimaryMainFrame()->GetGlobalId();
  ASSERT_TRUE(AddTabAtIndexToBrowser(app_browser, 1, ExtendedUrl(),
                                     ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(AddTabAtIndex(1, AppUrl(), ui::PAGE_TRANSITION_TYPED));
  auto* source = browser()->GetTabStripModel()->GetActiveWebContents();
  content::WebContentsDestroyedWatcher source_closed(source);
  RevokeAndExpire();
  base::HistogramTester histograms;
  const int request_count = fetcher_->request_count();

  EXPECT_EQ(app_browser, ReparentWebContentsIntoAppBrowser(source, app_id));
  source_closed.Wait();
  EXPECT_EQ(2, tabs->count());
  EXPECT_EQ(home, tabs->GetActiveWebContents());
  EXPECT_EQ(home_document, home->GetPrimaryMainFrame()->GetGlobalId());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_EQ(request_count + 1, fetcher_->request_count());
  tabs->ActivateTabAt(1);
  ExpectRevocation(app_browser, app_id);
  histograms.ExpectUniqueSample(
      kRevalidationHistogram,
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
}

class WebAppOriginAssociationExistingClientTest
    : public WebAppOriginAssociationLifecycleTest,
      public testing::WithParamInterface<ClientMode> {};

IN_PROC_BROWSER_TEST_P(WebAppOriginAssociationExistingClientTest,
                       ReusedWindowRevealsRevokedOrigin) {
  const auto app_id = InstallApp(GetParam());
  auto* app_browser = LaunchWebAppBrowserAndWait(app_id);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, ExtendedUrl()));
  auto* original_contents =
      app_browser->GetTabStripModel()->GetActiveWebContents();
  const auto original_document =
      original_contents->GetPrimaryMainFrame()->GetGlobalId();
  const GURL target =
      embedded_https_test_server().GetURL("extended.com", "/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), target));
  RevokeAndExpire();
  base::HistogramTester histograms;

  EXPECT_EQ(nullptr,
            ReparentWebContentsIntoAppBrowser(
                browser()->GetTabStripModel()->GetActiveWebContents(), app_id));
  if (GetParam() == ClientMode::kNavigateExisting) {
    ASSERT_TRUE(content::WaitForLoadStop(original_contents));
    EXPECT_EQ(target, original_contents->GetLastCommittedURL());
  } else {
    EXPECT_EQ(ExtendedUrl(), original_contents->GetLastCommittedURL());
    EXPECT_EQ(original_document,
              original_contents->GetPrimaryMainFrame()->GetGlobalId());
  }
  ExpectRevocation(app_browser, app_id);
  histograms.ExpectBucketCount(
      kRevalidationHistogram,
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppOriginAssociationExistingClientTest,
                         testing::Values(ClientMode::kFocusExisting,
                                         ClientMode::kNavigateExisting));

class WebAppOriginAssociationWindowCreationTest
    : public WebAppOriginAssociationLifecycleTest,
      public testing::WithParamInterface<CreationSource> {};

IN_PROC_BROWSER_TEST_P(WebAppOriginAssociationWindowCreationTest,
                       InsertedTabRevealsRevokedOrigin) {
  const auto app_id = InstallApp();
  RevokeAndExpire();
  auto params = BrowserWindowCreateParams::CreateForApp(
      GenerateApplicationNameFromAppId(app_id), true, gfx::Rect(), profile(),
      false);
  params.creation_source = GetParam();
  auto* app_browser = CreateBrowserWindow(std::move(params));
  ASSERT_TRUE(app_browser);
  base::HistogramTester histograms;

  // A bare window has no tab lifecycle. Insert and load a real page, as the
  // startup and restore code does after constructing the app window.
  ASSERT_TRUE(AddTabAtIndexToBrowser(app_browser, 0, ExtendedUrl(),
                                     ui::PAGE_TRANSITION_RELOAD));
  ExpectRevocation(app_browser, app_id);
  histograms.ExpectBucketCount(
      kRevalidationHistogram,
      UpdateValidatedOriginAssociationsResult::kUnvalidatedItemsRemain, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppOriginAssociationWindowCreationTest,
    testing::Values(CreationSource::kUnknown,
                    CreationSource::kSessionRestore,
                    CreationSource::kStartupCreator,
                    CreationSource::kLastAndUrlsStartupPref,
                    CreationSource::kDeskTemplate));

}  // namespace
}  // namespace web_app
