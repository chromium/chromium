// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_chromeos.h"

#include <memory>
#include <optional>

#include "ash/multi_user/multi_user_window_manager.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_highlight_border_overlay_delegate.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/ash/session/session_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/tab_strip_view_interface.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/profiles/profile_indicator_icon.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_delegate.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_layout_constants.h"

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

DEFINE_UI_CLASS_PROPERTY_TYPE(BrowserFrameViewChromeOS*)

namespace {

// The indicator for teleported windows has 8 DIPs before and below it.
constexpr int kProfileIndicatorPadding = 8;

// Returns the layer for the specified `web_view`'s native view.
ui::Layer* GetNativeViewLayer(views::WebView* web_view) {
  if (web_view) {
    if (views::NativeViewHost* holder = web_view->holder(); holder) {
      if (aura::Window* native_view = holder->native_view(); native_view) {
        return native_view->layer();
      }
    }
  }
  return nullptr;
}

// Returns the render widget host for the specified `web_view`.
content::RenderWidgetHost* GetRenderWidgetHost(views::WebView* web_view) {
  if (web_view) {
    if (auto* web_contents = web_view->GetWebContents(); web_contents) {
      if (auto* rvh = web_contents->GetRenderViewHost(); rvh) {
        return rvh->GetWidget();
      }
    }
  }
  return nullptr;
}

// Returns whether `browser` should draw its frame header.
// The default is true.
bool ShouldDrawFrameHeader(BrowserWindowInterface* browser) {
  // Currently, frame headers are only disabled for custom tab browsers.
  return browser->GetType() != BrowserWindowInterface::Type::TYPE_CUSTOM_TAB;
}

DEFINE_UI_CLASS_PROPERTY_KEY(BrowserFrameViewChromeOS*,
                             kBrowserFrameViewChromeOSKey,
                             nullptr)

// Returns true if the header should be painted so that it looks the same as
// the header used for packaged apps.
bool UsePackagedAppHeaderStyle(const Browser* browser) {
  if (browser->is_type_normal() ||
      (browser->is_type_popup() && !browser->is_trusted_source())) {
    return false;
  }

  return !browser->SupportsWindowFeature(
      Browser::WindowFeature::kFeatureTabStrip);
}

// Whether or not the window's title should show the avatar. Practically,
// returns true when the owner of the window is different from the owner of
// the desktop.
bool ShouldShowAvatar(aura::Window* window) {
  auto* multi_user_window_manager =
      ash::Shell::Get()->multi_user_window_manager();
  return !multi_user_window_manager->IsWindowOnDesktopOfUser(
      window, multi_user_window_manager->GetWindowOwner(window));
}

}  // namespace

class BrowserFrameViewChromeOS::ProfileChangeObserver
    : public ProfileAttributesStorage::Observer {
 public:
  explicit ProfileChangeObserver(BrowserFrameViewChromeOS& frame)
      : frame_(frame) {
    if (g_browser_process->profile_manager()) {
      profile_observation_.Observe(
          &g_browser_process->profile_manager()->GetProfileAttributesStorage());
    } else {
      CHECK_IS_TEST();
    }
  }

  ~ProfileChangeObserver() override = default;

  // ProfileAttributesStorage::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override {
    frame_->UpdateProfileIcons();
  }
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const std::u16string& profile_name) override {
    frame_->UpdateProfileIcons();
  }
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override {
    frame_->UpdateProfileIcons();
  }
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override {
    frame_->UpdateProfileIcons();
  }

 private:
  raw_ref<BrowserFrameViewChromeOS> frame_;
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_observation_{this};
};

BrowserFrameViewChromeOS::BrowserFrameViewChromeOS(BrowserWidget* widget,
                                                   BrowserView* browser_view)
    : BrowserFrameView(widget, browser_view) {
  ash::window_util::InstallResizeHandleWindowTargeterForWindow(
      widget->GetNativeWindow());

  aura::Window* frame_window = widget->GetNativeWindow();
  frame_window->SetProperty(kBrowserFrameViewChromeOSKey, this);

  GetViewAccessibility().SetRole(ax::mojom::Role::kTitleBar);
}

BrowserFrameViewChromeOS::~BrowserFrameViewChromeOS() {
  if (auto* immersive_controller =
          ImmersiveModeController::From(browser_view()->browser())) {
    immersive_controller->RemoveObserver(this);
  }

  if (profile_indicator_icon_) {
    RemoveChildViewT(std::exchange(profile_indicator_icon_, nullptr));
  }
}

BrowserFrameViewChromeOS* BrowserFrameViewChromeOS::Get(aura::Window* window) {
  return window->GetProperty(kBrowserFrameViewChromeOSKey);
}

void BrowserFrameViewChromeOS::Init() {
  Browser* browser = browser_view()->browser();

  const bool is_close_button_enabled =
      !(browser->app_controller() &&
        browser->app_controller()->IsPreventCloseEnabled());

  caption_button_container_ =
      AddChildView(std::make_unique<chromeos::FrameCaptionButtonContainerView>(
          browser_widget(), is_close_button_enabled));

  // Initializing the TabIconView is expensive, so only do it if we need to.
  if (browser_view()->ShouldShowWindowIcon()) {
    AddChildView(views::Builder<TabIconView>()
                     .CopyAddressTo(&window_icon_)
                     .SetModel(this)
                     .Build());
  }

  UpdateProfileIcons();

  window_observation_.Observe(GetFrameWindow());

  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
          browser->profile())) {
    app_registry_cache_observation_.Observe(
        &apps::AppServiceProxyFactory::GetForProfile(browser->profile())
             ->AppRegistryCache());
  }

  // To preserve privacy, tag incognito windows so that they won't be included
  // in screenshot sent to assistant server.
  if (browser->profile()->IsOffTheRecord()) {
    browser_widget()->GetNativeWindow()->SetProperty(
        chromeos::kBlockedForAssistantSnapshotKey, true);
  }

  display_observer_.emplace(this);
  if (ShouldDrawFrameHeader(browser)) {
    frame_header_ = CreateFrameHeader();
  }

  if (AppIsPwaWithBorderlessDisplayMode()) {
    UpdateBorderlessModeEnabled();
  }

  ImmersiveModeController::From(browser_view()->browser())->AddObserver(this);
}

