// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/app_browser_controller.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/devtools/protocol/browser_handler.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/theme_change_waiter.h"
#include "extensions/browser/extension_registry.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/display/types/display_constants.h"

namespace {
SkColor GetFrameColor(Browser* browser) {
  CustomThemeSupplier* theme = browser->app_controller()->GetThemeSupplier();
  SkColor result;
  EXPECT_TRUE(theme->GetColor(ThemeProperties::COLOR_FRAME_ACTIVE, &result));
  return result;
}
}  // namespace

namespace web_app {

class LoadFinishedWaiter : public TabStripModelObserver,
                           public content::WebContentsObserver {
 public:
  explicit LoadFinishedWaiter(Browser* browser) : browser_(browser) {
    browser_->tab_strip_model()->AddObserver(this);
  }

  ~LoadFinishedWaiter() override {
    browser_->tab_strip_model()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  SkColor GetColorAtNavigation() const { return color_at_navigation_; }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (selection.active_tab_changed())
      content::WebContentsObserver::Observe(selection.new_contents);
  }

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override {
    color_at_navigation_ = GetFrameColor(browser_);
  }
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    run_loop_.Quit();
  }

 private:
  raw_ptr<Browser> browser_ = nullptr;
  SkColor color_at_navigation_;
  base::RunLoop run_loop_;
};

class AppBrowserControllerBrowserTest : public InProcessBrowserTest {
 public:
  AppBrowserControllerBrowserTest()
      : test_system_web_app_installation_(
            ash::TestSystemWebAppInstallation::SetUpTabbedMultiWindowApp()) {}
  AppBrowserControllerBrowserTest(const AppBrowserControllerBrowserTest&) =
      delete;
  AppBrowserControllerBrowserTest& operator=(
      const AppBrowserControllerBrowserTest&) = delete;

 protected:
  Profile* profile() {
    if (!profile_)
      profile_ = browser()->profile();
    return profile_;
  }

  void InstallMockSystemWebApp() {
    test_system_web_app_installation_->WaitForAppInstall();
  }

  Browser* LaunchMockApp() {
    app_browser_ = web_app::LaunchWebAppBrowser(
        profile(), test_system_web_app_installation_->GetAppId());
    tabbed_app_url_ = test_system_web_app_installation_->GetAppUrl();
    EXPECT_TRUE(content::NavigateToURL(
        app_browser_->tab_strip_model()->GetActiveWebContents(),
        tabbed_app_url_));
    return app_browser_;
  }

  void LaunchMockPopup() {
    auto params = ash::CreateSystemWebAppLaunchParams(
        profile(), test_system_web_app_installation_->GetType(),
        display::kInvalidDisplayId);
    EXPECT_TRUE(params.has_value());
    params->disposition = WindowOpenDisposition::NEW_POPUP;

    app_browser_ = ash::LaunchSystemWebAppImpl(
        profile(), test_system_web_app_installation_->GetType(),
        test_system_web_app_installation_->GetAppUrl(), *params);
  }

  Browser* LaunchMockSWA() {
    auto params = ash::CreateSystemWebAppLaunchParams(
        profile(), test_system_web_app_installation_->GetType(),
        display::kInvalidDisplayId);
    EXPECT_TRUE(params.has_value());
    params->disposition = WindowOpenDisposition::NEW_WINDOW;

    return ash::LaunchSystemWebAppImpl(
        profile(), test_system_web_app_installation_->GetType(),
        test_system_web_app_installation_->GetAppUrl(), *params);
  }

  Browser* InstallAndLaunchMockApp() {
    InstallMockSystemWebApp();
    return LaunchMockApp();
  }

  void InstallAndLaunchMockPopup() {
    InstallMockSystemWebApp();
    LaunchMockPopup();
  }

  Browser* InstallAndLaunchMockSWA() {
    InstallMockSystemWebApp();
    return LaunchMockSWA();
  }

  GURL GetActiveTabURL() {
    return app_browser_->tab_strip_model()
        ->GetActiveWebContents()
        ->GetVisibleURL();
  }

  raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_ = nullptr;
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> app_browser_ = nullptr;
  GURL tabbed_app_url_;

 private:
  std::unique_ptr<ash::TestSystemWebAppInstallation>
      test_system_web_app_installation_;
};

