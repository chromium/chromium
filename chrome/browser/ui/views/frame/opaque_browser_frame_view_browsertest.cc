// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_helper.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/test_views.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_LINUX)
#include <algorithm>
#include <cmath>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/themes/chrome_linux_ui_getter.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_factory.h"
#include "ui/linux/linux_ui_getter.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/views/test/views_test_utils.h"

class FakeLinuxUiGetter : public ChromeLinuxUiGetter {
 public:
  explicit FakeLinuxUiGetter(bool use_system_theme)
      : use_system_theme_(use_system_theme) {}

  ui::LinuxUiTheme* GetForWindow(aura::Window* window) override {
    return GetForProfile(nullptr);
  }

  ui::LinuxUiTheme* GetForProfile(const Profile* profile) override {
    return use_system_theme_ ? ui::GetDefaultLinuxUiTheme() : nullptr;
  }

 private:
  const bool use_system_theme_;
};
#endif

// Tests web-app windows that use the OpaqueBrowserFrameView implementation
// for their non client frames.
class WebAppOpaqueBrowserFrameViewTest : public web_app::WebAppBrowserTestBase {
 public:
  WebAppOpaqueBrowserFrameViewTest() = default;

  WebAppOpaqueBrowserFrameViewTest(const WebAppOpaqueBrowserFrameViewTest&) =
      delete;
  WebAppOpaqueBrowserFrameViewTest& operator=(
      const WebAppOpaqueBrowserFrameViewTest&) = delete;

  ~WebAppOpaqueBrowserFrameViewTest() override = default;

  GURL GetAppURL() {
    // The page shouldn't have a manifest so no updating will occur to override
    // our settings.
    return embedded_https_test_server().GetURL("/web_apps/no_manifest.html");
  }

  void SetUpOnMainThread() override {
    web_app::WebAppBrowserTestBase::SetUpOnMainThread();
    SetThemeMode(ThemeMode::kDefault);
#if BUILDFLAG(IS_LINUX)
    ui::LinuxUiGetter::set_instance(nullptr);
#endif
  }

  bool InstallAndLaunchWebApp(
      std::optional<SkColor> theme_color = std::nullopt) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(GetAppURL());
    web_app_info->scope = GetAppURL().GetWithoutFilename();
    web_app_info->theme_color = theme_color;

    webapps::AppId app_id = web_app::test::InstallWebApp(
        browser()->GetProfile(), std::move(web_app_info));
    BrowserWindowInterface* app_browser =
        web_app::LaunchWebAppBrowser(browser()->GetProfile(), app_id);

    browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser);
    views::FrameView* frame_view =
        browser_view_->GetWidget()->non_client_view()->frame_view();

    // Not all platform configurations use OpaqueBrowserFrameView for their
    // browser windows, see |CreateBrowserFrameView()|.
    bool is_opaque_browser_frame_view =
        views::IsViewClass<OpaqueBrowserFrameView>(frame_view);
#if BUILDFLAG(IS_LINUX)
    DCHECK(is_opaque_browser_frame_view);
#else
    if (!is_opaque_browser_frame_view) {
      return false;
    }
