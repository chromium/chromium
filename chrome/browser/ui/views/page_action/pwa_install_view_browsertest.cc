// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/extend.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/clamped_math.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_pref_guardrails.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/banners/installable_web_app_check_result.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace webapps {
enum class InstallResultCode;
}

namespace {

class PwaInstallIconChangeWaiter : public views::ViewObserver {
 public:
  PwaInstallIconChangeWaiter(const PwaInstallIconChangeWaiter&) = delete;
  PwaInstallIconChangeWaiter& operator=(const PwaInstallIconChangeWaiter&) =
      delete;

  static void VerifyIconVisibility(views::View* iconView, bool visible);

 private:
  explicit PwaInstallIconChangeWaiter(views::View* view) {
    observation_.Observe(view);
  }
  ~PwaInstallIconChangeWaiter() override = default;

  // ViewObserver
  void OnViewVisibilityChanged(views::View* observation_view,
                               views::View* starting_view) override {
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;

  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

// static
void PwaInstallIconChangeWaiter::VerifyIconVisibility(views::View* iconView,
                                                      bool visible) {
  if (visible != iconView->GetVisible())
    PwaInstallIconChangeWaiter(iconView).run_loop_.Run();

  EXPECT_EQ(visible, iconView->GetVisible());
}

}  // namespace

// Tests various cases that effect the visibility of the install icon in the
// omnibox.
class PwaInstallViewBrowserTest : public extensions::ExtensionBrowserTest,
                                  public testing::WithParamInterface<bool> {
 public:
  PwaInstallViewBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // kIPHDemoMode will bypass IPH framework's triggering validation so that
    // we can test PWA specific triggering logic.
    std::vector<base::test::FeatureRefAndParams> enabled_features{
        {feature_engagement::kIPHDemoMode,
         {{feature_engagement::kIPHDemoModeFeatureChoiceParam,
           feature_engagement::kIPHDesktopPwaInstallFeature.name}}},
        {feature_engagement::kIPHDesktopPwaInstallFeature, {}}};
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsUniversalInstallEnabled()) {
      enabled_features.push_back({features::kWebAppUniversalInstall, {}});
    } else {
      disabled_features.push_back(features::kWebAppUniversalInstall);
    }
#if BUILDFLAG(IS_CHROMEOS_ASH)
    base::Extend(disabled_features, ash::standalone_browser::GetFeatureRefs());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    features_.InitWithFeaturesAndParameters(enabled_features,
                                            disabled_features);
  }

  PwaInstallViewBrowserTest(const PwaInstallViewBrowserTest&) = delete;
  PwaInstallViewBrowserTest& operator=(const PwaInstallViewBrowserTest&) =
      delete;

  ~PwaInstallViewBrowserTest() override = default;

  void SetUp() override {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&PwaInstallViewBrowserTest::RequestInterceptor,
                            base::Unretained(this)));
    ASSERT_TRUE(https_server_.Start());

    webapps::TestAppBannerManagerDesktop::SetUp();
    extensions::ExtensionBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);