IN_PROC_BROWSER_TEST_F(AppBrowserControllerBrowserTest, TabsTest) {
  InstallAndLaunchMockApp();

  EXPECT_TRUE(app_browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP));

  // No favicons shown for web apps.
  EXPECT_FALSE(
      app_browser_->tab_strip_model()->delegate()->ShouldDisplayFavicon(
          app_browser_->tab_strip_model()->GetActiveWebContents()));

  // Tabbed PWAs only open URLs within the scope of the app. The manifest is
  // another URL besides |tabbed_app_url_| in scope.
  GURL manifest("chrome://test-system-app/manifest.json");
  // Check URL of tab1.
  EXPECT_EQ(GetActiveTabURL(), tabbed_app_url_);
  // Create tab2 with another URL from app, check URL, number of tabs.
  chrome::AddTabAt(app_browser_, manifest, -1, true);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 2);
  EXPECT_EQ(GetActiveTabURL(), manifest);
  // Create tab3 with default URL, check URL, number of tabs.
  chrome::NewTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 3);
  EXPECT_EQ(GetActiveTabURL(), tabbed_app_url_);
  // Switch to tab1, check URL.
  chrome::SelectNextTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 3);
  EXPECT_EQ(GetActiveTabURL(), tabbed_app_url_);
  // Switch to tab2, check URL.
  chrome::SelectNextTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 3);
  EXPECT_EQ(GetActiveTabURL(), manifest);
  // Switch to tab3, check URL.
  chrome::SelectNextTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 3);
  EXPECT_EQ(GetActiveTabURL(), tabbed_app_url_);
  // Close tab3, check number of tabs.
  chrome::CloseTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 2);
  EXPECT_EQ(GetActiveTabURL(), manifest);
  // Close tab2, check number of tabs.
  chrome::CloseTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 1);
  EXPECT_EQ(GetActiveTabURL(), tabbed_app_url_);

  // Enter tab fullscreen, check tab strip not supported.
  static_cast<content::WebContentsDelegate*>(app_browser_)
      ->EnterFullscreenModeForTab(app_browser_->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                  {});
  EXPECT_FALSE(app_browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP));
}

IN_PROC_BROWSER_TEST_F(AppBrowserControllerBrowserTest, NonAppUrl) {
  InstallAndLaunchMockApp();

  // Check we have 2 browsers: |browser()| and |app_browser_|.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
  EXPECT_NE(browser(), app_browser_);
  EXPECT_TRUE(browser()->is_type_normal());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      "about:blank");
  EXPECT_TRUE(app_browser_->is_type_app());
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 1);
  EXPECT_EQ(GetActiveTabURL(), tabbed_app_url_);

  // Create tab2 with URL not from app, it will open in the NORMAL browser.
  chrome::AddTabAt(app_browser_, GURL(chrome::kChromeUINewTabURL), -1, true);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
  EXPECT_NE(browser(), app_browser_);
  EXPECT_TRUE(browser()->is_type_normal());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      "chrome://newtab/");
  EXPECT_TRUE(app_browser_->is_type_app());
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 1);
  EXPECT_EQ(GetActiveTabURL(), tabbed_app_url_);
}

IN_PROC_BROWSER_TEST_F(AppBrowserControllerBrowserTest, TabLoadNoThemeChange) {
  InstallAndLaunchMockApp();
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 1);
  // Frame gets manifest theme immediately.
  EXPECT_EQ(GetFrameColor(app_browser_), SK_ColorGREEN);

  // Dynamically change color.
  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  content::ThemeChangeWaiter theme_waiter(web_contents);
  EXPECT_TRUE(content::ExecJs(web_contents, R"(
      const el = document.createElement("meta");
      el.setAttribute("name", "theme-color");
      el.setAttribute("content", "yellow");
      document.documentElement.appendChild(el);
  )",
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                              /*world_id=*/1));
  theme_waiter.Wait();
  EXPECT_EQ(GetFrameColor(app_browser_), SK_ColorYELLOW);

  // Second tab keeps dynamic theme until loaded.
  LoadFinishedWaiter load_waiter(app_browser_);
  chrome::NewTab(app_browser_);
  load_waiter.Wait();
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 2);
  EXPECT_EQ(load_waiter.GetColorAtNavigation(), SK_ColorYELLOW);
  EXPECT_EQ(GetFrameColor(app_browser_), SK_ColorGREEN);

  // Switching tabs updates themes immediately.
  chrome::SelectNextTab(app_browser_);
  EXPECT_EQ(GetFrameColor(app_browser_), SK_ColorYELLOW);
  chrome::SelectNextTab(app_browser_);
  EXPECT_EQ(GetFrameColor(app_browser_), SK_ColorGREEN);
}