#endif

    opaque_browser_frame_view_ =
        static_cast<OpaqueBrowserFrameView*>(frame_view);
    web_app_frame_toolbar_ = browser_view_->web_app_frame_toolbar_for_testing();
    DCHECK(web_app_frame_toolbar_);
    DCHECK(web_app_frame_toolbar_->GetVisible());

    return true;
  }

  int GetRestoredTitleBarHeight() {
    return opaque_browser_frame_view_->layout()->NonClientTopHeight(true);
  }

  enum class ThemeMode {
    kSystem,
    kDefault,
  };

  void SetThemeMode(ThemeMode theme_mode) {
#if BUILDFLAG(IS_LINUX)
    bool use_system_theme = theme_mode == ThemeMode::kSystem;
    linux_ui_getter_ = std::make_unique<FakeLinuxUiGetter>(use_system_theme);
#endif
    ThemeService* theme_service =
        ThemeServiceFactory::GetForProfile(browser()->GetProfile());
    if (theme_mode == ThemeMode::kSystem) {
      theme_service->UseSystemTheme();
    } else {
      theme_service->UseDefaultTheme();
    }
    ASSERT_EQ(theme_service->UsingDefaultTheme(),
              theme_mode == ThemeMode::kDefault);
  }

  raw_ptr<BrowserView, AcrossTasksDanglingUntriaged> browser_view_ = nullptr;
  raw_ptr<OpaqueBrowserFrameView, AcrossTasksDanglingUntriaged>
      opaque_browser_frame_view_ = nullptr;
  raw_ptr<WebAppFrameToolbarView, AcrossTasksDanglingUntriaged>
      web_app_frame_toolbar_ = nullptr;
#if BUILDFLAG(IS_LINUX)
  std::unique_ptr<ui::LinuxUiGetter> linux_ui_getter_;
#endif
};

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, NoThemeColor) {
  if (!InstallAndLaunchWebApp()) {
    GTEST_SKIP();
  }
  EXPECT_EQ(web_app_frame_toolbar_->active_color_for_testing(),
            web_app_frame_toolbar_->GetColorProvider()->GetColor(
                kColorFrameCaptionActive));
}

#if BUILDFLAG(IS_LINUX)
// The app theme color should be ignored in system theme mode.
IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, SystemThemeColor) {
  SetThemeMode(ThemeMode::kSystem);

  // Read unthemed native frame color.
  SkColor native_frame_color =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->browser_widget()
          ->GetFrameView()
          ->GetFrameColor(BrowserFrameActiveState::kActive);
  SkColor expected_caption_color =
      color_utils::GetColorWithMaxContrast(native_frame_color);

  // Install web app with theme color contrasting against native frame color.
  SkColor theme_color =
      color_utils::GetColorWithMaxContrast(native_frame_color);
  EXPECT_NE(color_utils::IsDark(theme_color),
            color_utils::IsDark(native_frame_color));
  ASSERT_TRUE(InstallAndLaunchWebApp(theme_color));

  // App theme color should be ignored in favor of native system theme.
  EXPECT_EQ(opaque_browser_frame_view_->GetFrameColor(
                BrowserFrameActiveState::kActive),
            native_frame_color);
  EXPECT_EQ(opaque_browser_frame_view_->GetCaptionColor(
                BrowserFrameActiveState::kActive),
            expected_caption_color);
  EXPECT_EQ(web_app_frame_toolbar_->active_color_for_testing(),
            expected_caption_color);
}
#endif  // BUILDFLAG(IS_LINUX)

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, LightThemeColor) {
  if (!InstallAndLaunchWebApp(SK_ColorYELLOW)) {
    GTEST_SKIP();
  }
  EXPECT_EQ(web_app_frame_toolbar_->active_color_for_testing(),
            web_app_frame_toolbar_->GetColorProvider()->GetColor(
                kColorFrameCaptionActive));
  EXPECT_TRUE(
      color_utils::IsDark(*web_app_frame_toolbar_->active_color_for_testing()));
}

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, DarkThemeColor) {
  if (!InstallAndLaunchWebApp(SK_ColorBLUE)) {
    GTEST_SKIP();
  }
  EXPECT_EQ(web_app_frame_toolbar_->active_color_for_testing(),
            web_app_frame_toolbar_->GetColorProvider()->GetColor(
                kColorFrameCaptionActive));
  EXPECT_FALSE(
      color_utils::IsDark(*web_app_frame_toolbar_->active_color_for_testing()));
}

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, MediumThemeColor) {
  // Use the theme color for Gmail.
  if (!InstallAndLaunchWebApp(SkColorSetRGB(0xD6, 0x49, 0x3B))) {
    GTEST_SKIP();
  }
  EXPECT_EQ(web_app_frame_toolbar_->active_color_for_testing(),
            web_app_frame_toolbar_->GetColorProvider()->GetColor(
                kColorFrameCaptionActive));
  EXPECT_FALSE(
      color_utils::IsDark(*web_app_frame_toolbar_->active_color_for_testing()));
}

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, StaticTitleBarHeight) {
  if (!InstallAndLaunchWebApp()) {
    GTEST_SKIP();
  }

  RunScheduledLayouts();
  const int title_bar_height = GetRestoredTitleBarHeight();
  EXPECT_GT(title_bar_height, 0);

  // Add taller children to the web app frame toolbar RHS.
  const int container_height = web_app_frame_toolbar_->height();
  web_app_frame_toolbar_->get_right_container_for_testing()->AddChildViewRaw(
      new views::StaticSizedView(gfx::Size(1, title_bar_height * 2)));
  RunScheduledLayouts();

  // The height of the web app frame toolbar and title bar should not be
  // affected.
  EXPECT_EQ(container_height, web_app_frame_toolbar_->height());
  EXPECT_EQ(title_bar_height, GetRestoredTitleBarHeight());
}

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, Fullscreen) {
  if (!InstallAndLaunchWebApp()) {
    GTEST_SKIP();
  }

  opaque_browser_frame_view_->browser_widget()->SetFullscreen(true);
  browser_view_->GetWidget()->LayoutRootViewIfNecessary();

  // Verify that all children except the ClientView are hidden when the window
  // is fullscreened.
  for (views::View* child : opaque_browser_frame_view_->children()) {
    EXPECT_EQ(views::IsViewClass<views::ClientView>(child),
              child->GetVisible());
  }
}

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, AccessibleProperties) {
  if (!InstallAndLaunchWebApp()) {
    GTEST_SKIP();
  }

  ui::AXNodeData data;

  opaque_browser_frame_view_->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.role, ax::mojom::Role::kPane);
}