BrowserLayoutParams BrowserFrameViewChromeOS::GetBrowserLayoutParams() const {
  BrowserLayoutParams params;
  params.visual_client_area = GetLocalBounds();
  if (profile_indicator_icon_) {
    params.leading_exclusion.content =
        gfx::SizeF(profile_indicator_icon_->bounds().right(),
                   profile_indicator_icon_->bounds().bottom());
    params.leading_exclusion.horizontal_padding = kProfileIndicatorPadding;
    params.leading_exclusion.vertical_padding =
        profile_indicator_icon_->bounds().y();
  }
  if (GetShowCaptionButtonsWhenNotInOverview()) {
    const auto caption_bounds = caption_button_container_->bounds();
    // When the tabstrip is present, the caption button container is cut down to
    // the preferred height of the tabstrip.
    const int tabstrip_height = browser_view()->GetTabStripHeight();
    const int height =
        tabstrip_height ? tabstrip_height : caption_bounds.height();
    params.trailing_exclusion.content =
        gfx::SizeF(width() - caption_bounds.x(), height);
  }
  return params;
}

gfx::Rect BrowserFrameViewChromeOS::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size) const {
  const int left_inset = GetTabStripLeftInset();
  const bool restored =
      !browser_widget()->IsMaximized() && !browser_widget()->IsFullscreen();
  return gfx::Rect(left_inset, GetTopInset(restored),
                   std::max(0, width() - left_inset - GetTabStripRightInset()),
                   tabstrip_minimum_size.height());
}

gfx::Rect BrowserFrameViewChromeOS::GetBoundsForWebAppFrameToolbar(
    const gfx::Size& toolbar_preferred_size) const {
  const int x = GetToolbarLeftInset();
  const int available_width = caption_button_container_->x() - x;
  int painted_height = GetTopInset(false);
  if (browser_view()->GetTabStripVisible()) {
    painted_height += browser_view()->GetTabStripHeight();
  }
  return gfx::Rect(x, 0, std::max(0, available_width), painted_height);
}

bool BrowserFrameViewChromeOS::ShouldShowWebAppFrameToolbar() const {
  if (!GetShowCaptionButtons()) {
    return false;
  }

  if (browser_view()->browser()->is_type_app_popup() &&
      !browser_view()->AppUsesWindowControlsOverlay() &&
      !browser_view()->AppUsesBorderlessMode()) {
    return false;
  }

  return true;
}

int BrowserFrameViewChromeOS::GetTopInset(bool restored) const {
  // TODO(estade): why do callsites in this class hardcode false for |restored|?

  if (!GetShouldPaint()) {
    // When immersive fullscreen unrevealed, tabstrip is offscreen with normal
    // tabstrip bounds, the top inset should reach this topmost edge.
    const auto* const immersive_controller =
        ImmersiveModeController::From(browser_view()->browser());
    if (immersive_controller->IsEnabled() &&
        !immersive_controller->IsRevealed()) {
      return (-1) * browser_view()->GetTabStripHeight();
    }

    // The header isn't painted for restored popup/app windows in overview mode,
    // but the inset is still calculated below, so the overview code can align
    // the window content with a fake header.
    if (!GetOverviewMode() || browser_widget()->IsFullscreen() ||
        browser_view()->GetTabStripVisible() ||
        browser_view()->webui_tab_strip()) {
      return 0;
    }
  }

  if (browser_view()->GetTabStripVisible()) {
    return 0;
  }

  Browser* browser = browser_view()->browser();

  int header_height = frame_header_ ? frame_header_->GetHeaderHeight() : 0;
  const gfx::Size toolbar_size =
      browser_view()->GetWebAppFrameToolbarPreferredSize();
  if (!toolbar_size.IsEmpty()) {
    header_height = std::max(header_height, toolbar_size.height());
  }

  return UsePackagedAppHeaderStyle(browser)
             ? header_height
             : caption_button_container_->bounds().bottom();
}

void BrowserFrameViewChromeOS::UpdateThrobber(bool running) {
  if (window_icon_) {
    window_icon_->Update();
  }
}

SkColor BrowserFrameViewChromeOS::GetCaptionColor(
    BrowserFrameActiveState active_state) const {
  // Web apps apply a theme color if specified by the extension/manifest.
  std::optional<SkColor> frame_theme_color =
      browser_view()->browser()->app_controller()->GetThemeColor();
  const SkColor frame_color =
      frame_theme_color.value_or(GetFrameColor(active_state));
  const SkColor active_caption_color =
      views::FrameCaptionButton::GetButtonColor(frame_color);

  if (ShouldPaintAsActiveForState(active_state)) {
    return active_caption_color;
  }

  const float inactive_alpha_ratio =
      views::FrameCaptionButton::GetInactiveButtonColorAlphaRatio();
  return SkColorSetA(active_caption_color,
                     inactive_alpha_ratio * SK_AlphaOPAQUE);
}