#if BUILDFLAG(IS_CHROMEOS_ASH)
    arc::SetArcAvailableCommandLineForTesting(command_line);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    command_line->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        GetInstallableAppURL().DeprecatedGetOriginAsURL().spec());
  }

  void SetUpInProcessBrowserTestFixture() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();

    pwa_install_view_ =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider()
            ->GetPageActionIconView(PageActionIconType::kPwaInstall);
    EXPECT_FALSE(pwa_install_view_->GetVisible());

    web_contents_ = GetCurrentTab();
    app_banner_manager_ =
        webapps::TestAppBannerManagerDesktop::FromWebContents(web_contents_);
    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(browser()->profile()));
  }

  std::unique_ptr<net::test_server::HttpResponse> RequestInterceptor(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != intercept_request_path_)
      return nullptr;
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_FOUND);
    http_response->set_content(intercept_request_response_);
    return std::move(http_response);
  }

  content::WebContents* GetCurrentTab() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  struct OpenTabResult {
    raw_ptr<content::WebContents> web_contents;
    raw_ptr<webapps::TestAppBannerManagerDesktop, DanglingUntriaged>
        app_banner_manager;
    bool installable;
  };

  OpenTabResult OpenTab(const GURL& url) {
    chrome::NewTab(browser());
    content::WebContents* web_contents = GetCurrentTab();
    auto* app_banner_manager =
        webapps::TestAppBannerManagerDesktop::FromWebContents(web_contents);
    DCHECK(!app_banner_manager->WaitForInstallableCheck());

    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    bool installable = app_banner_manager->WaitForInstallableCheck();

    return OpenTabResult{web_contents, app_banner_manager, installable};
  }

  GURL GetInstallableAppURL() {
    return https_server_.GetURL("/banners/manifest_test_page.html");
  }

  GURL GetNestedInstallableAppURL() {
    return https_server_.GetURL("/banners/scope_a/page_1.html");
  }

  GURL GetNonInstallableAppURL() {
    return https_server_.GetURL("app.com", "/simple.html");
  }

  // Starts a navigation to |url| but does not wait for it to finish.
  void StartNavigateToUrl(const GURL& url) {
    browser()->OpenURL(
        content::OpenURLParams(
            url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
            ui::PAGE_TRANSITION_TYPED, false /* is_renderer_initiated */),
        /*navigation_handle_callback=*/{});
    app_banner_manager_->WaitForInstallableCheckTearDown();
  }

  webapps::AppId StartPwaInstallFromPageActionViewAndGetInstalledApp() {
    webapps::AppId app_id;
    base::RunLoop run_loop;
    web_app::SetInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&app_id, &run_loop](const webapps::AppId& installed_app_id,
                             webapps::InstallResultCode code) {
          app_id = installed_app_id;
          run_loop.Quit();
        }));

    {
      views::Widget* install_dialog_widget =
          ClickPWAInstallIconAndWaitForBubbleShown();
      EXPECT_NE(install_dialog_widget, nullptr);
      views::test::WidgetDestroyedWaiter destroyed_waiter(
          install_dialog_widget);
      views::test::AcceptDialog(install_dialog_widget);
      destroyed_waiter.Wait();
    }

    run_loop.Run();

    return app_id;
  }

  void UninstallWebApp(const webapps::AppId& app_id) {
    base::RunLoop run_loop;
    web_app::WebAppProvider::GetForTest(browser()->profile())
        ->scheduler()
        .RemoveUserUninstallableManagements(
            app_id, webapps::WebappUninstallSource::kAppMenu,
            base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
              EXPECT_TRUE(UninstallSucceeded(code));
              run_loop.Quit();
            }));
    run_loop.Run();
    base::RunLoop().RunUntilIdle();
  }

  // Tests that we measure when a user uninstalls a PWA within a "bounce" period
  // of time after installation.
  void TestInstallBounce(base::TimeDelta install_duration, int expected_count) {
    base::HistogramTester histogram_tester;
    base::Time test_time = base::Time::Now();

    StartNavigateToUrl(GetInstallableAppURL());
    ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());

    web_app::SetInstallBounceMetricTimeForTesting(test_time);

    const webapps::AppId app_id =
        StartPwaInstallFromPageActionViewAndGetInstalledApp();

    web_app::SetInstallBounceMetricTimeForTesting(test_time + install_duration);

    UninstallWebApp(app_id);

    web_app::SetInstallBounceMetricTimeForTesting(std::nullopt);

    std::vector<base::Bucket> expected_buckets;
    if (expected_count > 0) {
      expected_buckets.push_back(
          {static_cast<int>(webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON),
           expected_count});
    }
    EXPECT_EQ(histogram_tester.GetAllSamples("Webapp.Install.InstallBounce"),
              expected_buckets);
  }

  views::Widget* ClickPWAInstallIconAndWaitForBubbleShown() {
    std::string bubble_view_name = IsUniversalInstallEnabled()
                                       ? "WebAppSimpleInstallDialog"
                                       : "PWAConfirmationBubbleView";
    views::NamedWidgetShownWaiter pwa_confirmation_bubble_id_waiter(
        views::test::AnyWidgetTestPasskey(), bubble_view_name);

    pwa_install_view_->ExecuteForTesting();
    return pwa_confirmation_bubble_id_waiter.WaitIfNeededAndGet();
  }


 protected:
  bool IsUniversalInstallEnabled() { return GetParam(); }
  net::EmbeddedTestServer https_server_;
  std::string intercept_request_path_;
  std::string intercept_request_response_;

  raw_ptr<PageActionIconView, AcrossTasksDanglingUntriaged> pwa_install_view_ =
      nullptr;
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_ =
      nullptr;
  raw_ptr<webapps::TestAppBannerManagerDesktop, AcrossTasksDanglingUntriaged>
      app_banner_manager_ = nullptr;

 private:
  web_app::OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
  base::test::ScopedFeatureList features_;
};

