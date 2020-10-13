// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/pwa_install_view.h"

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/install_bounce_metric.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/network_switches.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/view_observer.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_app_instance.h"
#endif  // defined(OS_CHROMEOS)

namespace {

class PwaInstallIconChangeWaiter : public views::ViewObserver {
 public:
  static void VerifyIconVisibility(views::View* iconView, bool visible) {
    if (visible != iconView->GetVisible())
      PwaInstallIconChangeWaiter(iconView).run_loop_.Run();

    EXPECT_EQ(visible, iconView->GetVisible());
  }

 private:
  explicit PwaInstallIconChangeWaiter(views::View* view) {
    observed_.Add(view);
  }
  ~PwaInstallIconChangeWaiter() override = default;

  // ViewObserver
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override {
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;

  ScopedObserver<views::View, views::ViewObserver> observed_{this};

  DISALLOW_COPY_AND_ASSIGN(PwaInstallIconChangeWaiter);
};

}  // namespace

class PwaInstallViewBrowserTest
    : public extensions::ExtensionBrowserTest,
      public ::testing::WithParamInterface<web_app::ProviderType> {
 public:
  PwaInstallViewBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    if (GetParam() == web_app::ProviderType::kWebApps) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kDesktopPWAsWithoutExtensions);
    } else if (GetParam() == web_app::ProviderType::kBookmarkApps) {
      scoped_feature_list_.InitAndDisableFeature(
          features::kDesktopPWAsWithoutExtensions);
    }
  }
  ~PwaInstallViewBrowserTest() override = default;

  void SetUp() override {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&PwaInstallViewBrowserTest::RequestInterceptor,
                            base::Unretained(this)));
    ASSERT_TRUE(https_server_.Start());

    banners::TestAppBannerManagerDesktop::SetUp();
    extensions::ExtensionBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);

#if defined(OS_CHROMEOS)
    arc::SetArcAvailableCommandLineForTesting(command_line);
#endif  // defined(OS_CHROMEOS)

    command_line->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        GetInstallableAppURL().GetOrigin().spec());
  }

  void SetUpInProcessBrowserTestFixture() override {
#if defined(OS_CHROMEOS)
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
#endif  // defined(OS_CHROMEOS)
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();

    web_app::WebAppProvider::Get(browser()->profile())
        ->os_integration_manager()
        .SuppressOsHooksForTesting();

    pwa_install_view_ =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider()
            ->GetPageActionIconView(PageActionIconType::kPwaInstall);
    EXPECT_FALSE(pwa_install_view_->GetVisible());

    web_contents_ = GetCurrentTab();
    app_banner_manager_ =
        banners::TestAppBannerManagerDesktop::FromWebContents(web_contents_);
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
    content::WebContents* web_contents;
    banners::TestAppBannerManagerDesktop* app_banner_manager;
    bool installable;
  };

  OpenTabResult OpenTab(const GURL& url) {
    chrome::NewTab(browser());
    content::WebContents* web_contents = GetCurrentTab();
    auto* app_banner_manager =
        banners::TestAppBannerManagerDesktop::FromWebContents(web_contents);
    DCHECK(!app_banner_manager->WaitForInstallableCheck());

    ui_test_utils::NavigateToURL(browser(), url);
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
    browser()->OpenURL(content::OpenURLParams(
        url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
        ui::PAGE_TRANSITION_TYPED, false /* is_renderer_initiated */));
    app_banner_manager_->WaitForInstallableCheckTearDown();
  }

  web_app::AppId ExecutePwaInstallIcon() {
    chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);

    web_app::AppId app_id;
    base::RunLoop run_loop;
    web_app::SetInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&app_id, &run_loop](const web_app::AppId& installed_app_id,
                             web_app::InstallResultCode code) {
          app_id = installed_app_id;
          run_loop.Quit();
        }));

    pwa_install_view_->ExecuteForTesting();

    run_loop.Run();

    chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);

    return app_id;
  }

  // Tests that we measure when a user uninstalls a PWA within a "bounce" period
  // of time after installation.
  void TestInstallBounce(base::TimeDelta install_duration, int expected_count) {
    base::HistogramTester histogram_tester;
    base::Time test_time = base::Time::Now();

    StartNavigateToUrl(GetInstallableAppURL());
    ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());

    web_app::SetInstallBounceMetricTimeForTesting(test_time);

    const web_app::AppId app_id = ExecutePwaInstallIcon();

    web_app::SetInstallBounceMetricTimeForTesting(test_time + install_duration);

    base::RunLoop run_loop;
    web_app::WebAppProvider::Get(browser()->profile())
        ->install_finalizer()
        .UninstallExternalAppByUser(
            app_id, base::BindLambdaForTesting([&](bool uninstalled) {
              EXPECT_TRUE(uninstalled);
              run_loop.Quit();
            }));
    run_loop.Run();

    web_app::SetInstallBounceMetricTimeForTesting(base::nullopt);

    std::vector<base::Bucket> expected_buckets;
    if (expected_count > 0) {
      expected_buckets.push_back(
          {static_cast<int>(WebappInstallSource::OMNIBOX_INSTALL_ICON),
           expected_count});
    }
    EXPECT_EQ(histogram_tester.GetAllSamples("Webapp.Install.InstallBounce"),
              expected_buckets);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  std::string intercept_request_path_;
  std::string intercept_request_response_;

  PageActionIconView* pwa_install_view_ = nullptr;
  content::WebContents* web_contents_ = nullptr;
  banners::TestAppBannerManagerDesktop* app_banner_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PwaInstallViewBrowserTest);
};

