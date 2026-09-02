// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view.h"

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_web_contents_delegate/browser_web_contents_delegate.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/form_import/form_data_importer.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/theme_change_waiter.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/mock_os_settings_provider.h"

namespace {

class TestAutofillManager : public autofill::BrowserAutofillManager {
 public:
  explicit TestAutofillManager(autofill::ContentAutofillDriver* driver)
      : BrowserAutofillManager(driver) {}

  [[nodiscard]] testing::AssertionResult WaitForFormsSeen(
      int min_num_awaited_calls) {
    return forms_seen_waiter_.Wait(min_num_awaited_calls);
  }

 private:
  autofill::TestAutofillManagerWaiter forms_seen_waiter_{
      *this,
      {autofill::AutofillManagerEvent::kFormsSeen}};
};

}  // namespace

class BrowserFrameViewBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  BrowserFrameViewBrowserTest() = default;

  BrowserFrameViewBrowserTest(const BrowserFrameViewBrowserTest&) = delete;
  BrowserFrameViewBrowserTest& operator=(const BrowserFrameViewBrowserTest&) =
      delete;

  ~BrowserFrameViewBrowserTest() override = default;

  void SetUp() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    extensions::ExtensionBrowserTest::SetUp();
  }

  // Note: A "bookmark app" is a type of hosted app. All of these tests apply
  // equally to hosted and bookmark apps, but it's easier to install a bookmark
  // app in a test.
  // TODO: Add tests for non-bookmark hosted apps, as bookmark apps will no
  // longer be hosted apps when BMO ships.
  void InstallAndLaunchBookmarkApp(std::optional<GURL> app_url = std::nullopt) {
    blink::mojom::Manifest manifest;
    manifest.manifest_url = embedded_test_server()->GetURL("/manifest");
    manifest.start_url = app_url.value_or(GetAppURL());
    manifest.id = app_url.value_or(GetAppURL());
    manifest.scope = manifest.start_url.GetWithoutFilename();
    manifest.theme_color = app_theme_color_;

    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info =
        web_app::test::GetInstallInfoForCurrentManifest(
            browser()->GetTabStripModel()->GetActiveWebContents()->GetWeakPtr(),
            manifest);

    webapps::AppId app_id =
        web_app::test::InstallWebApp(profile(), std::move(web_app_info));
    app_browser_ = web_app::LaunchWebAppBrowser(profile(), app_id);
    web_contents_ = app_browser_->GetTabStripModel()->GetActiveWebContents();
    // Ensure the main page has loaded and is ready for ExecJs DOM
    // manipulation.
    ASSERT_TRUE(content::NavigateToURL(web_contents_, manifest.start_url));

    app_browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser_);
  }

  // Frame view may get reset after theme change, so always access from the
  // browser view and don't retain the pointer.
  // TODO(crbug.com/40656280): Make it not do this and only refresh the Widget.
  BrowserFrameView* GetAppFrameView() {
    return app_browser_view_->browser_widget()->GetFrameView();
  }

 protected:
  SkColor app_theme_color_ = SK_ColorBLUE;
  raw_ptr<BrowserWindowInterface, AcrossTasksDanglingUntriaged> app_browser_ =
      nullptr;
  raw_ptr<BrowserView, AcrossTasksDanglingUntriaged> app_browser_view_ =
      nullptr;
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_ =
      nullptr;
  autofill::TestAutofillManagerInjector<TestAutofillManager>
      autofill_manager_injector_;

 private:
  GURL GetAppURL() { return embedded_test_server()->GetURL("/empty.html"); }

  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

// Tests the frame color for a normal browser window.
IN_PROC_BROWSER_TEST_F(BrowserFrameViewBrowserTest, BrowserFrameColorThemed) {
  InstallExtension(test_data_dir_.AppendASCII("theme"), 1);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  const BrowserFrameView* frame_view =
      browser_view->browser_widget()->GetFrameView();
  const ui::ColorProvider* color_provider = frame_view->GetColorProvider();
  const SkColor expected_active_color =
      color_provider->GetColor(ui::kColorFrameActive);
  const SkColor expected_inactive_color =
      color_provider->GetColor(ui::kColorFrameInactive);

  EXPECT_EQ(expected_active_color,
            frame_view->GetFrameColor(BrowserFrameActiveState::kActive));
  EXPECT_EQ(expected_inactive_color,
            frame_view->GetFrameColor(BrowserFrameActiveState::kInactive));
}