#if BUILDFLAG(IS_WIN)
class WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest
    : public InProcessBrowserTest {
 public:
  WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest() = default;
  WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest(
      const WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest&) = delete;
  WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest& operator=(
      const WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest&) = delete;

  ~WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    embedded_test_server()->ServeFilesFromDirectory(temp_dir_.GetPath());
    ASSERT_TRUE(embedded_test_server()->Start());

    InProcessBrowserTest::SetUp();
  }

  bool InstallAndLaunchWebAppWithWindowControlsOverlay() {
    GURL start_url = web_app_frame_toolbar_helper_
                         .LoadWindowControlsOverlayTestPageWithDataAndGetURL(
                             embedded_test_server(), &temp_dir_);

    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->display_mode = blink::mojom::DisplayMode::kStandalone;
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    web_app_info->title = u"A Web App";
    web_app_info->display_override = {web_app::DisplayOverride::Create(
        blink::mojom::DisplayMode::kWindowControlsOverlay)};

    webapps::AppId app_id = web_app::test::InstallWebApp(
        browser()->GetProfile(), std::move(web_app_info));

    BrowserWindowInterface* app_browser =
        web_app::LaunchWebAppBrowser(browser()->GetProfile(), app_id);

    web_app::NavigateViaLinkClickToURLAndWait(app_browser, start_url);

    browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser);
    views::FrameView* frame_view =
        browser_view_->GetWidget()->non_client_view()->frame_view();

    // Not all platform configurations use OpaqueBrowserFrameView for their
    // browser windows, see |CreateBrowserFrameView()|.
    bool is_opaque_browser_frame_view =
        views::IsViewClass<OpaqueBrowserFrameView>(frame_view);

    if (!is_opaque_browser_frame_view) {
      return false;
    }

    opaque_browser_frame_view_ =
        static_cast<OpaqueBrowserFrameView*>(frame_view);
    auto* web_app_frame_toolbar =
        browser_view_->web_app_frame_toolbar_for_testing();
    DCHECK(web_app_frame_toolbar);
    DCHECK(web_app_frame_toolbar->GetVisible());

    return true;
  }

  void ToggleWindowControlsOverlayEnabledAndWait() {
    auto* web_contents = browser_view_->GetActiveWebContents();
    web_app_frame_toolbar_helper_.SetupGeometryChangeCallback(web_contents);
    base::test::TestFuture<void> future;
    browser_view_->ToggleWindowControlsOverlayEnabled(future.GetCallback());
    EXPECT_TRUE(future.Wait());
    content::TitleWatcher title_watcher(web_contents, u"ongeometrychange");
    std::ignore = title_watcher.WaitAndGetTitle();
  }

  raw_ptr<BrowserView, AcrossTasksDanglingUntriaged> browser_view_ = nullptr;
  raw_ptr<OpaqueBrowserFrameView, AcrossTasksDanglingUntriaged>
      opaque_browser_frame_view_ = nullptr;
  WebAppFrameToolbarTestHelper web_app_frame_toolbar_helper_;

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest,
                       CaptionButtonsTooltip) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay()) {
    GTEST_SKIP() << "Skip test if it is not a OpaqueBrowserFrameView";
  }

  auto* minimize_button = static_cast<const views::Button*>(
      opaque_browser_frame_view_->GetViewByID(VIEW_ID_MINIMIZE_BUTTON));
  auto* maximize_button = static_cast<const views::Button*>(
      opaque_browser_frame_view_->GetViewByID(VIEW_ID_MAXIMIZE_BUTTON));
  auto* restore_button = static_cast<const views::Button*>(
      opaque_browser_frame_view_->GetViewByID(VIEW_ID_RESTORE_BUTTON));
  auto* close_button = static_cast<const views::Button*>(
      opaque_browser_frame_view_->GetViewByID(VIEW_ID_CLOSE_BUTTON));

  // Verify tooltip text was first empty.
  EXPECT_EQ(minimize_button->GetTooltipText(), u"");
  EXPECT_EQ(maximize_button->GetTooltipText(), u"");
  EXPECT_EQ(restore_button->GetTooltipText(), u"");
  EXPECT_EQ(close_button->GetTooltipText(), u"");

  ToggleWindowControlsOverlayEnabledAndWait();

  // Verify tooltip text has been updated.
  EXPECT_EQ(minimize_button->GetTooltipText(),
            minimize_button->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(maximize_button->GetTooltipText(),
            maximize_button->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(restore_button->GetTooltipText(),
            restore_button->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(close_button->GetTooltipText(),
            close_button->GetViewAccessibility().GetCachedName());

  ToggleWindowControlsOverlayEnabledAndWait();

  // Verify tooltip text has been cleared when the feature is toggled off.
  EXPECT_EQ(minimize_button->GetTooltipText(), u"");
  EXPECT_EQ(maximize_button->GetTooltipText(), u"");
  EXPECT_EQ(restore_button->GetTooltipText(), u"");
  EXPECT_EQ(close_button->GetTooltipText(), u"");
}

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest,
                       CaptionButtonHitTest) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay()) {
    GTEST_SKIP() << "Skip test if it is not a OpaqueBrowserFrameView";
  }

  opaque_browser_frame_view_->GetWidget()->LayoutRootViewIfNecessary();

  // Avoid the top right resize corner.
  constexpr int kInset = 10;
  const gfx::Point kPoint(opaque_browser_frame_view_->width() - kInset, kInset);

  EXPECT_EQ(opaque_browser_frame_view_->NonClientHitTest(kPoint), HTCLOSE);

  ToggleWindowControlsOverlayEnabledAndWait();

  // Verify the component updates on toggle.
  EXPECT_EQ(opaque_browser_frame_view_->NonClientHitTest(kPoint), HTCLIENT);

  ToggleWindowControlsOverlayEnabledAndWait();

  // Verify the component clears when the feature is turned off.
  EXPECT_EQ(opaque_browser_frame_view_->NonClientHitTest(kPoint), HTCLOSE);
}
#endif