// Tests that the plus icon is not shown when an existing app is installed and
// set to open in a window.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest,
                       PwaSetToOpenInWindowIsNotInstallable) {
  bool installable = OpenTab(GetInstallableAppURL()).installable;
  ASSERT_TRUE(installable);
  ExecutePwaInstallIcon();

  // Use a new tab because installed app may have opened in new window.
  OpenTabResult result = OpenTab(GetInstallableAppURL());

  EXPECT_EQ(
      result.app_banner_manager->GetInstallableWebAppCheckResultForTesting(),
      banners::AppBannerManager::InstallableWebAppCheckResult::
          kNoAlreadyInstalled);
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
  ExecutePwaInstallIcon();

  // Use a new tab because installed app may have opened in new window.
  OpenTabResult result = OpenTab(GetNestedInstallableAppURL());

  // The nested PWA should now not be installable.
  EXPECT_EQ(
      result.app_banner_manager->GetInstallableWebAppCheckResultForTesting(),
      banners::AppBannerManager::InstallableWebAppCheckResult::
          kNoAlreadyInstalled);
  EXPECT_FALSE(pwa_install_view_->GetVisible());
}

// Tests that the install icon is shown when an existing app is installed and
// set to open in a tab.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest,
                       PwaSetToOpenInTabIsInstallable) {
  bool installable = OpenTab(GetInstallableAppURL()).installable;
  ASSERT_TRUE(installable);
  web_app::AppId app_id = ExecutePwaInstallIcon();

  // Change launch container to open in tab.
  web_app::WebAppProvider::Get(browser()->profile())
      ->registry_controller()
      .SetAppUserDisplayMode(app_id, web_app::DisplayMode::kBrowser,
                             /*is_user_action=*/false);

  // Use a new tab because installed app may have opened in new window.
  OpenTabResult result = OpenTab(GetInstallableAppURL());

  EXPECT_EQ(
      result.app_banner_manager->GetInstallableWebAppCheckResultForTesting(),
      banners::AppBannerManager::InstallableWebAppCheckResult::kPromotable);
  EXPECT_TRUE(pwa_install_view_->GetVisible());
}

// Test that the accept metrics is reported correctly.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest, PwaInstalledMetricRecorded) {
  bool installable = OpenTab(GetInstallableAppURL()).installable;
  ASSERT_TRUE(installable);

  base::HistogramTester histograms;
  ExecutePwaInstallIcon();
  histograms.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kAcceptButtonClicked, 1);
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

// Tests that the install icon updates its visibility when tab crashes.
IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest,
                       IconVisibilityAfterTabCrashed) {
  StartNavigateToUrl(GetInstallableAppURL());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());

  web_contents_->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
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
  ExecutePwaInstallIcon();
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

    banners::AppBannerManager::SetTimeDeltaForTesting(day);

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
  SkColor omnibox_background = browser_view->GetLocationBarView()->GetColor(
      OmniboxPart::LOCATION_BAR_BACKGROUND);
  SkColor label_color = pwa_install_view_->GetLabelColorForTesting();
  EXPECT_EQ(SkColorGetA(label_color), SK_AlphaOPAQUE);
  EXPECT_GT(color_utils::GetContrastRatio(omnibox_background, label_color),
            color_utils::kMinimumReadableContrastRatio);
}

IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest, BouncedInstallMeasured) {
  TestInstallBounce(base::TimeDelta::FromMinutes(50), 1);
}

IN_PROC_BROWSER_TEST_P(PwaInstallViewBrowserTest, BouncedInstallIgnored) {
  TestInstallBounce(base::TimeDelta::FromMinutes(70), 0);
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
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());

  EXPECT_FALSE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(base::EqualsASCII(
      banners::AppBannerManager::GetInstallableWebAppName(web_contents_),
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
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());

  EXPECT_FALSE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(base::EqualsASCII(
      banners::AppBannerManager::GetInstallableWebAppName(web_contents_),
      "Manifest listing related chrome app"));
}

#if defined(OS_CHROMEOS)
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
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());

  EXPECT_FALSE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(base::EqualsASCII(
      banners::AppBannerManager::GetInstallableWebAppName(web_contents_),
      "Manifest listing related android app"));
}
#endif  // defined(OS_CHROMEOS)

INSTANTIATE_TEST_SUITE_P(All,
                         PwaInstallViewBrowserTest,
                         ::testing::Values(web_app::ProviderType::kBookmarkApps,
                                           web_app::ProviderType::kWebApps),
                         web_app::ProviderTypeParamToString);