// Tests that the plus icon is not shown when an existing app is installed and
// set to open in a window.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest,
                       PwaSetToOpenInWindowIsNotInstallable) {
  bool installable = OpenTab(GetInstallableAppURL()).installable;
  ASSERT_TRUE(installable);
  StartPwaInstallFromPageActionViewAndGetInstalledApp();

  // Use a new tab because installed app may have opened in new window.
  OpenTabResult result = OpenTab(GetInstallableAppURL());

  EXPECT_EQ(result.app_banner_manager->GetInstallableWebAppCheckResult(),
            webapps::InstallableWebAppCheckResult::kNo_AlreadyInstalled);
  EXPECT_FALSE(pwa_install_view_->GetVisible());
}

// Tests that the plus icon is not shown when an outer app is installed and we
// navigate to a nested app.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest,
                       NestedPwaIsNotInstallableWhenOuterPwaIsInstalled) {
  // When nothing is installed, the nested PWA should be installable.
  StartNavigateToUrl(GetNestedInstallableAppURL());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());

  // Install the outer PWA.
  ASSERT_TRUE(OpenTab(GetInstallableAppURL()).installable);
  StartPwaInstallFromPageActionViewAndGetInstalledApp();

  // Use a new tab because installed app may have opened in new window.
  OpenTabResult result = OpenTab(GetNestedInstallableAppURL());

  // The nested PWA should now not be installable.
  EXPECT_EQ(result.app_banner_manager->GetInstallableWebAppCheckResult(),
            webapps::InstallableWebAppCheckResult::kNo_AlreadyInstalled);
  EXPECT_FALSE(pwa_install_view_->GetVisible());
}

// Tests that the install icon is shown when an existing app is installed and
// set to open in a tab.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest,
                       PwaSetToOpenInTabIsInstallable) {
  bool installable = OpenTab(GetInstallableAppURL()).installable;
  ASSERT_TRUE(installable);
  webapps::AppId app_id = StartPwaInstallFromPageActionViewAndGetInstalledApp();

  // Change launch container to open in tab.
  web_app::WebAppProvider::GetForTest(browser()->profile())
      ->sync_bridge_unsafe()
      .SetAppUserDisplayModeForTesting(
          app_id, web_app::mojom::UserDisplayMode::kBrowser);

  // Use a new tab because installed app may have opened in new window.
  OpenTabResult result = OpenTab(GetInstallableAppURL());

  EXPECT_EQ(result.app_banner_manager->GetInstallableWebAppCheckResult(),
            webapps::InstallableWebAppCheckResult::kYes_Promotable);
  EXPECT_TRUE(pwa_install_view_->GetVisible());
}

// Tests that the plus icon updates its visibility when switching between
// installable/non-installable tabs.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest,
                       IconVisibilityAfterTabSwitching) {
  content::WebContents* installable_web_contents;
  {
    OpenTabResult result = OpenTab(GetInstallableAppURL());
    installable_web_contents = result.web_contents;
    ASSERT_TRUE(result.installable);
  }

  content::WebContents* non_installable_web_contents;
  {
    OpenTabResult result = OpenTab(GetNonInstallableAppURL());
    non_installable_web_contents = result.web_contents;
    ASSERT_FALSE(result.installable);
  }

  chrome::SelectPreviousTab(browser());
  ASSERT_EQ(installable_web_contents, GetCurrentTab());
  EXPECT_TRUE(pwa_install_view_->GetVisible());

  chrome::SelectNextTab(browser());
  ASSERT_EQ(non_installable_web_contents, GetCurrentTab());
  EXPECT_FALSE(pwa_install_view_->GetVisible());
}