// Tests the frame color for a bookmark app when a theme is applied.
IN_PROC_BROWSER_TEST_F(BrowserFrameViewBrowserTest,
                       BookmarkAppFrameColorCustomTheme) {
  // The theme color should not affect the window, but the theme must not be the
  // default GTK theme for Linux so we install one anyway.
  InstallExtension(test_data_dir_.AppendASCII("theme"), 1);
  InstallAndLaunchBookmarkApp();
  // Note: This is checking for the bookmark app's theme color, not the user's
  // theme color.
  EXPECT_EQ(app_theme_color_,
            GetAppFrameView()->GetFrameColor(BrowserFrameActiveState::kActive));
}

// Tests the frame color for a bookmark app when a theme is applied, with the
// app itself having no theme color.
IN_PROC_BROWSER_TEST_F(BrowserFrameViewBrowserTest,
                       BookmarkAppFrameColorCustomThemeNoThemeColor) {
  InstallAndLaunchBookmarkApp();
  const SkColor color_without_theme =
      GetAppFrameView()->GetFrameColor(BrowserFrameActiveState::kActive);

  InstallExtension(test_data_dir_.AppendASCII("theme"), 1);
  // Bookmark apps are not affected by browser themes.
  EXPECT_EQ(color_without_theme,
            GetAppFrameView()->GetFrameColor(BrowserFrameActiveState::kActive));
}

// Tests that an opaque frame color is used for a web app with a transparent
// theme color.
IN_PROC_BROWSER_TEST_F(BrowserFrameViewBrowserTest,
                       OpaqueFrameColorForTransparentWebAppThemeColor) {
  // Ensure we're not using the system theme on Linux.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->GetProfile());
  theme_service->UseDefaultTheme();

  app_theme_color_ = SkColorSetA(SK_ColorBLUE, 0x88);
  InstallAndLaunchBookmarkApp();
  EXPECT_EQ(GetAppFrameView()->GetFrameColor(BrowserFrameActiveState::kActive),
            SK_ColorBLUE);
}

// Tests the frame color for a bookmark app when the system theme is applied.
IN_PROC_BROWSER_TEST_F(BrowserFrameViewBrowserTest,
                       BookmarkAppFrameColorSystemTheme) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->GetProfile());
  // Should be using the system theme by default, but this assert was not true
  // on the bots. Explicitly set.
  theme_service->UseSystemTheme();
  ASSERT_TRUE(theme_service->UsingSystemTheme());

  InstallAndLaunchBookmarkApp();
#if BUILDFLAG(IS_LINUX)
  // On Linux, the system theme is the GTK theme and should change the frame
  // color to the system color (not the app theme color); otherwise the title
  // and border would clash horribly with the GTK title bar.
  // (https://crbug.com/41410571)
  const ui::ColorProvider* color_provider =
      GetAppFrameView()->GetColorProvider();
  const SkColor frame_color = color_provider->GetColor(ui::kColorFrameActive);
  EXPECT_EQ(frame_color,
            GetAppFrameView()->GetFrameColor(BrowserFrameActiveState::kActive));
#else
  EXPECT_EQ(app_theme_color_,
            GetAppFrameView()->GetFrameColor(BrowserFrameActiveState::kActive));
#endif
}

// Verifies that the incognito window frame is always the right color.
IN_PROC_BROWSER_TEST_F(BrowserFrameViewBrowserTest, IncognitoIsCorrectColor) {
  // Set the color that's expected to be ignored.
  ui::MockOsSettingsProvider os_settings_provider;
  os_settings_provider.SetAccentColor(gfx::kGoogleBlue400);

  BrowserWindowInterface* incognito_browser =
      CreateIncognitoBrowser(browser()->GetProfile());

  BrowserView* view = BrowserView::GetBrowserViewForBrowser(incognito_browser);
  BrowserWidget* widget = view->browser_widget();
  BrowserFrameView* frame_view = widget->GetFrameView();

  color_utils::HSL frame_color_hsl;
  SkColorToHSL(frame_view->GetFrameColor(BrowserFrameActiveState::kActive),
               &frame_color_hsl);
  // Ensure that the frame color is very dark in Incognito.
  EXPECT_LT(frame_color_hsl.l, 0.2);

  incognito_browser->GetWindow()->Close();
}