SkColor BrowserFrameViewChromeOS::GetFrameColor(
    BrowserFrameActiveState active_state) const {
  if (!UsePackagedAppHeaderStyle(browser_view()->browser())) {
    return BrowserFrameView::GetFrameColor(active_state);
  }

  std::optional<SkColor> color;
  if (browser_view()->GetIsWebAppType()) {
    color = browser_view()->browser()->app_controller()->GetThemeColor();
  }

  SkColor fallback_color = chromeos::kDefaultFrameColor;

  if (GetWidget()) {
    // TODO(skau): Migrate to ColorProvider.
    fallback_color = cros_styles::ResolveColor(
        cros_styles::ColorName::kBgColor,
        GetNativeTheme()->preferred_color_scheme() ==
            ui::NativeTheme::PreferredColorScheme::kDark);
  }

  return color.value_or(fallback_color);
}

void BrowserFrameViewChromeOS::UpdateMinimumSize() {
  gfx::Size current_min_size = GetMinimumSize();
  if (last_minimum_size_ == current_min_size) {
    return;
  }

  last_minimum_size_ = current_min_size;
  GetWidget()->OnSizeConstraintsChanged();
}

gfx::Rect BrowserFrameViewChromeOS::GetBoundsForClientView() const {
  // The ClientView must be flush with the top edge of the widget so that the
  // web contents can take up the entire screen in immersive fullscreen (with
  // or without the top-of-window views revealed). When in immersive fullscreen
  // and the top-of-window views are revealed, the TopContainerView paints the
  // window header by redirecting paints from its background to
  // BrowserFrameViewChromeOS.
  return bounds();
}

gfx::Rect BrowserFrameViewChromeOS::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  const int top_inset = GetTopInset(false);
  return gfx::Rect(client_bounds.x(),
                   std::max(0, client_bounds.y() - top_inset),
                   client_bounds.width(), client_bounds.height() + top_inset);
}

int BrowserFrameViewChromeOS::NonClientHitTest(const gfx::Point& point) {
  int hit_test = chromeos::FrameBorderNonClientHitTest(this, point);

  // When the window is restored (and not in tablet split-view mode) we want a
  // large click target above the tabs to drag the window, so redirect clicks in
  // the tab's shadow to caption.
  if (hit_test == HTCLIENT && !browser_widget()->IsMaximized() &&
      !browser_widget()->IsFullscreen() &&
      !display::Screen::Get()->InTabletMode()) {
    // TODO(crbug.com/40768579): Tab Strip hit calculation and bounds logic
    // should reside in the TabStrip class.
    gfx::Point client_point(point);
    View::ConvertPointToTarget(this, browser_widget()->client_view(),
                               &client_point);
    gfx::Rect tabstrip_shadow_bounds(
        browser_view()
            ->tab_strip_view()
            ->GetViewByElementId(kTabStripElementId)
            ->bounds());
    constexpr int kTabShadowHeight = 4;
    tabstrip_shadow_bounds.set_height(kTabShadowHeight);
    if (tabstrip_shadow_bounds.Contains(client_point)) {
      return HTCAPTION;
    }
  }

  return hit_test;
}

void BrowserFrameViewChromeOS::ResetWindowControls() {
  BrowserFrameView::ResetWindowControls();
  caption_button_container_->SetVisible(GetShowCaptionButtons());
  caption_button_container_->ResetWindowControls();
}

void BrowserFrameViewChromeOS::WindowControlsOverlayEnabledChanged() {
  bool enabled = browser_view()->IsWindowControlsOverlayEnabled();
  caption_button_container_->OnWindowControlsOverlayEnabledChanged(
      enabled, GetFrameHeaderColor(browser_view()->IsActive()));
}

void BrowserFrameViewChromeOS::UpdateWindowIcon() {
  if (window_icon_) {
    window_icon_->SchedulePaint();
  }
}

void BrowserFrameViewChromeOS::UpdateWindowTitle() {
  if (!browser_widget()->IsFullscreen() && frame_header_) {
    frame_header_->SchedulePaintForTitle();
  }

  browser_widget()->GetNativeWindow()->SetProperty(
      chromeos::kWindowOverviewTitleKey,
      browser_view()->browser()->GetWindowTitleForCurrentTab(
          /*include_app_name=*/false));
}

void BrowserFrameViewChromeOS::SizeConstraintsChanged() {}

void BrowserFrameViewChromeOS::OnPaint(gfx::Canvas* canvas) {
  if (!GetShouldPaint()) {
    return;
  }

  if (frame_header_) {
    frame_header_->PaintHeader(canvas);
  }
}

void BrowserFrameViewChromeOS::UpdateBorderlessModeEnabled() {
  caption_button_container_->UpdateBorderlessModeEnabled(
      browser_view()->IsBorderlessModeEnabled());
}

bool BrowserFrameViewChromeOS::AppIsPwaWithBorderlessDisplayMode() const {
  return browser_view()->GetIsWebAppType() &&
         browser_view()->AppUsesBorderlessMode();
}

void BrowserFrameViewChromeOS::Layout(PassKey) {
  // The header must be laid out before computing |painted_height| because the
  // computation of |painted_height| for app and popup windows depends on the
  // position of the window controls.
  if (frame_header_) {
    frame_header_->LayoutHeader();
  }

  int painted_height = GetTopInset(false);
  if (browser_view()->GetTabStripVisible()) {
    painted_height += browser_view()->GetTabStripHeight();
  }

  if (frame_header_) {
    frame_header_->SetHeaderHeightForPainting(painted_height);
  }

  if (profile_indicator_icon_) {
    LayoutProfileIndicator();
  }

  if (AppIsPwaWithBorderlessDisplayMode()) {
    UpdateBorderlessModeEnabled();
  }

  LayoutSuperclass<BrowserFrameView>(this);
  UpdateTopViewInset();

  if (frame_header_) {
    // The top right corner must be occupied by a caption button for easy mouse
    // access. This check is agnostic to RTL layout.
    DCHECK_EQ(caption_button_container_->y(), 0);
    DCHECK_EQ(caption_button_container_->bounds().right(), width());
  }
}