// Tests that the plus icon updates its visibility when PWAConfirmationBubbleView is showing in
// installable tab and switching to non-installable tab.
IN_PROC_BROWSER_TEST_P(
    PwaInstallViewBrowserTest,
    IconVisibilityAfterTabSwitchingWhenPWAConfirmationBubbleViewShowing) {
  content::WebContents* installable_web_contents;
  {
    OpenTabResult result = OpenTab(GetInstallableAppURL());
    installable_web_contents = result.web_contents;
    ASSERT_TRUE(result.installable);
  }
  content::WebContents* non_installable_web_contents;
  {
    OpenTabResult result = OpenTab(GetNonInstallableAppURL());
    non_installable_web_contents = result.web_contents;
    ASSERT_FALSE(result.installable);
  }
  chrome::SelectPreviousTab(browser());
  ASSERT_EQ(installable_web_contents, GetCurrentTab());
  EXPECT_TRUE(pwa_install_view_->GetVisible());

  views::Widget* pwa_install_widget =
      ClickPWAInstallIconAndWaitForBubbleShown();
  EXPECT_NE(pwa_install_widget, nullptr);
  chrome::SelectNextTab(browser());
  views::test::WidgetDestroyedWaiter destroy_waiter(pwa_install_widget);
  pwa_install_widget->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  destroy_waiter.Wait();

  ASSERT_EQ(non_installable_web_contents, GetCurrentTab());
  EXPECT_FALSE(pwa_install_view_->GetVisible());
}

// Tests that the install icon updates its visibility when tab crashes.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest,
                       IconVisibilityAfterTabCrashed) {
  StartNavigateToUrl(GetInstallableAppURL());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());

  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    content::RenderFrameDeletedObserver crash_observer(
        web_contents_->GetPrimaryMainFrame());
    web_contents_->GetPrimaryMainFrame()->GetProcess()->Shutdown(1);
    crash_observer.WaitUntilDeleted();
  }
  ASSERT_TRUE(web_contents_->IsCrashed());
  PwaInstallIconChangeWaiter::VerifyIconVisibility(pwa_install_view_, false);
}

// Tests that the plus icon updates its visibility once the installability check
// completes.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest,
                       IconVisibilityAfterInstallabilityCheck) {
  StartNavigateToUrl(GetInstallableAppURL());
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());

  StartNavigateToUrl(GetNonInstallableAppURL());
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_FALSE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_FALSE(pwa_install_view_->GetVisible());
}

// Tests that the plus icon updates its visibility after installation.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest,
                       IconVisibilityAfterInstallation) {
  StartNavigateToUrl(GetInstallableAppURL());
  content::WebContents* first_tab = GetCurrentTab();
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());

  OpenTabResult result = OpenTab(GetInstallableAppURL());
  EXPECT_TRUE(result.installable);
  EXPECT_NE(first_tab, GetCurrentTab());
  StartPwaInstallFromPageActionViewAndGetInstalledApp();
  EXPECT_EQ(first_tab, GetCurrentTab());
  EXPECT_FALSE(pwa_install_view_->GetVisible());
}

// Tests that the plus icon animates its label when the installability check
// passes but doesn't animate more than once for the same installability check.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest, LabelAnimation) {
  StartNavigateToUrl(GetInstallableAppURL());
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  chrome::NewTab(browser());
  EXPECT_FALSE(pwa_install_view_->GetVisible());

  chrome::SelectPreviousTab(browser());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_FALSE(pwa_install_view_->is_animating_label());
}

// Tests that the plus icon becomes invisible when the user is typing in the
// omnibox.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest, InputInOmnibox) {
  StartNavigateToUrl(GetInstallableAppURL());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  LocationBar* location_bar = browser_view->GetLocationBarView();
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  omnibox_view->model()->SetInputInProgress(true);
  EXPECT_FALSE(pwa_install_view_->GetVisible());
}

// Tests that the icon persists while loading the same scope and omits running
// the label animation again.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest, NavigateToSameScope) {
  StartNavigateToUrl(https_server_.GetURL("/banners/scope_a/page_1.html"));
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  StartNavigateToUrl(https_server_.GetURL("/banners/scope_a/page_2.html"));
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_FALSE(pwa_install_view_->is_animating_label());
}

// Tests that the icon persists while loading the same scope but goes away when
// the installability check fails.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest,
                       NavigateToSameScopeNonInstallable) {
  StartNavigateToUrl(https_server_.GetURL("/banners/scope_a/page_1.html"));
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  StartNavigateToUrl(
      https_server_.GetURL("/banners/scope_a/bad_manifest.html"));
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  ASSERT_FALSE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  EXPECT_FALSE(pwa_install_view_->is_animating_label());
}