#if BUILDFLAG(IS_LINUX)
namespace {

struct TestNavButtonConfig {
  gfx::Size button_size{24, 48};
  gfx::Insets button_margin = gfx::Insets::TLBR(5, 2, 0, 2);
  int redraw_count = 0;
};

class ConfigurableNavButtonProvider : public ui::NavButtonProvider {
 public:
  explicit ConfigurableNavButtonProvider(
      scoped_refptr<base::RefCountedData<TestNavButtonConfig>> config)
      : config_(std::move(config)) {}
  ~ConfigurableNavButtonProvider() override = default;

  void RedrawImages(int top_area_height, bool maximized, bool active) override {
    ++config_->data.redraw_count;
    const int clamped_top_area_height = std::max(0, top_area_height);
    const int effective_top = std::max(0, config_->data.button_margin.top());
    const int effective_bottom =
        std::max(0, config_->data.button_margin.bottom());
    const int needed_height =
        effective_top + config_->data.button_size.height() + effective_bottom;
    double scale = 1.0;
    if (needed_height > clamped_top_area_height && needed_height > 0) {
      scale = static_cast<double>(clamped_top_area_height) / needed_height;
    }
    const gfx::Size scaled_size(
        std::max(1, static_cast<int>(
                        std::round(scale * config_->data.button_size.width()))),
        std::clamp(static_cast<int>(
                       std::round(scale * config_->data.button_size.height())),
                   0, clamped_top_area_height));
    const double occupied_height = scale * needed_height;
    const double offset = (clamped_top_area_height - occupied_height) / 2.0;
    const int top_margin =
        std::clamp(static_cast<int>(std::round(scale * effective_top + offset)),
                   0, clamped_top_area_height - scaled_size.height());

    SkBitmap bitmap;
    if (!scaled_size.IsEmpty()) {
      bitmap.allocN32Pixels(scaled_size.width(), scaled_size.height());
      bitmap.eraseColor(SK_ColorRED);
    }
    image_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    margin_ = gfx::Insets::TLBR(
        top_margin,
        static_cast<int>(
            std::round(scale * config_->data.button_margin.left())),
        0,
        static_cast<int>(
            std::round(scale * config_->data.button_margin.right())));
  }

