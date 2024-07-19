// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/form_data_importer_test_api.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/theme_change_waiter.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"

namespace {

class TestAutofillManager : public autofill::BrowserAutofillManager {
 public:
  explicit TestAutofillManager(autofill::ContentAutofillDriver* driver)
      : BrowserAutofillManager(driver, "en-US") {}

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

class BrowserNonClientFrameViewBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  BrowserNonClientFrameViewBrowserTest() = default;

  BrowserNonClientFrameViewBrowserTest(
      const BrowserNonClientFrameViewBrowserTest&) = delete;
  BrowserNonClientFrameViewBrowserTest& operator=(
      const BrowserNonClientFrameViewBrowserTest&) = delete;

  ~BrowserNonClientFrameViewBrowserTest() override = default;

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
    manifest.scope = manifest.start_url.GetWithoutFilename();
    manifest.has_theme_color = true;
    manifest.theme_color = app_theme_color_;

    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
            manifest.start_url);
    web_app::UpdateWebAppInfoFromManifest(manifest, web_app_info.get());

    webapps::AppId app_id =
        web_app::test::InstallWebApp(profile(), std::move(web_app_info));
    app_browser_ = web_app::LaunchWebAppBrowser(profile(), app_id);
    web_contents_ = app_browser_->tab_strip_model()->GetActiveWebContents();
    // Ensure the main page has loaded and is ready for ExecJs DOM
    // manipulation.
    ASSERT_TRUE(content::NavigateToURL(web_contents_, manifest.start_url));

    app_browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser_);
  }

  // Frame view may get reset after theme change, so always access from the
  // browser view and don't retain the pointer.
  // TODO(crbug.com/40656280): Make it not do this and only refresh the Widget.
  BrowserNonClientFrameView* GetAppFrameView() {
    return app_browser_view_->frame()->GetFrameView();
  }

 protected:
  SkColor app_theme_color_ = SK_ColorBLUE;
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> app_browser_ = nullptr;
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
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
                       BrowserFrameColorThemed) {
  InstallExtension(test_data_dir_.AppendASCII("theme"), 1);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  const BrowserNonClientFrameView* frame_view =
      browser_view->frame()->GetFrameView();
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
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
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
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
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
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
                       OpaqueFrameColorForTransparentWebAppThemeColor) {
  // Ensure we're not using the system theme on Linux.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
  theme_service->UseDefaultTheme();

  app_theme_color_ = SkColorSetA(SK_ColorBLUE, 0x88);
  InstallAndLaunchBookmarkApp();
  EXPECT_EQ(GetAppFrameView()->GetFrameColor(BrowserFrameActiveState::kActive),
            SK_ColorBLUE);
}

// Tests the frame color for a bookmark app when the system theme is applied.
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
                       BookmarkAppFrameColorSystemTheme) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
  // Should be using the system theme by default, but this assert was not true
  // on the bots. Explicitly set.
  theme_service->UseSystemTheme();
  ASSERT_TRUE(theme_service->UsingSystemTheme());

  InstallAndLaunchBookmarkApp();
#if BUILDFLAG(IS_LINUX)
  // On Linux, the system theme is the GTK theme and should change the frame
  // color to the system color (not the app theme color); otherwise the title
  // and border would clash horribly with the GTK title bar.
  // (https://crbug.com/878636)
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
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
                       IncognitoIsCorrectColor) {
  // Set the color that's expected to be ignored.
  auto* theme = ui::NativeTheme::GetInstanceForNativeUi();
  theme->set_user_color(gfx::kGoogleBlue400);
  theme->NotifyOnNativeThemeUpdated();

  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());

  BrowserView* view = BrowserView::GetBrowserViewForBrowser(incognito_browser);
  BrowserFrame* frame = view->frame();
  BrowserNonClientFrameView* frame_view = frame->GetFrameView();

  color_utils::HSL frame_color_hsl;
  SkColorToHSL(frame_view->GetFrameColor(BrowserFrameActiveState::kActive),
               &frame_color_hsl);
  // Ensure that the frame color is very dark in Incognito.
  EXPECT_LT(frame_color_hsl.l, 0.2);

  incognito_browser->window()->Close();
}

// Checks that the title bar for hosted app windows is hidden when in fullscreen
// for tab mode.
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
                       FullscreenForTabTitlebarHeight) {
  InstallAndLaunchBookmarkApp();
  static_cast<content::WebContentsDelegate*>(app_browser_)
      ->EnterFullscreenModeForTab(web_contents_->GetPrimaryMainFrame(), {});

  EXPECT_EQ(GetAppFrameView()->GetTopInset(false), 0);
}

// Tests that the custom tab bar is visible in fullscreen mode.
// TODO(crbug.com/40855995): Flaky on linux-wayland-rel and linux-lacros-rel
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#define MAYBE_CustomTabBarIsVisibleInFullscreen \
  DISABLED_CustomTabBarIsVisibleInFullscreen
#else
#define MAYBE_CustomTabBarIsVisibleInFullscreen \
  CustomTabBarIsVisibleInFullscreen
#endif
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
                       MAYBE_CustomTabBarIsVisibleInFullscreen) {
  InstallAndLaunchBookmarkApp();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(app_browser_, GURL("http://example.com")));

  static_cast<content::WebContentsDelegate*>(app_browser_)
      ->EnterFullscreenModeForTab(web_contents_->GetPrimaryMainFrame(), {});

  EXPECT_TRUE(app_browser_view_->toolbar()->custom_tab_bar()->IsDrawn());
}

// Tests that hosted app frames reflect the theme color set by HTML meta tags.
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
                       HTMLMetaThemeColorOverridesManifest) {
  // Ensure we're not using the system theme on Linux.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
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
                   ->GetCreditCardSaveManager();
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
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
                       DISABLED_SaveCardIcon) {
  InstallAndLaunchBookmarkApp(embedded_test_server()->GetURL(
      "/autofill/credit_card_upload_form_address_and_cc.html"));
  ASSERT_TRUE(autofill_manager_injector_[web_contents_]->WaitForFormsSeen(1));
  ASSERT_TRUE(content::ExecJs(web_contents_.get(), "fill_form.click();"));

  content::TestNavigationObserver nav_observer(web_contents_);
  SaveCardOfferObserver offer_observer(web_contents_);
  ASSERT_TRUE(content::ExecJs(web_contents_.get(), "submit.click();"));
  nav_observer.Wait();
  offer_observer.Wait();

  PageActionIconView* icon =
      app_browser_view_->toolbar_button_provider()->GetPageActionIconView(
          PageActionIconType::kSaveCard);
  EXPECT_TRUE(GetAppFrameView()->Contains(icon));
  EXPECT_TRUE(icon->GetVisible());
}

#if BUILDFLAG(IS_CHROMEOS)
// Tests that GetWindowMask is supported for lacros in chromeos.
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewBrowserTest,
                       BrowserFrameWindowMask) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserNonClientFrameView* frame_view = browser_view->frame()->GetFrameView();
  SkPath path;
  frame_view->GetWindowMask(frame_view->bounds().size(), &path);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_FALSE(path.isEmpty());
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(path.isEmpty());
#endif
}
#endif  // BUILDFLAG(IS_CHROMEOS)