// Tests that the icon updates its state after uninstallation.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest,
                       IconStateAfterUnInstallation) {
  GURL app_url = GetInstallableAppURL();
  bool installable = OpenTab(app_url).installable;
  ASSERT_TRUE(installable);
  const webapps::AppId app_id =
      StartPwaInstallFromPageActionViewAndGetInstalledApp();

  // Use a new tab because installed app may have opened in new window.
  OpenTabResult result = OpenTab(app_url);

  // Validate that state is set to already installed.
  EXPECT_EQ(result.app_banner_manager->GetInstallableWebAppCheckResult(),
            webapps::InstallableWebAppCheckResult::kNo_AlreadyInstalled);
  EXPECT_FALSE(pwa_install_view_->GetVisible());

  // Uninstall app and wait for completion.
  UninstallWebApp(app_id);

  // Validate that state got changed to installable.
  ASSERT_TRUE(result.app_banner_manager->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
}

// Tests that the icon and animation resets while loading a different scope.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest, NavigateToDifferentScope) {
  StartNavigateToUrl(https_server_.GetURL("/banners/scope_a/page_1.html"));
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  StartNavigateToUrl(https_server_.GetURL("/banners/scope_b/scope_b.html"));
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());
}

// Tests that the icon and animation resets while loading a different empty
// scope.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest,
                       NavigateToDifferentEmptyScope) {
  StartNavigateToUrl(https_server_.GetURL("/banners/scope_a/page_1.html"));
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  StartNavigateToUrl(https_server_.GetURL("/banners/manifest_test_page.html"));
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());
}

// Tests that the animation is suppressed for navigations within the same scope
// for an exponentially increasing period of time.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest, AnimationSuppression) {
  std::vector<bool> animation_shown_for_day = {
      true,  true,  false, true,  false, false, false, true,
      false, false, false, false, false, false, false, true,
  };
  for (size_t day = 0; day < animation_shown_for_day.size(); ++day) {
    SCOPED_TRACE(testing::Message() << "day: " << day);

    webapps::AppBannerManager::SetTimeDeltaForTesting(day);

    StartNavigateToUrl(GetInstallableAppURL());
    ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
    EXPECT_EQ(pwa_install_view_->is_animating_label(),
              animation_shown_for_day[day]);
  }
}

// Tests that the icon label is visible against the omnibox background after the
// native widget becomes active.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest, TextContrast) {
  StartNavigateToUrl(GetInstallableAppURL());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  pwa_install_view_->GetWidget()->OnNativeWidgetActivationChanged(true);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  SkColor omnibox_background = browser_view->GetColorProvider()->GetColor(
      kColorToolbarBackgroundSubtleEmphasis);
  SkColor label_color = pwa_install_view_->GetLabelColorForTesting();
  EXPECT_EQ(SkColorGetA(label_color), SK_AlphaOPAQUE);
  EXPECT_GT(color_utils::GetContrastRatio(omnibox_background, label_color),
            color_utils::kMinimumReadableContrastRatio);
}

IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest, BouncedInstallMeasured) {
  TestInstallBounce(base::Minutes(50), 1);
}

IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest, BouncedInstallIgnored) {
  TestInstallBounce(base::Minutes(70), 0);
}

// Omnibox install promotion should show if there are no viable related apps
// even if prefer_related_applications is true.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest, PreferRelatedAppUnknown) {
  StartNavigateToUrl(
      https_server_.GetURL("/banners/manifest_test_page.html?manifest="
                           "manifest_prefer_related_apps_unknown.json"));
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());

  EXPECT_TRUE(pwa_install_view_->GetVisible());
}

// Omnibox install promotion should not show if prefer_related_applications is
// false but a related Chrome app is installed.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest, PreferRelatedChromeApp) {
  StartNavigateToUrl(
      https_server_.GetURL("/banners/manifest_test_page.html?manifest="
                           "manifest_prefer_related_chrome_app.json"));
  ASSERT_FALSE(app_banner_manager_->WaitForInstallableCheck());

  EXPECT_FALSE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(base::EqualsASCII(
      webapps::AppBannerManager::GetInstallableWebAppName(web_contents_),
      "Manifest prefer related chrome app"));
}

// Omnibox install promotion should not show if prefer_related_applications is
// true and a Chrome app listed as related.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest,
                       ListedRelatedChromeAppInstalled) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("app"));

  intercept_request_path_ = "/banners/manifest_listing_related_chrome_app.json";
  intercept_request_response_ = R"(
    {
      "name": "Manifest listing related chrome app",
      "icons": [
        {
          "src": "/banners/image-512px.png",
          "sizes": "512x512",
          "type": "image/png"
        }
      ],
      "scope": ".",
      "start_url": ".",
      "display": "standalone",
      "prefer_related_applications": false,
      "related_applications": [{
        "platform": "chrome_web_store",
        "id": ")";
  intercept_request_response_ += extension->id();
  intercept_request_response_ += R"(",
        "comment": "This is the id of test/data/extensions/app"
      }]
    }
  )";

  StartNavigateToUrl(https_server_.GetURL(
      "/banners/manifest_test_page.html?manifest=" + intercept_request_path_));
  ASSERT_FALSE(app_banner_manager_->WaitForInstallableCheck());

  EXPECT_FALSE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(base::EqualsASCII(
      webapps::AppBannerManager::GetInstallableWebAppName(web_contents_),
      "Manifest listing related chrome app"));
}