  gfx::ImageSkia GetImage(FrameButtonDisplayType type,
                          ButtonState state) const override {
    return image_;
  }

  gfx::Insets GetNavButtonMargin(FrameButtonDisplayType type) const override {
    return margin_;
  }

  gfx::Insets GetTopAreaSpacing() const override { return gfx::Insets(); }

  int GetNavButtonHeight(bool maximized) const override {
    return config_->data.button_size.height() +
           config_->data.button_margin.height();
  }

  int GetInterNavButtonSpacing() const override { return 2; }

 private:
  scoped_refptr<base::RefCountedData<TestNavButtonConfig>> config_;
  gfx::ImageSkia image_;
  gfx::Insets margin_;
};

class TestLinuxUiTheme : public ui::LinuxUiTheme {
 public:
  TestLinuxUiTheme(
      ui::LinuxUiTheme* delegate,
      scoped_refptr<base::RefCountedData<TestNavButtonConfig>> config)
      : delegate_(delegate), config_(std::move(config)) {}
  ~TestLinuxUiTheme() override = default;

  ui::NativeTheme* GetNativeTheme() const override {
    return delegate_->GetNativeTheme();
  }
  bool GetColor(int id, SkColor* color, bool use_custom_frame) const override {
    return delegate_->GetColor(id, color, use_custom_frame);
  }
  bool GetDisplayProperty(int id, int* result) const override {
    return delegate_->GetDisplayProperty(id, result);
  }
  void GetFocusRingColor(SkColor* color) const override {
    delegate_->GetFocusRingColor(color);
  }
  void GetActiveSelectionBgColor(SkColor* color) const override {
    delegate_->GetActiveSelectionBgColor(color);
  }
  void GetActiveSelectionFgColor(SkColor* color) const override {
    delegate_->GetActiveSelectionFgColor(color);
  }
  void GetInactiveSelectionBgColor(SkColor* color) const override {
    delegate_->GetInactiveSelectionBgColor(color);
  }
  void GetInactiveSelectionFgColor(SkColor* color) const override {
    delegate_->GetInactiveSelectionFgColor(color);
  }
  bool PreferDarkTheme() const override { return delegate_->PreferDarkTheme(); }
  void SetDarkTheme(bool dark) override { delegate_->SetDarkTheme(dark); }
  void SetColorScheme(std::optional<bool> prefer_dark) override {
    delegate_->SetColorScheme(prefer_dark);
  }
  void SetAccentColor(std::optional<SkColor> accent_color) override {
    delegate_->SetAccentColor(accent_color);
  }
  std::unique_ptr<ui::NavButtonProvider> CreateNavButtonProvider(
      ui::FrameType type) override {
    return std::make_unique<ConfigurableNavButtonProvider>(config_);
  }
  ui::WindowFrameProvider* GetWindowFrameProvider(ui::FrameType type,
                                                  bool solid_frame,
                                                  bool tiled,
                                                  bool maximized) override {
    return delegate_->GetWindowFrameProvider(type, solid_frame, tiled,
                                             maximized);
  }