gfx::Size BrowserFrameViewChromeOS::GetMinimumSize() const {
  // System web apps (e.g. Settings) may have a fixed minimum size.
  Browser* browser = browser_view()->browser();
  if (ash::IsSystemWebApp(browser)) {
    gfx::Size minimum_size = ash::GetSystemWebAppMinimumWindowSize(browser);
    if (!minimum_size.IsEmpty()) {
      return minimum_size;
    }
  }

  // The minimum size of a borderless window is only limited by the window's
  // `highlight_border_overlay_`.
  if (browser_view()->IsBorderlessModeEnabled()) {
    // `CalculateImageSourceSize()` returns the minimum size needed to draw the
    // highlight border, which in turn is the minimum size of a borderless
    // window.
    return highlight_border_overlay_->CalculateImageSourceSize();
  }

  gfx::Size min_client_view_size(
      browser_widget()->client_view()->GetMinimumSize());
  const int min_frame_width =
      frame_header_ ? frame_header_->GetMinimumHeaderWidth() : 0;
  int min_width = std::max(min_frame_width, min_client_view_size.width());
  if (browser_view()->GetTabStripVisible()) {
    // Ensure that the minimum width is enough to hold a minimum width tab strip
    // at its usual insets.
    const int min_tabstrip_width =
        browser_view()->tab_strip_view()->GetMinimumSize().width();
    min_width =
        std::max(min_width, min_tabstrip_width + GetTabStripLeftInset() +
                                GetTabStripRightInset());
  }

  int min_height = min_client_view_size.height();
  if (browser_view()->IsWindowControlsOverlayEnabled()) {
    // Ensure that the minimum height is at least the height of the caption
    // button container, which contains the WCO toggle and other windowing
    // controls.
    min_height = min_height + caption_button_container_->size().height();
  }

  // Include bottom rounded corners region. See b:294588040.
  aura::Window* window = GetWidget()->GetNativeWindow();
  const gfx::RoundedCornersF window_radii =
      ash::WindowState::Get(window)->GetWindowRoundedCorners();
  CHECK_EQ(window_radii.lower_left(), window_radii.lower_right());

  min_height = min_height + window_radii.lower_left();

  return gfx::Size(min_width, min_height);
}

void BrowserFrameViewChromeOS::OnThemeChanged() {
  OnUpdateFrameColor();
  caption_button_container_->OnWindowControlsOverlayEnabledChanged(
      browser_view()->IsWindowControlsOverlayEnabled(),
      GetFrameHeaderColor(browser_view()->IsActive()));
  BrowserFrameView::OnThemeChanged();
  MaybeAnimateThemeChanged();
}

void BrowserFrameViewChromeOS::ChildPreferredSizeChanged(views::View* child) {
  if (browser_view()->initialized()) {
    InvalidateLayout();
    browser_widget()->GetRootView()->DeprecatedLayoutImmediately();
  }
}

bool BrowserFrameViewChromeOS::DoesIntersectRect(const views::View* target,
                                                 const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  if (!views::ViewTargeterDelegate::DoesIntersectRect(this, rect)) {
    // |rect| is outside the frame's bounds.
    return false;
  }

  // In immersive mode, the caption buttons container is reparented to the
  // TopContainerView and hence |rect| should not be claimed here.  See
  // BrowserFrameViewChromeOS::OnImmersiveRevealStarted().
  const bool should_leave_to_top_container =
      ImmersiveModeController::From(browser_view()->browser())->IsRevealed();

  return !should_leave_to_top_container;
}

views::View::Views BrowserFrameViewChromeOS::GetChildrenInZOrder() {
  if (ShouldDrawFrameHeader(browser_view()->browser()) && frame_header_) {
    return frame_header_->GetAdjustedChildrenInZOrder(this);
  }

  return BrowserFrameView::GetChildrenInZOrder();
}

SkColor BrowserFrameViewChromeOS::GetTitleColor() {
  return GetColorProvider()->GetColor(kColorCaptionForeground);
}

SkColor BrowserFrameViewChromeOS::GetFrameHeaderColor(bool active) {
  return GetFrameColor(active ? BrowserFrameActiveState::kActive
                              : BrowserFrameActiveState::kInactive);
}

gfx::ImageSkia BrowserFrameViewChromeOS::GetFrameHeaderImage(bool active) {
  return GetFrameImage(active ? BrowserFrameActiveState::kActive
                              : BrowserFrameActiveState::kInactive);
}

int BrowserFrameViewChromeOS::GetFrameHeaderImageYInset() {
  return browser_view()->GetThemeOffsetFromBrowserView().y();
}

gfx::ImageSkia BrowserFrameViewChromeOS::GetFrameHeaderOverlayImage(
    bool active) {
  return GetFrameOverlayImage(active ? BrowserFrameActiveState::kActive
                                     : BrowserFrameActiveState::kInactive);
}

void BrowserFrameViewChromeOS::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kInTabletMode:
      OnTabletModeToggled(true);
      return;
    case display::TabletState::kInClamshellMode:
      OnTabletModeToggled(false);
      return;
    case display::TabletState::kEnteringTabletMode:
    case display::TabletState::kExitingTabletMode:
      break;
  }
}

void BrowserFrameViewChromeOS::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  // When the display is rotated, the frame header may have invalid snap icons.
  // For example, rotating from landscape display to portrait display layout
  // should update snap icons from left/right arrows to upward/downward arrows
  // for top and bottom snaps.
  if ((changed_metrics & DISPLAY_METRIC_ROTATION) && frame_header_) {
    frame_header_->InvalidateLayout();
  }
}