// TODO(crbug.com/40796769): Flaky.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest,
                       DISABLED_PwaIntallIphSiteEngagement) {
  GURL app_url = GetInstallableAppURL();
  bool installable = OpenTab(app_url).installable;
  ASSERT_TRUE(installable);

  // IPH is not shown when the site is not highly engaged.
  EXPECT_FALSE(browser()->window()->IsFeaturePromoActive(
      feature_engagement::kIPHDesktopPwaInstallFeature));

  // Manually set engagement score to be above IPH triggering threshold.
  site_engagement::SiteEngagementService::Get(profile())->AddPointsForTesting(
      app_url, web_app::kIphFieldTrialParamDefaultSiteEngagementThreshold + 1);
  OpenTab(app_url);
  EXPECT_TRUE(browser()->window()->IsFeaturePromoActive(
      feature_engagement::kIPHDesktopPwaInstallFeature));
}

IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest, PwaIntallIphIgnored) {
  GURL app_url = GetInstallableAppURL();
  site_engagement::SiteEngagementService::Get(profile())->AddPointsForTesting(
      app_url, web_app::kIphFieldTrialParamDefaultSiteEngagementThreshold + 1);
  // Manually set IPH ignored here, because the IPH demo mode only let IPH be
  // shown once in an user session.
  web_app::WebAppPrefGuardrails::GetForDesktopInstallIph(profile()->GetPrefs())
      .RecordIgnore(web_app::GenerateAppId(/*manifest_id_path=*/std::nullopt,
                                           GetInstallableAppURL()),
                    base::Time::Now());
  bool installable = OpenTab(app_url).installable;
  ASSERT_TRUE(installable);

  // IPH is not shown when the IPH is ignored recently.
  EXPECT_FALSE(browser()->window()->IsFeaturePromoActive(
      feature_engagement::kIPHDesktopPwaInstallFeature));
}

IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest, IconViewAccessibleName) {
  const std::u16string& web_app_name =
      webapps::AppBannerManager::GetInstallableWebAppName(web_contents_);
  EXPECT_EQ(pwa_install_view_->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringFUTF16(IDS_OMNIBOX_PWA_INSTALL_ICON_TOOLTIP,
                                       web_app_name));
  EXPECT_EQ(pwa_install_view_->GetTextForTooltipAndAccessibleName(),
            l10n_util::GetStringFUTF16(IDS_OMNIBOX_PWA_INSTALL_ICON_TOOLTIP,
                                       web_app_name));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Omnibox install promotion should not show if prefer_related_applications is
// true and an ARC app listed as related.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest,
                       ListedRelatedAndroidAppInstalled) {
  arc::SetArcPlayStoreEnabledForProfile(browser()->profile(), true);
  ArcAppListPrefs* arc_app_list_prefs =
      ArcAppListPrefs::Get(browser()->profile());
  auto app_instance =
      std::make_unique<arc::FakeAppInstance>(arc_app_list_prefs);
  arc_app_list_prefs->app_connection_holder()->SetInstance(app_instance.get());
  WaitForInstanceReady(arc_app_list_prefs->app_connection_holder());

  // Install test Android app.
  arc::mojom::ArcPackageInfo package;
  package.package_name = "com.example.app";
  app_instance->InstallPackage(package.Clone());

  StartNavigateToUrl(
      https_server_.GetURL("/banners/manifest_test_page.html?manifest="
                           "manifest_listing_related_android_app.json"));
  ASSERT_FALSE(app_banner_manager_->WaitForInstallableCheck());

  EXPECT_FALSE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(base::EqualsASCII(
      webapps::AppBannerManager::GetInstallableWebAppName(web_contents_),
      "Manifest listing related android app"));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

INSTANTIATE_TEST_SUITE_P(All,
                         PwaInstallViewBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WebAppSimpleInstallDialog"
                                             : "PWAConfirmationBubbleView";
                         });