 private:
  raw_ptr<ui::LinuxUiTheme> delegate_;
  scoped_refptr<base::RefCountedData<TestNavButtonConfig>> config_;
};

class CustomThemeLinuxUiGetter : public ChromeLinuxUiGetter {
 public:
  explicit CustomThemeLinuxUiGetter(ui::LinuxUiTheme* theme) : theme_(theme) {}
  ~CustomThemeLinuxUiGetter() override = default;

  ui::LinuxUiTheme* GetForWindow(aura::Window* window) override {
    return theme_;
  }
  ui::LinuxUiTheme* GetForProfile(const Profile* profile) override {
    return theme_;
  }

 private:
  raw_ptr<ui::LinuxUiTheme> theme_;
};

using LinuxNativeBrowserFrameViewTest = InProcessBrowserTest;

// Regression test for crbug.com/483769296: native Linux caption buttons sized
// via `NavButtonProvider::RedrawImages()` must fit within the frame's top area
// (without stretching `TabStripRegionView` past `TabStrip` or overlapping the
// content area of popup windows), and `OnThemeChanged()` must invalidate cached
// button images and update button bounds and tabstrip layout on the next layout
// pass.
IN_PROC_BROWSER_TEST_F(LinuxNativeBrowserFrameViewTest,
                       CaptionButtonsFitTopAreaAndUpdateOnThemeChange) {
  ui::LinuxUiTheme* default_theme = ui::GetDefaultLinuxUiTheme();
  if (!default_theme) {
    GTEST_SKIP() << "Default LinuxUiTheme unavailable";
  }

  auto config =
      base::MakeRefCounted<base::RefCountedData<TestNavButtonConfig>>();
  // Oversized 53px unscaled button (`5 + 48 > 40`): `RedrawImages()` scales it
  // into the available top area height so `TabStripRegionView` stays at the
  // preferred `TabStrip` height with no gap above `ToolbarView`.
  config->data.button_size = gfx::Size(24, 48);
  config->data.button_margin = gfx::Insets::TLBR(5, 2, 0, 2);

  TestLinuxUiTheme test_theme(default_theme, config);
  CustomThemeLinuxUiGetter getter(&test_theme);
  ThemeServiceFactory::GetForProfile(browser()->GetProfile())->UseSystemTheme();

  BrowserWindowInterface* system_browser =
      CreateBrowser(browser()->GetProfile());
  BrowserView* view = BrowserView::GetBrowserViewForBrowser(system_browser);
  BrowserFrameView* frame_view = view->browser_widget()->GetFrameView();
  views::test::RunScheduledLayout(view->GetWidget()->GetRootView());

  ASSERT_GT(config->data.redraw_count, 0);
  const int expected_tabstrip_height =
      view->horizontal_tab_strip_for_testing()->GetPreferredSize().height();
  const BrowserLayoutParams initial_params =
      frame_view->GetBrowserLayoutParams();
  const float initial_exclusion_height =
      std::max(initial_params.leading_exclusion.content.height(),
               initial_params.trailing_exclusion.content.height());
  const float initial_exclusion_width =
      initial_params.leading_exclusion.content.width() +
      initial_params.trailing_exclusion.content.width();
  EXPECT_LE(initial_exclusion_height, expected_tabstrip_height);
  EXPECT_EQ(view->tab_strip_view()->height(), expected_tabstrip_height);
  const gfx::Rect toolbar_bounds_in_browser = views::View::ConvertRectToTarget(
      view->toolbar()->parent(), view, view->toolbar()->bounds());
  EXPECT_EQ(toolbar_bounds_in_browser.y(),
            view->tab_strip_view()->bounds().bottom() -
                GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap));