void BrowserFrameViewChromeOS::OnTabletModeToggled(bool enabled) {
  if (!enabled &&
      ImmersiveModeController::From(browser_view()->browser())->IsRevealed()) {
    // Before updating the caption buttons state below (which triggers a
    // relayout), we want to move the caption buttons from the
    // TopContainerView back to this view.
    OnImmersiveRevealEnded();
  }

  const bool should_show_caption_buttons = GetShowCaptionButtons();
  caption_button_container_->SetVisible(should_show_caption_buttons);
  caption_button_container_->UpdateCaptionButtonState(true /*=animate*/);

  auto* const immersive_mode_controller =
      ImmersiveModeController::From(browser_view()->browser());
  const bool was_immersive = immersive_mode_controller->IsEnabled();

  // Set the immersive mode to what it should be because an immersive mode may
  // be used in non fullscreen state in tablet mode.
  immersive_mode_controller->SetEnabled(ShouldEnableImmersiveModeController());

  // Do not relayout if immersive mode hasn't changed because the non client
  // frame area will not change.
  if (was_immersive == immersive_mode_controller->IsEnabled()) {
    return;
  }

  InvalidateLayout();
  // Can be null in tests.
  if (browser_widget()->client_view()) {
    browser_widget()->client_view()->InvalidateLayout();
  }
  if (browser_widget()->GetRootView()) {
    browser_widget()->GetRootView()->DeprecatedLayoutImmediately();
  }
}

bool BrowserFrameViewChromeOS::ShouldTabIconViewAnimate() const {
  // Web apps use their app icon and shouldn't show a throbber.
  if (browser_view()->GetIsWebAppType()) {
    return false;
  }

  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to null check the selected
  // WebContents because in this condition there is not yet a selected tab.
  content::WebContents* current_tab = browser_view()->GetActiveWebContents();
  return current_tab && current_tab->ShouldShowLoadingUI();
}

ui::ImageModel BrowserFrameViewChromeOS::GetFaviconForTabIconView() {
  views::WidgetDelegate* delegate = browser_widget()->widget_delegate();
  return delegate ? delegate->GetWindowIcon() : ui::ImageModel();
}

void BrowserFrameViewChromeOS::OnWindowDestroying(aura::Window* window) {
  DCHECK(window_observation_.IsObserving());
  window_observation_.Reset();
  display_observer_.reset();
}

void BrowserFrameViewChromeOS::OnWindowPropertyChanged(aura::Window* window,
                                                       const void* key,
                                                       intptr_t old) {
  // ChromeOS has rounded windows for certain window states. If these states
  // changes, we need to update the rounded corners of the frame associate with
  // the `window`accordingly.
  if (key == chromeos::kWindowHasRoundedCornersKey) {
    UpdateWindowRoundedCorners();
  }

  if (key == aura::client::kShowStateKey) {
    bool enter_fullscreen = window->GetProperty(aura::client::kShowStateKey) ==
                            ui::mojom::WindowShowState::kFullscreen;
    bool exit_fullscreen = static_cast<ui::mojom::WindowShowState>(old) ==
                           ui::mojom::WindowShowState::kFullscreen;

    // May have to hide caption buttons while in fullscreen mode, or show them
    // when exiting fullscreen.
    if (enter_fullscreen || exit_fullscreen) {
      ResetWindowControls();
    }

    // The client view (in particular the tab strip) has different layout in
    // restored vs. maximized/fullscreen. Invalidate the layout because the
    // window bounds may not have changed. https://crbug.com/1342414
    if (browser_widget()->client_view()) {
      browser_widget()->client_view()->InvalidateLayout();
    }
  }

  if (key == chromeos::kWindowStateTypeKey) {
    // Update window controls when window state changes as whether or not these
    // are shown can depend on the window state (e.g. hiding the caption buttons
    // in non-immersive full screen mode, see crbug.com/1336470).
    ResetWindowControls();

    // Update the window controls if we are entering or exiting float state.
    const bool is_floated = IsFloated();
    const bool was_floated = static_cast<chromeos::WindowStateType>(old) ==
                             chromeos::WindowStateType::kFloated;

    if (frame_header_ && (is_floated != was_floated)) {
      frame_header_->OnFloatStateChanged();
    }

    const bool is_fullscreen = browser_widget()->IsFullscreen();
    const bool was_fullscreen = chromeos::IsFullscreenOrPinnedWindowStateType(
        static_cast<chromeos::WindowStateType>(old));
    // Additionally updates immersive mode when the state is transitioning
    // between non fullscreen states such as maximzied, snapped or float. When
    // switching to or from the fullscreen states, or the state change between
    // fullscreen states (fullscreen <> pinneed), the immersive mode is updated
    // in `BrowserView::FullscreenStateChanged`.
    if (!is_fullscreen && !was_fullscreen) {
      ImmersiveModeController::From(browser_view()->browser())
          ->SetEnabled(ShouldEnableImmersiveModeController());
    }

    return;
  }

  if (key == chromeos::kIsShowingInOverviewKey) {
    OnAddedToOrRemovedFromOverview();
    return;
  }

  if (!frame_header_) {
    return;
  }

  if (key == aura::client::kShowStateKey) {
    frame_header_->OnShowStateChanged(
        window->GetProperty(aura::client::kShowStateKey));
  } else if (key == chromeos::kFrameRestoreLookKey) {
    frame_header_->view()->InvalidateLayout();
  }
}