// Checks that the title bar for hosted app windows is hidden when in fullscreen
// for tab mode.
IN_PROC_BROWSER_TEST_F(BrowserFrameViewBrowserTest,
                       FullscreenForTabTitlebarHeight) {
  InstallAndLaunchBookmarkApp();
  BrowserWebContentsDelegate::From(app_browser_)
      ->EnterFullscreenModeForTab(web_contents_->GetPrimaryMainFrame(), {});

  EXPECT_EQ(GetAppFrameView()->GetTopInset(false), 0);
}

// Tests that the custom tab bar is visible in fullscreen mode.
// TODO(crbug.com/40855995): Flaky on linux-wayland-rel.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_CustomTabBarIsVisibleInFullscreen \
  DISABLED_CustomTabBarIsVisibleInFullscreen
#else
#define MAYBE_CustomTabBarIsVisibleInFullscreen \
  CustomTabBarIsVisibleInFullscreen
#endif
IN_PROC_BROWSER_TEST_F(BrowserFrameViewBrowserTest,
                       MAYBE_CustomTabBarIsVisibleInFullscreen) {
  InstallAndLaunchBookmarkApp();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(app_browser_, GURL("http://example.com")));

  BrowserWebContentsDelegate::From(app_browser_)
      ->EnterFullscreenModeForTab(web_contents_->GetPrimaryMainFrame(), {});

  EXPECT_TRUE(app_browser_view_->toolbar()->custom_tab_bar()->IsDrawn());
}

// Tests that hosted app frames reflect the theme color set by HTML meta tags.
IN_PROC_BROWSER_TEST_F(BrowserFrameViewBrowserTest,
                       HTMLMetaThemeColorOverridesManifest) {
  // Ensure we're not using the system theme on Linux.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->GetProfile());
  theme_service->UseDefaultTheme();

  InstallAndLaunchBookmarkApp();
  ASSERT_EQ(app_theme_color_, SK_ColorBLUE);
  EXPECT_EQ(
      GetAppFrameView()->GetFrameColor(BrowserFrameActiveState::kUseCurrent),
      app_theme_color_);

  views::View* const window_title_view =
      GetAppFrameView()->GetViewByID(VIEW_ID_WINDOW_TITLE);
  views::Label* const window_title =
      window_title_view ? static_cast<views::Label*>(window_title_view)
                        : nullptr;
  if (window_title) {
    EXPECT_EQ(window_title->GetBackgroundColor(), app_theme_color_);
  }

  {
    // Add two meta theme color elements. The first element's color should be
    // picked.
    content::ThemeChangeWaiter waiter(web_contents_);
    EXPECT_TRUE(content::ExecJs(
        web_contents_.get(),
        "document.documentElement.innerHTML = '"
        "<meta id=\"first\"  name=\"theme-color\" content=\"red\">"
        "<meta id=\"second\" name=\"theme-color\" content=\"#00ff00\">'"));
    waiter.Wait();

    EXPECT_EQ(
        GetAppFrameView()->GetFrameColor(BrowserFrameActiveState::kUseCurrent),
        SK_ColorRED);
    if (window_title) {
      EXPECT_EQ(window_title->GetBackgroundColor(), SK_ColorRED);
    }
  }
  {
    // Change the color of the first element. The new color should be picked.
    content::ThemeChangeWaiter waiter(web_contents_);
    EXPECT_TRUE(content::ExecJs(
        web_contents_.get(),
        "document.getElementById('first').setAttribute('content', 'yellow')"));
    waiter.Wait();

    EXPECT_EQ(
        GetAppFrameView()->GetFrameColor(BrowserFrameActiveState::kUseCurrent),
        SK_ColorYELLOW);
    if (window_title) {
      EXPECT_EQ(window_title->GetBackgroundColor(), SK_ColorYELLOW);
    }
  }
  {
    // Set a non matching media query to the first element. The second element's
    // color should be picked.
    content::ThemeChangeWaiter waiter(web_contents_);
    EXPECT_TRUE(content::ExecJs(web_contents_.get(),
                                "document.getElementById('first')."
                                "setAttribute('media', '(max-width: 0px)')"));
    waiter.Wait();

    EXPECT_EQ(
        GetAppFrameView()->GetFrameColor(BrowserFrameActiveState::kUseCurrent),
        SK_ColorGREEN);
  }
  {
    // Remove the second element. The manifest color should be picked because
    // the first element still does not match.
    content::ThemeChangeWaiter waiter(web_contents_);
    EXPECT_TRUE(content::ExecJs(web_contents_.get(),
                                "document.getElementById('second').remove()"));
    waiter.Wait();

    EXPECT_EQ(
        GetAppFrameView()->GetFrameColor(BrowserFrameActiveState::kUseCurrent),
        SK_ColorBLUE);
  }
  {
    // Set a matching media query to the first element. The first element's
    // color should be picked.
    content::ThemeChangeWaiter waiter(web_contents_);
    std::string width =
        content::EvalJs(web_contents_.get(), "innerWidth.toString()")
            .ExtractString();
    EXPECT_TRUE(content::ExecJs(web_contents_.get(),
                                "document.getElementById('first')."
                                "setAttribute('media', '(max-width: " +
                                    width + "px')"));
    waiter.Wait();

    EXPECT_EQ(
        GetAppFrameView()->GetFrameColor(BrowserFrameActiveState::kUseCurrent),
        SK_ColorYELLOW);
  }
  {
    // Resize the window so that the media query on the first element does not
    // match anymore. The manifest color should be picked.
    content::ThemeChangeWaiter waiter(web_contents_);
    EXPECT_TRUE(content::ExecJs(web_contents_.get(), "window.resizeBy(24, 0)"));
    waiter.Wait();

    EXPECT_EQ(
        GetAppFrameView()->GetFrameColor(BrowserFrameActiveState::kUseCurrent),
        SK_ColorBLUE);
  }
}