// App Popups are only used on Chrome OS. See https://crbug.com/1060917.
#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(AppBrowserControllerBrowserTest,
                       WhiteThemeForSystemAppPopup) {
  InstallAndLaunchMockPopup();
  EXPECT_FALSE(app_browser_->app_controller()->GetThemeColor().has_value());
}

IN_PROC_BROWSER_TEST_F(AppBrowserControllerBrowserTest, Shutdown) {
  // Cache profile before browser() closes.
  profile();
  InstallMockSystemWebApp();

  BrowserHandler handler(nullptr, std::string());
  handler.Close();
  ui_test_utils::WaitForBrowserToClose();

  LaunchMockPopup();
  EXPECT_EQ(app_browser_, nullptr);
}

IN_PROC_BROWSER_TEST_F(AppBrowserControllerBrowserTest,
                       ReuseBrowserForSystemAppPopup) {
  InstallAndLaunchMockPopup();
  // We should have the original browser for this BrowserTest, plus new popup.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
  InstallAndLaunchMockPopup();
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
}

IN_PROC_BROWSER_TEST_F(AppBrowserControllerBrowserTest,
                       OpenMultipleBrowsersForMultiWindowSWA) {
  Browser* first_browser = InstallAndLaunchMockSWA();
  // We should have the original browser for this BrowserTest, plus a new one,
  // offset by a tasteful amount.
  EXPECT_NE(nullptr, first_browser);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
  Browser* second_browser = LaunchMockSWA();
  EXPECT_NE(nullptr, second_browser);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 3u);

  auto bounds1 = first_browser->window()->GetRestoredBounds();
  auto bounds2 = second_browser->window()->GetRestoredBounds();
  // We've already hit the bottom bounds, so the y axis didn't move.
  EXPECT_EQ(bounds1.x() + WindowSizer::kWindowTilePixels, bounds2.x());

  // Open a ton of windows until they start stacking.
  bool hit_the_bottom_right = false;
  gfx::Rect previous_bounds = bounds2;
  for (int i = 0; i < 10; i++) {
    Browser* next_browser = LaunchMockSWA();
    if (previous_bounds == next_browser->window()->GetRestoredBounds()) {
      hit_the_bottom_right = true;
      break;
    }
    previous_bounds = next_browser->window()->GetRestoredBounds();
  }

  EXPECT_TRUE(hit_the_bottom_right);
}

IN_PROC_BROWSER_TEST_F(AppBrowserControllerBrowserTest,
                       NoExtensionsContainerExists) {
  InstallAndLaunchMockPopup();
  EXPECT_EQ(app_browser_->window()->GetExtensionsContainer(), nullptr);
}
#endif

class AppBrowserControllerChromeUntrustedBrowserTest
    : public InProcessBrowserTest {
 public:
  AppBrowserControllerChromeUntrustedBrowserTest()
      : test_system_web_app_installation_(
            ash::TestSystemWebAppInstallation::SetUpChromeUntrustedApp()) {}

 protected:
  Browser* InstallAndLaunchMockApp() {
    test_system_web_app_installation_->WaitForAppInstall();
    Browser* app_browser = web_app::LaunchWebAppBrowser(
        browser()->profile(), test_system_web_app_installation_->GetAppId());
    CHECK(content::NavigateToURL(
        app_browser->tab_strip_model()->GetActiveWebContents(),
        test_system_web_app_installation_->GetAppUrl()));
    return app_browser;
  }

 private:
  std::unique_ptr<ash::TestSystemWebAppInstallation>
      test_system_web_app_installation_;
};

IN_PROC_BROWSER_TEST_F(AppBrowserControllerChromeUntrustedBrowserTest,
                       DoesNotShowToolbar) {
  Browser* app_browser = InstallAndLaunchMockApp();
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

}  // namespace web_app