void BrowserFrameViewChromeOS::OnImmersiveRevealStarted() {
  ResetWindowControls();
  // The frame caption buttons use ink drop highlights and flood fill effects.
  // They make those buttons paint_to_layer. On immersive mode, the browser's
  // TopContainerView is also converted to paint_to_layer (see
  // ImmersiveModeControllerAsh::OnImmersiveRevealStarted()). In this mode, the
  // TopContainerView is responsible for painting this
  // BrowserFrameViewChromeOS (see TopContainerView::PaintChildren()).
  // However, BrowserFrameViewChromeOS is a sibling of TopContainerView
  // not a child. As a result, when the frame caption buttons are set to
  // paint_to_layer as a result of an ink drop effect, they will disappear.
  // https://crbug.com/840242. To fix this, we'll make the caption buttons
  // temporarily children of the TopContainerView while they're all painting to
  // their layers.
  auto* container = browser_view()->top_container();
  container->AddChildViewAt(caption_button_container_.get(), 0);

  container->DeprecatedLayoutImmediately();
}

void BrowserFrameViewChromeOS::OnImmersiveRevealEnded() {
  ResetWindowControls();
  AddChildViewAt(caption_button_container_.get(), 0);

  DeprecatedLayoutImmediately();
}

void BrowserFrameViewChromeOS::OnImmersiveFullscreenExited() {
  OnImmersiveRevealEnded();
}

void BrowserFrameViewChromeOS::OnAppUpdate(const apps::AppUpdate& update) {
  Browser* browser = browser_view()->browser();

  if (!browser->app_controller() ||
      browser->app_controller()->app_id() != update.AppId() ||
      !caption_button_container_) {
    return;
  }

  caption_button_container_->SetCloseButtonEnabled(
      update.AllowClose().value_or(true));
}

void BrowserFrameViewChromeOS::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observation_.Reset();
}

bool BrowserFrameViewChromeOS::ShouldEnableImmersiveModeController() const {
  // Do not support immersive mode in kiosk.
  if (chromeos::IsKioskSession()) {
    return false;
  }

  if (IsLockedFullscreen() &&
      !GetFrameWindow()->GetProperty(chromeos::kUseImmersiveInTrustedPinned)) {
    return false;
  }
  if (display::Screen::Get()->InTabletMode() &&
      (IsSnapped() || browser_widget()->IsMaximized())) {
    // Snapped or maximized browser windows that doesn't have tabstrip uses
    // immersive frame to hide frame in tablet mode.
    return !browser_view()->GetSupportsTabStrip();
  }

  const auto* fullscreen_controller = browser_view()
                                          ->browser()
                                          ->GetFeatures()
                                          .exclusive_access_manager()
                                          ->fullscreen_controller();
  // For other scnarios, use immersive if the browser is in fullscreen, and it
  // is NOT requested via extension or HTML API `requestFullscreen()`.
  return browser_widget()->IsFullscreen() &&
         !fullscreen_controller->IsExtensionFullscreenOrPending() &&
         fullscreen_controller->IsFullscreenForBrowser();
}

// static
bool BrowserFrameViewChromeOS::ShouldShowAvatarForTesting(
    aura::Window* window) {
  return ShouldShowAvatar(window);
}

bool BrowserFrameViewChromeOS::IsLockedFullscreen() const {
  return ash::WindowState::Get(browser_widget()->GetNativeWindow())
      ->IsLockedFullscreen();
}

void BrowserFrameViewChromeOS::PaintAsActiveChanged() {
  BrowserFrameView::PaintAsActiveChanged();

  UpdateProfileIcons();

  if (frame_header_) {
    frame_header_->SetPaintAsActive(ShouldPaintAsActive());
  }
}

void BrowserFrameViewChromeOS::AddedToWidget() {
  if (highlight_border_overlay_ ||
      !GetWidget()->GetNativeWindow()->GetProperty(
          chromeos::kShouldHaveHighlightBorderOverlay)) {
    return;
  }

  highlight_border_overlay_ = std::make_unique<HighlightBorderOverlay>(
      GetWidget(), std::make_unique<ash::WmHighlightBorderOverlayDelegate>());
}

BrowserFrameViewChromeOS::BoundsAndMargins
BrowserFrameViewChromeOS::GetCaptionButtonBounds() const {
  return BoundsAndMargins{GetShowCaptionButtonsWhenNotInOverview()
                              ? gfx::RectF(caption_button_container_->bounds())
                              : gfx::RectF()};
}

bool BrowserFrameViewChromeOS::GetShowCaptionButtons() const {
  if (GetOverviewMode()) {
    return false;
  }

  return GetShowCaptionButtonsWhenNotInOverview();
}

bool BrowserFrameViewChromeOS::GetShowCaptionButtonsWhenNotInOverview() const {
  // Show the caption buttons if an immersive mode is enabled for trusted pined
  // state. This is to show the three dot menu which is a part of caption button
  // container, rather than showing buttons. Only relevant for non-web browser
  // scenarios.
  if (IsLockedFullscreen() &&
      GetFrameWindow()->GetProperty(chromeos::kUseImmersiveInTrustedPinned)) {
    return true;
  }

  if (GetHideCaptionButtonsForFullscreen()) {
    return false;
  }

  // Show the caption buttons for packaged apps which support immersive mode.
  if (UsePackagedAppHeaderStyle(browser_view()->browser())) {
    return true;
  }

  // Browsers in tablet mode still show their caption buttons in float state,
  // even with the webUI tab strip.
  if (display::Screen::Get()->InTabletMode()) {
    return IsFloated();
  }

  return !UseWebUITabStrip();
}

int BrowserFrameViewChromeOS::GetToolbarLeftInset() const {
  // Include padding on left and right of icon.
  return profile_indicator_icon_
             ? kProfileIndicatorPadding * 2 + profile_indicator_icon_->width()
             : 0;
}

int BrowserFrameViewChromeOS::GetTabStripLeftInset() const {
  // Include padding on left of icon.
  // The tab strip has its own 'padding' to the right of the icon.
  return profile_indicator_icon_
             ? kProfileIndicatorPadding + profile_indicator_icon_->width()
             : 0;
}