  // In a popup window, caption buttons live in the non-client titlebar above
  // `visual_client_area` and must not overlap the content area vertically.
  BrowserWindowInterface* popup_browser =
      CreateBrowserForPopup(browser()->GetProfile());
  BrowserView* popup_view =
      BrowserView::GetBrowserViewForBrowser(popup_browser);
  views::test::RunScheduledLayout(popup_view->GetWidget()->GetRootView());
  const BrowserLayoutParams popup_params =
      popup_view->browser_widget()->GetFrameView()->GetBrowserLayoutParams();
  EXPECT_EQ(popup_params.leading_exclusion.content.height(), 0.0f);
  EXPECT_EQ(popup_params.trailing_exclusion.content.height(), 0.0f);
  CloseBrowserSynchronously(popup_browser);

  // Simulate a runtime GTK theme change that alters button dimensions without
  // changing `{top_area_height, maximized, active}`.
  const int redraws_before_theme_change = config->data.redraw_count;
  const int tabstrip_width_before = view->tab_strip_view()->width();
  config->data.button_size = gfx::Size(36, 18);
  config->data.button_margin = gfx::Insets::TLBR(4, 6, 0, 6);

  view->GetWidget()->ThemeChanged();
  views::test::RunScheduledLayout(view->GetWidget()->GetRootView());

  const BrowserLayoutParams updated_params =
      frame_view->GetBrowserLayoutParams();
  EXPECT_GT(config->data.redraw_count, redraws_before_theme_change);
  const float updated_exclusion_height =
      std::max(updated_params.leading_exclusion.content.height(),
               updated_params.trailing_exclusion.content.height());
  const float updated_exclusion_width =
      updated_params.leading_exclusion.content.width() +
      updated_params.trailing_exclusion.content.width();
  EXPECT_LT(updated_exclusion_height, initial_exclusion_height);
  EXPECT_GT(updated_exclusion_width, initial_exclusion_width);
  EXPECT_LT(view->tab_strip_view()->width(), tabstrip_width_before);

  // Querying `GetMinimumSize()` across fullscreen/maximized transitions before
  // scheduled layout runs must keep `visual_client_area` aligned with
  // `BrowserView::bounds()`.
  view->browser_widget()->SetFullscreen(true);
  EXPECT_FALSE(view->GetWidget()->GetMinimumSize().IsEmpty());
  views::test::RunScheduledLayout(view->GetWidget()->GetRootView());
  view->browser_widget()->SetFullscreen(false);
  EXPECT_FALSE(view->GetWidget()->GetMinimumSize().IsEmpty());
  views::test::RunScheduledLayout(view->GetWidget()->GetRootView());

  CloseBrowserSynchronously(system_browser);
  ui::LinuxUiGetter::set_instance(nullptr);
}

}  // namespace
#endif  // BUILDFLAG(IS_LINUX)