class SaveCardOfferObserver
    : public autofill::CreditCardSaveManager::ObserverForTest {
 public:
  explicit SaveCardOfferObserver(content::WebContents* web_contents) {
    manager_ = autofill::ContentAutofillDriver::GetForRenderFrameHost(
                   web_contents->GetPrimaryMainFrame())
                   ->GetAutofillManager()
                   .client()
                   .GetFormDataImporter()
                   ->GetPaymentsFormDataImporter()
                   .GetCreditCardSaveManager();
    manager_->SetEventObserverForTesting(this);
  }

  ~SaveCardOfferObserver() override {
    manager_->SetEventObserverForTesting(nullptr);
  }

  // CreditCardSaveManager::ObserverForTest:
  void OnOfferLocalSave() override { run_loop_.Quit(); }

  void Wait() { run_loop_.Run(); }

 private:
  raw_ptr<autofill::CreditCardSaveManager> manager_ = nullptr;
  base::RunLoop run_loop_;
};

// TODO(crbug.com/40866991): Test is flaky.
// Tests that hosted app frames reflect the theme color set by HTML meta tags.
IN_PROC_BROWSER_TEST_F(BrowserFrameViewBrowserTest, DISABLED_SaveCardIcon) {
  InstallAndLaunchBookmarkApp(embedded_test_server()->GetURL(
      "/autofill/credit_card_upload_form_address_and_cc.html"));
  ASSERT_TRUE(autofill_manager_injector_[web_contents_]->WaitForFormsSeen(1));
  ASSERT_TRUE(content::ExecJs(web_contents_.get(), "fill_form.click();"));

  content::TestNavigationObserver nav_observer(web_contents_);
  SaveCardOfferObserver offer_observer(web_contents_);
  ASSERT_TRUE(content::ExecJs(web_contents_.get(), "submit.click();"));
  nav_observer.Wait();
  offer_observer.Wait();

  page_actions::PageActionViewInterface* page_action_interface =
      app_browser_view_->toolbar_button_provider()->GetPageActionViewInterface(
          kActionShowPaymentsBubbleOrPage);
  ASSERT_TRUE(page_action_interface);
  views::View* icon =
      page_action_interface->GetIconLabelBubbleViewNotMigrated();
  EXPECT_TRUE(GetAppFrameView()->Contains(icon));
  EXPECT_TRUE(icon->GetVisible());
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(BrowserFrameViewBrowserTest, BrowserFrameWindowMask) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserFrameView* frame_view = browser_view->browser_widget()->GetFrameView();
  SkPath path;
  frame_view->GetWindowMask(frame_view->bounds().size(), &path);
  EXPECT_TRUE(path.isEmpty());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

using BrowserFrameViewPopupTest = InProcessBrowserTest;

// TODO(crbug.com/41478509): Flaky on Linux TSAN and ASAN.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    (defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER))
#define MAYBE_HitTestPopupTopChrome DISABLED_HitTestPopupTopChrome
#else
#define MAYBE_HitTestPopupTopChrome HitTestPopupTopChrome
#endif
IN_PROC_BROWSER_TEST_F(BrowserFrameViewPopupTest, MAYBE_HitTestPopupTopChrome) {
  BrowserWindowInterface* popup_browser =
      CreateBrowserForPopup(browser()->GetProfile());
  BrowserView* popup_browser_view =
      BrowserView::GetBrowserViewForBrowser(popup_browser);
  BrowserFrameView* frame_view =
      popup_browser_view->browser_widget()->GetFrameView();

  constexpr gfx::Rect kLeftOfFrame(-1, 4, 1, 1);
  EXPECT_FALSE(frame_view->HitTestRect(kLeftOfFrame));

  constexpr gfx::Rect kAboveFrame(4, -1, 1, 1);
  EXPECT_FALSE(frame_view->HitTestRect(kAboveFrame));

  const int top_inset = frame_view->GetTopInset(false);
  const gfx::Rect in_browser_view(4, top_inset, 1, 1);
  EXPECT_TRUE(frame_view->HitTestRect(in_browser_view));
}

using BrowserFrameViewTabbedTest = InProcessBrowserTest;

// TODO(crbug.com/40101869): Flaky on Linux TSAN.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(THREAD_SANITIZER)
#define MAYBE_HitTestTabstrip DISABLED_HitTestTabstrip
#else
#define MAYBE_HitTestTabstrip HitTestTabstrip
#endif

IN_PROC_BROWSER_TEST_F(BrowserFrameViewTabbedTest, MAYBE_HitTestTabstrip) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserFrameView* frame_view = browser_view->browser_widget()->GetFrameView();

  const gfx::Rect frame_bounds = frame_view->bounds();

  gfx::RectF tabstrip_bounds_in_frame_coords(
      browser_view->horizontal_tab_strip_for_testing()->GetLocalBounds());
  views::View::ConvertRectToTarget(
      browser_view->horizontal_tab_strip_for_testing(), frame_view,
      &tabstrip_bounds_in_frame_coords);
  const gfx::Rect tabstrip_bounds =
      gfx::ToEnclosingRect(tabstrip_bounds_in_frame_coords);
  EXPECT_FALSE(tabstrip_bounds.IsEmpty());

  // Completely outside the frame's bounds.
  EXPECT_FALSE(frame_view->HitTestRect(
      gfx::Rect(frame_bounds.x() - 1, frame_bounds.y() + 1, 1, 1)));
  EXPECT_FALSE(frame_view->HitTestRect(
      gfx::Rect(frame_bounds.x() + 1, frame_bounds.y() - 1, 1, 1)));

  // Hits client portions of the tabstrip (near the bottom left corner of the
  // first tab).
  EXPECT_TRUE(frame_view->HitTestRect(gfx::Rect(
      tabstrip_bounds.x() + 10, tabstrip_bounds.bottom() - 10, 1, 1)));
  EXPECT_TRUE(browser_view->HitTestRect(gfx::Rect(
      tabstrip_bounds.x() + 10, tabstrip_bounds.bottom() - 10, 1, 1)));

#if !BUILDFLAG(IS_CHROMEOS)
  // Hits non-client portions of the tab strip (the top left corner of the
  // first tab).
  EXPECT_TRUE(frame_view->HitTestRect(
      gfx::Rect(tabstrip_bounds.x(), tabstrip_bounds.y(), 1, 1)));
#endif

  // Hits tab strip and the browser-client area.
  EXPECT_TRUE(frame_view->HitTestRect(gfx::Rect(
      tabstrip_bounds.x() + 1,
      tabstrip_bounds.bottom() -
          GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap) - 1,
      100, 100)));
}