int BrowserFrameViewChromeOS::GetTabStripRightInset() const {
  int inset = 0;
  if (GetShowCaptionButtonsWhenNotInOverview()) {
    inset += caption_button_container_->GetPreferredSize().width();
  }
  return inset;
}

bool BrowserFrameViewChromeOS::GetShouldPaint() const {
  // Floated windows show their frame as they need to be dragged or hidden.
  if (IsFloated()) {
    return true;
  }

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  // Normal windows that have a WebUI-based tab strip do not need a browser
  // frame as no tab strip is drawn on top of the browser frame.
  if (UseWebUITabStrip()) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

  // We need to paint when the top-of-window views are revealed in immersive
  // fullscreen.
  auto* const immersive_mode_controller =
      ImmersiveModeController::From(browser_view()->browser());
  if (immersive_mode_controller->IsEnabled()) {
    return immersive_mode_controller->IsRevealed();
  }

  return !browser_widget()->IsFullscreen();
}

void BrowserFrameViewChromeOS::OnAddedToOrRemovedFromOverview() {
  const bool should_show_caption_buttons = GetShowCaptionButtons();
  caption_button_container_->SetVisible(should_show_caption_buttons);
  if (browser_view()->GetIsWebAppType()) {
    // The WebAppFrameToolbarView is part of the BrowserView, so make sure the
    // BrowserView is re-layed out to take into account these changes.
    browser_view()->InvalidateLayout();
  }
}

std::unique_ptr<chromeos::FrameHeader>
BrowserFrameViewChromeOS::CreateFrameHeader() {
  std::unique_ptr<chromeos::FrameHeader> header;
  Browser* browser = browser_view()->browser();
  if (!UsePackagedAppHeaderStyle(browser)) {
    header = std::make_unique<BrowserFrameHeaderChromeOS>(
        browser_widget(), this, this, caption_button_container_);
  } else {
    header = std::make_unique<chromeos::DefaultFrameHeader>(
        browser_widget(), this, caption_button_container_);
  }

  header->SetLeftHeaderView(window_icon_);
  return header;
}

void BrowserFrameViewChromeOS::UpdateTopViewInset() {
  // In immersive fullscreen mode, the top view inset property should be 0.
  const bool immersive =
      ImmersiveModeController::From(browser_view()->browser())->IsEnabled();
  const bool tab_strip_visible = browser_view()->GetTabStripVisible();
  const int inset = (tab_strip_visible || immersive ||
                     (AppIsPwaWithBorderlessDisplayMode() &&
                      browser_view()->IsBorderlessModeEnabled()))
                        ? 0
                        : GetTopInset(/*restored=*/false);
  browser_widget()->GetNativeWindow()->SetProperty(aura::client::kTopViewInset,
                                                   inset);
}

bool BrowserFrameViewChromeOS::GetShowProfileIndicatorIcon() const {
  // We only show the profile indicator for the teleported browser windows
  // between multi-user sessions. Note that you can't teleport an incognito
  // window.
  Browser* browser = browser_view()->browser();
  if (browser->profile()->IsIncognitoProfile()) {
    return false;
  }

  if (browser->is_type_popup()) {
    return false;
  }

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  // TODO(http://crbug.com/1059514): This check shouldn't be necessary.  Provide
  // an appropriate affordance for the profile icon with the webUI tabstrip and
  // remove this block.
  if (!browser_view()->GetTabStripVisible()) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

  return ShouldShowAvatar(browser_view()->GetNativeWindow());
}

void BrowserFrameViewChromeOS::UpdateProfileIcons() {
  View* root_view = browser_widget()->GetRootView();
  if (GetShowProfileIndicatorIcon()) {
    bool needs_layout = !profile_indicator_icon_;
    if (!profile_indicator_icon_) {
      profile_indicator_icon_ =
          AddChildView(std::make_unique<ProfileIndicatorIcon>());
    }

    gfx::Image image(
        GetAvatarImageForContext(browser_view()->browser()->profile()));
    profile_indicator_icon_->SetSize(image.Size());
    profile_indicator_icon_->SetIcon(image);

    if (needs_layout && root_view) {
      // Adding a child does not invalidate the layout.
      InvalidateLayout();
      root_view->DeprecatedLayoutImmediately();
    }
  } else if (profile_indicator_icon_) {
    RemoveChildViewT(std::exchange(profile_indicator_icon_, nullptr));
    if (root_view) {
      root_view->DeprecatedLayoutImmediately();
    }
  }
}

void BrowserFrameViewChromeOS::UpdateWindowRoundedCorners() {
  DCHECK(GetWidget());

  aura::Window* window = GetWidget()->GetNativeWindow();
  auto* window_state = ash::WindowState::Get(window);

  // For certain windows, we do not window state associated with them. (See
  // `ash::WindowState::Get()` for details)
  if (!window_state) {
    return;
  }

  const gfx::RoundedCornersF window_radii =
      window_state->GetWindowRoundedCorners();

  if (frame_header_) {
    CHECK_EQ(window_radii.upper_left(), window_radii.upper_right());
    frame_header_->SetHeaderCornerRadius(window_radii.upper_left());
  }

  if (browser_view()->IsWindowControlsOverlayEnabled()) {
    // With window controls overlay enabled, the caption_button_container is
    // drawn above the client view. The container has a background that extends
    // over the curvature of the top-right corner, requiring its rounding.
    caption_button_container_->layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(0, window_radii.upper_right(), 0, 0));
    caption_button_container_->layer()->SetIsFastRoundedCorner(/*enable=*/true);
  }

  GetWidget()->client_view()->UpdateWindowRoundedCorners(window_radii);
}

void BrowserFrameViewChromeOS::LayoutProfileIndicator() {
  DCHECK(profile_indicator_icon_);
  const int frame_height =
      GetTopInset(false) + browser_view()->GetTabStripHeight();
  profile_indicator_icon_->SetPosition(
      gfx::Point(kProfileIndicatorPadding,
                 (frame_height - profile_indicator_icon_->height()) / 2));
  profile_indicator_icon_->SetVisible(true);

  // The layout size is set along with the image.
  DCHECK_LE(profile_indicator_icon_->height(), frame_height);
}

bool BrowserFrameViewChromeOS::GetOverviewMode() const {
  return GetFrameWindow()->GetProperty(chromeos::kIsShowingInOverviewKey);
}

bool BrowserFrameViewChromeOS::GetHideCaptionButtonsForFullscreen() const {
  if (!browser_widget()->IsFullscreen()) {
    return false;
  }

  auto* const immersive_controller =
      ImmersiveModeController::From(browser_view()->browser());

  // In fullscreen view, but not in immersive mode. Hide the caption buttons.
  if (!immersive_controller || !immersive_controller->IsEnabled()) {
    return true;
  }

  return immersive_controller->ShouldHideTopViews();
}

void BrowserFrameViewChromeOS::OnUpdateFrameColor() {
  aura::Window* window = browser_widget()->GetNativeWindow();
  window->SetProperty(chromeos::kFrameActiveColorKey,
                      GetFrameColor(BrowserFrameActiveState::kActive));
  window->SetProperty(chromeos::kFrameInactiveColorKey,
                      GetFrameColor(BrowserFrameActiveState::kInactive));

  if (frame_header_) {
    frame_header_->UpdateFrameColors();
  }
}

void BrowserFrameViewChromeOS::MaybeAnimateThemeChanged() {
  if (!browser_view()) {
    return;
  }

  Browser* browser = browser_view()->browser();

  // Theme change events are only animated for system web apps which explicitly
  // request the behavior.
  bool animate_theme_change_for_swa =
      ash::IsSystemWebApp(browser) &&
      browser->app_controller()->system_app()->ShouldAnimateThemeChanges();
  if (!animate_theme_change_for_swa) {
    return;
  }

  views::WebView* web_view = browser_view()->contents_web_view();
  ui::Layer* layer = GetNativeViewLayer(web_view);
  content::RenderWidgetHost* render_widget_host = GetRenderWidgetHost(web_view);
  if (!layer || !render_widget_host) {
    return;
  }

  // Immediately hide the layer associated with the `contents_web_view()` native
  // view so that repainting of the web contents (which is janky) is hidden from
  // user. Note that opacity is set just above `0.f` to pass a DCHECK that
  // exists in `aura::Window` that might otherwise be tripped when changing
  // window visibility (see https://crbug.com/351553).
  layer->SetOpacity(std::nextafter(0.f, 1.f));

  // Cache a callback to invoke to animate the layer back in. Note that because
  // this is a cancelable callback, any previously created callback will be
  // cancelled.
  theme_changed_animation_callback_.Reset(base::BindOnce(
      [](const base::WeakPtr<BrowserFrameViewChromeOS>& self,
         base::TimeTicks theme_changed_time, bool success) {
        if (!self || !self->browser_view()) {
          return;
        }

        views::WebView* web_view = self->browser_view()->contents_web_view();
        ui::Layer* layer = GetNativeViewLayer(web_view);
        if (!layer) {
          return;
        }

        // Delay animating the layer back in at least until the
        // `chromeos::DefaultFrameHeader` has had a chance to complete its own
        // color change animation.
        const base::TimeDelta offset =
            chromeos::kDefaultFrameColorChangeAnimationDuration -
            (base::TimeTicks::Now() - theme_changed_time);

        views::AnimationBuilder()
            .SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                       IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
            .Once()
            .Offset(std::max(offset, base::TimeDelta()))
            .SetDuration(chromeos::kDefaultFrameColorChangeAnimationDuration)
            .SetOpacity(layer, 1.f);
      },
      weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));

  // Animate the layer back in only after a round trip through the renderer and
  // compositor pipelines. This should ensure that the web contents has finished
  // repainting theme changes.
  render_widget_host->InsertVisualStateCallback(
      theme_changed_animation_callback_.callback());
}

bool BrowserFrameViewChromeOS::IsFloated() const {
  return ash::WindowState::Get(browser_widget()->GetNativeWindow())
      ->IsFloated();
}

bool BrowserFrameViewChromeOS::IsSnapped() const {
  return ash::WindowState::Get(browser_widget()->GetNativeWindow())
      ->IsSnapped();
}

bool BrowserFrameViewChromeOS::UseWebUITabStrip() const {
  return WebUITabStripContainerView::UseTouchableTabStrip(
             browser_view()->browser()) &&
         browser_view()->GetSupportsTabStrip();
}

const aura::Window* BrowserFrameViewChromeOS::GetFrameWindow() const {
  return const_cast<BrowserFrameViewChromeOS*>(this)->GetFrameWindow();
}

aura::Window* BrowserFrameViewChromeOS::GetFrameWindow() {
  return browser_widget()->GetNativeWindow();
}

BEGIN_METADATA(BrowserFrameViewChromeOS)
ADD_READONLY_PROPERTY_METADATA(bool, ShowCaptionButtons)
ADD_READONLY_PROPERTY_METADATA(bool, ShowCaptionButtonsWhenNotInOverview)
ADD_READONLY_PROPERTY_METADATA(int, ToolbarLeftInset)
ADD_READONLY_PROPERTY_METADATA(int, TabStripLeftInset)
ADD_READONLY_PROPERTY_METADATA(int, TabStripRightInset)
ADD_READONLY_PROPERTY_METADATA(bool, ShouldPaint)
ADD_READONLY_PROPERTY_METADATA(bool, ShowProfileIndicatorIcon)
ADD_READONLY_PROPERTY_METADATA(bool, OverviewMode)

END_METADATA
