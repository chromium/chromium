// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_chromeos.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/profiles/profile_indicator_icon.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/wm/window_util.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/session/session_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/ui/frame/interior_resize_handler_targeter.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

DEFINE_UI_CLASS_PROPERTY_TYPE(BrowserNonClientFrameViewChromeOS*)

namespace {

// The indicator for teleported windows has 8 DIPs before and below it.
constexpr int kProfileIndicatorPadding = 8;

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns the layer for the specified `web_view`'s native view.
ui::Layer* GetNativeViewLayer(views::WebView* web_view) {
  if (web_view) {
    if (views::NativeViewHost* holder = web_view->holder(); holder) {
      if (aura::Window* native_view = holder->native_view(); native_view)
        return native_view->layer();
    }
  }
  return nullptr;
}

// Returns the render widget host for the specified `web_view`.
content::RenderWidgetHost* GetRenderWidgetHost(views::WebView* web_view) {
  if (web_view) {
    if (auto* web_contents = web_view->GetWebContents(); web_contents) {
      if (auto* rvh = web_contents->GetRenderViewHost(); rvh)
        return rvh->GetWidget();
    }
  }
  return nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

DEFINE_UI_CLASS_PROPERTY_KEY(BrowserNonClientFrameViewChromeOS*,
                             kBrowserNonClientFrameViewChromeOSKey,
                             nullptr)

// Returns true if the header should be painted so that it looks the same as
// the header used for packaged apps.
bool UsePackagedAppHeaderStyle(const Browser* browser) {
  if (browser->is_type_normal() ||
      (browser->is_type_popup() && !browser->is_trusted_source())) {
    return false;
  }

  return !browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP);
}

}  // namespace

BrowserNonClientFrameViewChromeOS::BrowserNonClientFrameViewChromeOS(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::window_util::InstallResizeHandleWindowTargeterForWindow(
      frame->GetNativeWindow());
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  frame->GetNativeWindow()->SetEventTargeter(
      std::make_unique<chromeos::InteriorResizeHandleTargeter>(
          base::BindRepeating([](const aura::Window* window) {
            return window->GetProperty(chromeos::kWindowStateTypeKey);
          })));
#endif

  // TODO: b/330360595 - Confirm if this is needed in Lacros.
  aura::Window* frame_window = frame->GetNativeWindow();
  frame_window->SetProperty(kBrowserNonClientFrameViewChromeOSKey, this);

  GetViewAccessibility().SetRole(ax::mojom::Role::kTitleBar);
}

BrowserNonClientFrameViewChromeOS::~BrowserNonClientFrameViewChromeOS() {
  ImmersiveModeController* immersive_controller =
      browser_view()->immersive_mode_controller();
  if (immersive_controller)
    immersive_controller->RemoveObserver(this);

  if (profile_indicator_icon_) {
    RemoveChildViewT(std::exchange(profile_indicator_icon_, nullptr));
  }
}

BrowserNonClientFrameViewChromeOS* BrowserNonClientFrameViewChromeOS::Get(
    aura::Window* window) {
  return window->GetProperty(kBrowserNonClientFrameViewChromeOSKey);
}

void BrowserNonClientFrameViewChromeOS::Init() {
  Browser* browser = browser_view()->browser();

  const bool is_close_button_enabled =
      !(browser->app_controller() &&
        browser->app_controller()->IsPreventCloseEnabled());

  caption_button_container_ =
      AddChildView(std::make_unique<chromeos::FrameCaptionButtonContainerView>(
          frame(), is_close_button_enabled));

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
    frame()->GetNativeWindow()->SetProperty(
        chromeos::kBlockedForAssistantSnapshotKey, true);
  }

  display_observer_.emplace(this);

  if (frame()->ShouldDrawFrameHeader()) {
    frame_header_ = CreateFrameHeader();
  }

  if (AppIsPwaWithBorderlessDisplayMode()) {
    UpdateBorderlessModeEnabled();
  }

  browser_view()->immersive_mode_controller()->AddObserver(this);
}

gfx::Rect BrowserNonClientFrameViewChromeOS::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size) const {
  const int left_inset = GetTabStripLeftInset();
  const bool restored = !frame()->IsMaximized() && !frame()->IsFullscreen();
  return gfx::Rect(left_inset, GetTopInset(restored),
                   std::max(0, width() - left_inset - GetTabStripRightInset()),
                   tabstrip_minimum_size.height());
}

gfx::Rect BrowserNonClientFrameViewChromeOS::GetBoundsForWebAppFrameToolbar(
    const gfx::Size& toolbar_preferred_size) const {
  if (!GetShowCaptionButtons()) {
    return gfx::Rect();
  }
  if (browser_view()->browser()->is_type_app_popup() &&
      !browser_view()->AppUsesWindowControlsOverlay() &&
      !browser_view()->AppUsesBorderlessMode()) {
    return gfx::Rect();
  }

  const int x = GetToolbarLeftInset();
  const int available_width = caption_button_container_->x() - x;
  int painted_height = GetTopInset(false);
  if (browser_view()->GetTabStripVisible()) {
    painted_height += browser_view()->tabstrip()->GetPreferredSize().height();
  }
  return gfx::Rect(x, 0, std::max(0, available_width), painted_height);
}

void BrowserNonClientFrameViewChromeOS::LayoutWebAppWindowTitle(
    const gfx::Rect& available_space,
    views::Label& window_title_label) const {
  // No window titles on Chrome OS, so just hide the window title.
  window_title_label.SetVisible(false);
}

int BrowserNonClientFrameViewChromeOS::GetTopInset(bool restored) const {
  // TODO(estade): why do callsites in this class hardcode false for |restored|?

  if (!GetShouldPaint()) {
    // When immersive fullscreen unrevealed, tabstrip is offscreen with normal
    // tapstrip bounds, the top inset should reach this topmost edge.
    const ImmersiveModeController* const immersive_controller =
        browser_view()->immersive_mode_controller();
    if (immersive_controller->IsEnabled() &&
        !immersive_controller->IsRevealed()) {
      return (-1) * browser_view()->GetTabStripHeight();
    }

    // The header isn't painted for restored popup/app windows in overview mode,
    // but the inset is still calculated below, so the overview code can align
    // the window content with a fake header.
    if (!GetOverviewMode() || frame()->IsFullscreen() ||
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

void BrowserNonClientFrameViewChromeOS::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

bool BrowserNonClientFrameViewChromeOS::CanUserExitFullscreen() const {
  return !platform_util::IsBrowserLockedFullscreen(browser_view()->browser());
}

SkColor BrowserNonClientFrameViewChromeOS::GetCaptionColor(
    BrowserFrameActiveState active_state) const {
  // Web apps apply a theme color if specified by the extension/manifest.
  std::optional<SkColor> frame_theme_color =
      browser_view()->browser()->app_controller()->GetThemeColor();
  const SkColor frame_color =
      frame_theme_color.value_or(GetFrameColor(active_state));
  const SkColor active_caption_color =
      views::FrameCaptionButton::GetButtonColor(frame_color);

  if (ShouldPaintAsActive(active_state))
    return active_caption_color;

  const float inactive_alpha_ratio =
      views::FrameCaptionButton::GetInactiveButtonColorAlphaRatio();
  return SkColorSetA(active_caption_color,
                     inactive_alpha_ratio * SK_AlphaOPAQUE);
}

SkColor BrowserNonClientFrameViewChromeOS::GetFrameColor(
    BrowserFrameActiveState active_state) const {
  if (!UsePackagedAppHeaderStyle(browser_view()->browser()))
    return BrowserNonClientFrameView::GetFrameColor(active_state);

  std::optional<SkColor> color;
  if (browser_view()->GetIsWebAppType())
    color = browser_view()->browser()->app_controller()->GetThemeColor();

  SkColor fallback_color = chromeos::kDefaultFrameColor;

  if (GetWidget()) {
    // TODO(skau): Migrate to ColorProvider.
    fallback_color =
        cros_styles::ResolveColor(cros_styles::ColorName::kBgColor,
                                  GetNativeTheme()->ShouldUseDarkColors());
  }

  return color.value_or(fallback_color);
}

void BrowserNonClientFrameViewChromeOS::UpdateMinimumSize() {
  gfx::Size current_min_size = GetMinimumSize();
  if (last_minimum_size_ == current_min_size)
    return;

  last_minimum_size_ = current_min_size;
  GetWidget()->OnSizeConstraintsChanged();
}

gfx::Rect BrowserNonClientFrameViewChromeOS::GetBoundsForClientView() const {
  // The ClientView must be flush with the top edge of the widget so that the
  // web contents can take up the entire screen in immersive fullscreen (with
  // or without the top-of-window views revealed). When in immersive fullscreen
  // and the top-of-window views are revealed, the TopContainerView paints the
  // window header by redirecting paints from its background to
  // BrowserNonClientFrameViewChromeOS.
  return bounds();
}

gfx::Rect BrowserNonClientFrameViewChromeOS::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  const int top_inset = GetTopInset(false);
  return gfx::Rect(client_bounds.x(),
                   std::max(0, client_bounds.y() - top_inset),
                   client_bounds.width(), client_bounds.height() + top_inset);
}

int BrowserNonClientFrameViewChromeOS::NonClientHitTest(
    const gfx::Point& point) {
  int hit_test = chromeos::FrameBorderNonClientHitTest(this, point);

  // When the window is restored (and not in tablet split-view mode) we want a
  // large click target above the tabs to drag the window, so redirect clicks in
  // the tab's shadow to caption.
  if (hit_test == HTCLIENT && !frame()->IsMaximized() &&
      !frame()->IsFullscreen() &&
      !display::Screen::GetScreen()->InTabletMode()) {
    // TODO(crbug.com/40768579): Tab Strip hit calculation and bounds logic
    // should reside in the TabStrip class.
    gfx::Point client_point(point);
    View::ConvertPointToTarget(this, frame()->client_view(), &client_point);
    gfx::Rect tabstrip_shadow_bounds(browser_view()->tabstrip()->bounds());
    constexpr int kTabShadowHeight = 4;
    tabstrip_shadow_bounds.set_height(kTabShadowHeight);
    if (tabstrip_shadow_bounds.Contains(client_point))
      return HTCAPTION;
  }

  return hit_test;
}

void BrowserNonClientFrameViewChromeOS::GetWindowMask(const gfx::Size& size,
                                                      SkPath* window_mask) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The opaque region of surface should be set exactly same as the frame header
  // path in BrowserFrameHeader.
  if (frame()->ShouldDrawFrameHeader())
    *window_mask = frame_header_->GetWindowMaskForFrameHeader(size);
#endif
}

void BrowserNonClientFrameViewChromeOS::ResetWindowControls() {
  BrowserNonClientFrameView::ResetWindowControls();
  caption_button_container_->SetVisible(GetShowCaptionButtons());
  caption_button_container_->ResetWindowControls();
}

void BrowserNonClientFrameViewChromeOS::WindowControlsOverlayEnabledChanged() {
  bool enabled = browser_view()->IsWindowControlsOverlayEnabled();
  caption_button_container_->OnWindowControlsOverlayEnabledChanged(
      enabled, GetFrameHeaderColor(browser_view()->IsActive()));
}

void BrowserNonClientFrameViewChromeOS::UpdateWindowIcon() {
  if (window_icon_)
    window_icon_->SchedulePaint();
}

void BrowserNonClientFrameViewChromeOS::UpdateWindowTitle() {
  if (!frame()->IsFullscreen() && frame_header_)
    frame_header_->SchedulePaintForTitle();

  frame()->GetNativeWindow()->SetProperty(
      chromeos::kWindowOverviewTitleKey,
      browser_view()->browser()->GetWindowTitleForCurrentTab(
          /*include_app_name=*/false));
}

void BrowserNonClientFrameViewChromeOS::SizeConstraintsChanged() {}

void BrowserNonClientFrameViewChromeOS::OnPaint(gfx::Canvas* canvas) {
  if (!GetShouldPaint())
    return;

  if (frame_header_)
    frame_header_->PaintHeader(canvas);
}

void BrowserNonClientFrameViewChromeOS::UpdateBorderlessModeEnabled() {
  caption_button_container_->UpdateBorderlessModeEnabled(
      browser_view()->IsBorderlessModeEnabled());
}

bool BrowserNonClientFrameViewChromeOS::AppIsPwaWithBorderlessDisplayMode()
    const {
  return browser_view()->GetIsWebAppType() &&
         browser_view()->AppUsesBorderlessMode();
}

void BrowserNonClientFrameViewChromeOS::Layout(PassKey) {
  // The header must be laid out before computing |painted_height| because the
  // computation of |painted_height| for app and popup windows depends on the
  // position of the window controls.
  if (frame_header_)
    frame_header_->LayoutHeader();

  int painted_height = GetTopInset(false);
  if (browser_view()->GetTabStripVisible())
    painted_height += browser_view()->tabstrip()->GetPreferredSize().height();

  if (frame_header_)
    frame_header_->SetHeaderHeightForPainting(painted_height);

  if (profile_indicator_icon_) {
    LayoutProfileIndicator();
  }

  if (AppIsPwaWithBorderlessDisplayMode()) {
    UpdateBorderlessModeEnabled();
  }

  LayoutSuperclass<BrowserNonClientFrameView>(this);
  UpdateTopViewInset();

  if (frame_header_) {
    // The top right corner must be occupied by a caption button for easy mouse
    // access. This check is agnostic to RTL layout.
    DCHECK_EQ(caption_button_container_->y(), 0);
    DCHECK_EQ(caption_button_container_->bounds().right(), width());
  }
}

gfx::Size BrowserNonClientFrameViewChromeOS::GetMinimumSize() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // System web apps (e.g. Settings) may have a fixed minimum size.
  Browser* browser = browser_view()->browser();
  if (ash::IsSystemWebApp(browser)) {
    gfx::Size minimum_size = ash::GetSystemWebAppMinimumWindowSize(browser);
    if (!minimum_size.IsEmpty())
      return minimum_size;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // The minimum size of a borderless window is only limited by the window's
  // `highlight_border_overlay_`.
  if (browser_view()->IsBorderlessModeEnabled()) {
    // `CalculateImageSourceSize()` returns the minimum size needed to draw the
    // highlight border, which in turn is the minimum size of a borderless
    // window.
    return highlight_border_overlay_->CalculateImageSourceSize();
  }

  gfx::Size min_client_view_size(frame()->client_view()->GetMinimumSize());
  const int min_frame_width =
      frame_header_ ? frame_header_->GetMinimumHeaderWidth() : 0;
  int min_width = std::max(min_frame_width, min_client_view_size.width());
  if (browser_view()->GetTabStripVisible()) {
    // Ensure that the minimum width is enough to hold a minimum width tab strip
    // at its usual insets.
    const int min_tabstrip_width =
        browser_view()->tab_strip_region_view()->GetMinimumSize().width();
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

  const int window_corner_radius = frame()->GetNativeWindow()->GetProperty(
      aura::client::kWindowCornerRadiusKey);
  if (chromeos::features::IsRoundedWindowsEnabled() &&
      window_corner_radius > 0) {
    // Include bottom rounded corners region. See b/294588040.
    min_height = min_height + window_corner_radius;
  }

  return gfx::Size(min_width, min_height);
}

void BrowserNonClientFrameViewChromeOS::OnThemeChanged() {
  OnUpdateFrameColor();
  caption_button_container_->OnWindowControlsOverlayEnabledChanged(
      browser_view()->IsWindowControlsOverlayEnabled(),
      GetFrameHeaderColor(browser_view()->IsActive()));
  BrowserNonClientFrameView::OnThemeChanged();
  MaybeAnimateThemeChanged();
}

void BrowserNonClientFrameViewChromeOS::ChildPreferredSizeChanged(
    views::View* child) {
  if (browser_view()->initialized()) {
    InvalidateLayout();
    frame()->GetRootView()->DeprecatedLayoutImmediately();
  }
}

bool BrowserNonClientFrameViewChromeOS::DoesIntersectRect(
    const views::View* target,
    const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  if (!views::ViewTargeterDelegate::DoesIntersectRect(this, rect)) {
    // |rect| is outside the frame's bounds.
    return false;
  }

  bool should_leave_to_top_container = false;
#if BUILDFLAG(IS_CHROMEOS)
  // In immersive mode, the caption buttons container is reparented to the
  // TopContainerView and hence |rect| should not be claimed here.  See
  // BrowserNonClientFrameViewChromeOS::OnImmersiveRevealStarted().
  should_leave_to_top_container =
      browser_view()->immersive_mode_controller()->IsRevealed();
#endif

  return !should_leave_to_top_container;
}

views::View::Views BrowserNonClientFrameViewChromeOS::GetChildrenInZOrder() {
  if (frame()->ShouldDrawFrameHeader() && frame_header_)
    return frame_header_->GetAdjustedChildrenInZOrder(this);

  return BrowserNonClientFrameView::GetChildrenInZOrder();
}

SkColor BrowserNonClientFrameViewChromeOS::GetTitleColor() {
  return GetColorProvider()->GetColor(kColorCaptionForeground);
}

SkColor BrowserNonClientFrameViewChromeOS::GetFrameHeaderColor(bool active) {
  return GetFrameColor(active ? BrowserFrameActiveState::kActive
                              : BrowserFrameActiveState::kInactive);
}

gfx::ImageSkia BrowserNonClientFrameViewChromeOS::GetFrameHeaderImage(
    bool active) {
  return GetFrameImage(active ? BrowserFrameActiveState::kActive
                              : BrowserFrameActiveState::kInactive);
}

int BrowserNonClientFrameViewChromeOS::GetFrameHeaderImageYInset() {
  return browser_view()->GetThemeOffsetFromBrowserView().y();
}

gfx::ImageSkia BrowserNonClientFrameViewChromeOS::GetFrameHeaderOverlayImage(
    bool active) {
  return GetFrameOverlayImage(active ? BrowserFrameActiveState::kActive
                                     : BrowserFrameActiveState::kInactive);
}

void BrowserNonClientFrameViewChromeOS::OnDisplayTabletStateChanged(
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

void BrowserNonClientFrameViewChromeOS::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  // When the display is rotated, the frame header may have invalid snap icons.
  // For example, rotating from landscape display to portrait display layout
  // should update snap icons from left/right arrows to upward/downward arrows
  // for top and bottom snaps.
  if ((changed_metrics & DISPLAY_METRIC_ROTATION) && frame_header_)
    frame_header_->InvalidateLayout();
}

void BrowserNonClientFrameViewChromeOS::OnTabletModeToggled(bool enabled) {
  if (!enabled && browser_view()->immersive_mode_controller()->IsRevealed()) {
    // Before updating the caption buttons state below (which triggers a
    // relayout), we want to move the caption buttons from the
    // TopContainerView back to this view.
    OnImmersiveRevealEnded();
  }

  const bool should_show_caption_buttons = GetShowCaptionButtons();
  caption_button_container_->SetVisible(should_show_caption_buttons);
  caption_button_container_->UpdateCaptionButtonState(true /*=animate*/);

  ImmersiveModeController* immersive_mode_controller =
      browser_view()->immersive_mode_controller();
  ExclusiveAccessManager* exclusive_access_manager =
      browser_view()->browser()->exclusive_access_manager();

  const bool was_immersive = immersive_mode_controller->IsEnabled();
  const bool was_fullscreen =
      exclusive_access_manager->context()->IsFullscreen();

  // If fullscreen mode is not what it should be, toggle fullscreen mode.
  if (ShouldEnableFullscreenMode(enabled) != was_fullscreen) {
    exclusive_access_manager->fullscreen_controller()
        ->ToggleBrowserFullscreenMode();
  }

  // Set immersive mode to what it should be. Note that we need to call this
  // after updating fullscreen mode since it may override immersive mode to not
  // wanted state (e.g. Non TabStrip frame with tablet mode enabled).
  immersive_mode_controller->SetEnabled(
      ShouldEnableImmersiveModeController(enabled));

  // Do not relayout if neither of immersive mode nor fullscreen mode has
  // changed because the non client frame area will not change.
  if (was_immersive == immersive_mode_controller->IsEnabled() &&
      was_fullscreen == exclusive_access_manager->context()->IsFullscreen()) {
    return;
  }

  InvalidateLayout();
  // Can be null in tests.
  if (frame()->client_view())
    frame()->client_view()->InvalidateLayout();
  if (frame()->GetRootView())
    frame()->GetRootView()->DeprecatedLayoutImmediately();
}

bool BrowserNonClientFrameViewChromeOS::ShouldTabIconViewAnimate() const {
  // Web apps use their app icon and shouldn't show a throbber.
  if (browser_view()->GetIsWebAppType())
    return false;

  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to null check the selected
  // WebContents because in this condition there is not yet a selected tab.
  content::WebContents* current_tab = browser_view()->GetActiveWebContents();
  return current_tab && current_tab->IsLoading();
}

ui::ImageModel BrowserNonClientFrameViewChromeOS::GetFaviconForTabIconView() {
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  return delegate ? delegate->GetWindowIcon() : ui::ImageModel();
}

void BrowserNonClientFrameViewChromeOS::OnWindowDestroying(
    aura::Window* window) {
  DCHECK(window_observation_.IsObserving());
  window_observation_.Reset();
  display_observer_.reset();
}

void BrowserNonClientFrameViewChromeOS::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  // Frames in chromeOS have rounded frames for certain window states. If these
  // states changes, we need to update the rounded corners accordingly. See
  // `chromeos::GetFrameCornerRadius()` for more details.
  if (chromeos::CanPropertyEffectFrameRadius(key)) {
    UpdateWindowRoundedCorners();
  }

  if (key == aura::client::kShowStateKey) {
    bool enter_fullscreen = window->GetProperty(aura::client::kShowStateKey) ==
                            ui::mojom::WindowShowState::kFullscreen;
    bool exit_fullscreen = static_cast<ui::mojom::WindowShowState>(old) ==
                           ui::mojom::WindowShowState::kFullscreen;

    // May have to hide caption buttons while in fullscreen mode, or show them
    // when exiting fullscreen.
    if (enter_fullscreen || exit_fullscreen)
      ResetWindowControls();

    // The client view (in particular the tab strip) has different layout in
    // restored vs. maximized/fullscreen. Invalidate the layout because the
    // window bounds may not have changed. https://crbug.com/1342414
    if (frame()->client_view())
      frame()->client_view()->InvalidateLayout();
  }

  if (key == chromeos::kWindowStateTypeKey) {
    // Update window controls when window state changes as whether or not these
    // are shown can depend on the window state (e.g. hiding the caption buttons
    // in non-immersive full screen mode, see crbug.com/1336470).
    ResetWindowControls();

    // Update the window controls if we are entering or exiting float state.
    const bool enter_floated = IsFloated();
    const bool exit_floated = static_cast<chromeos::WindowStateType>(old) ==
                              chromeos::WindowStateType::kFloated;
    if (!enter_floated && !exit_floated)
      return;

    if (frame_header_)
      frame_header_->OnFloatStateChanged();

    if (!display::Screen::GetScreen()->InTabletMode()) {
      return;
    }

    // Additionally updates immersive mode for PWA/SWA so that we show the title
    // bar when floated, and hide the title bar otherwise.
    browser_view()->immersive_mode_controller()->SetEnabled(
        ShouldEnableImmersiveModeController(false));

    return;
  }

  if (key == chromeos::kIsShowingInOverviewKey) {
    OnAddedToOrRemovedFromOverview();
    return;
  }

  if (!frame_header_)
    return;

  if (key == aura::client::kShowStateKey) {
    frame_header_->OnShowStateChanged(
        window->GetProperty(aura::client::kShowStateKey));
  } else if (key == chromeos::kFrameRestoreLookKey) {
    frame_header_->view()->InvalidateLayout();
  }
}

void BrowserNonClientFrameViewChromeOS::OnImmersiveRevealStarted() {
  ResetWindowControls();
  // The frame caption buttons use ink drop highlights and flood fill effects.
  // They make those buttons paint_to_layer. On immersive mode, the browser's
  // TopContainerView is also converted to paint_to_layer (see
  // ImmersiveModeControllerAsh::OnImmersiveRevealStarted()). In this mode, the
  // TopContainerView is responsible for painting this
  // BrowserNonClientFrameViewChromeOS (see TopContainerView::PaintChildren()).
  // However, BrowserNonClientFrameViewChromeOS is a sibling of TopContainerView
  // not a child. As a result, when the frame caption buttons are set to
  // paint_to_layer as a result of an ink drop effect, they will disappear.
  // https://crbug.com/840242. To fix this, we'll make the caption buttons
  // temporarily children of the TopContainerView while they're all painting to
  // their layers.
  auto* container = browser_view()->top_container();
  container->AddChildViewAt(caption_button_container_.get(), 0);

  container->DeprecatedLayoutImmediately();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // In Lacros, when entering in immersive fullscreen, it is possible
  // that chromeos::FrameHeader::painted_height_ is set to '0', when layout
  // occurs. This is because the tapstrip gets hidden.
  //
  // When it happens, PaintFrameImagesInRoundRect() has an empty rect
  // to paint onto, and the TabStrip's new theme is not painted.
  if (frame_header_ && frame_header_->GetHeaderHeightForPainting() == 0)
    frame_header_->LayoutHeader();
#endif
}

void BrowserNonClientFrameViewChromeOS::OnImmersiveRevealEnded() {
  ResetWindowControls();
  AddChildViewAt(caption_button_container_.get(), 0);

  DeprecatedLayoutImmediately();
}

void BrowserNonClientFrameViewChromeOS::OnImmersiveFullscreenExited() {
  OnImmersiveRevealEnded();
}

void BrowserNonClientFrameViewChromeOS::OnAppUpdate(
    const apps::AppUpdate& update) {
  Browser* browser = browser_view()->browser();

  if (!browser->app_controller() ||
      browser->app_controller()->app_id() != update.AppId() ||
      !caption_button_container_) {
    return;
  }

  caption_button_container_->SetCloseButtonEnabled(
      update.AllowClose().value_or(true));
}

void BrowserNonClientFrameViewChromeOS::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observation_.Reset();
}

void BrowserNonClientFrameViewChromeOS::PaintAsActiveChanged() {
  BrowserNonClientFrameView::PaintAsActiveChanged();

  UpdateProfileIcons();

  if (frame_header_)
    frame_header_->SetPaintAsActive(ShouldPaintAsActive());
}

void BrowserNonClientFrameViewChromeOS::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  BrowserNonClientFrameView::OnProfileAvatarChanged(profile_path);
  UpdateProfileIcons();
}

void BrowserNonClientFrameViewChromeOS::AddedToWidget() {
  if (highlight_border_overlay_ ||
      !GetWidget()->GetNativeWindow()->GetProperty(
          chromeos::kShouldHaveHighlightBorderOverlay)) {
    return;
  }

  highlight_border_overlay_ =
      std::make_unique<HighlightBorderOverlay>(GetWidget());
}

bool BrowserNonClientFrameViewChromeOS::GetShowCaptionButtons() const {
  if (GetOverviewMode()) {
    return false;
  }

  return GetShowCaptionButtonsWhenNotInOverview();
}

bool BrowserNonClientFrameViewChromeOS::GetShowCaptionButtonsWhenNotInOverview()
    const {
  if (GetHideCaptionButtonsForFullscreen()) {
    return false;
  }

  // Show the caption buttons for packaged apps which support immersive mode.
  if (UsePackagedAppHeaderStyle(browser_view()->browser())) {
    return true;
  }

  // Browsers in tablet mode still show their caption buttons in float state,
  // even with the webUI tab strip.
  if (display::Screen::GetScreen()->InTabletMode()) {
    return IsFloated();
  }

  return !UseWebUITabStrip();
}

int BrowserNonClientFrameViewChromeOS::GetToolbarLeftInset() const {
  // Include padding on left and right of icon.
  return profile_indicator_icon_
             ? kProfileIndicatorPadding * 2 + profile_indicator_icon_->width()
             : 0;
}

int BrowserNonClientFrameViewChromeOS::GetTabStripLeftInset() const {
  // Include padding on left of icon.
  // The tab strip has its own 'padding' to the right of the icon.
  return profile_indicator_icon_
             ? kProfileIndicatorPadding + profile_indicator_icon_->width()
             : 0;
}

int BrowserNonClientFrameViewChromeOS::GetTabStripRightInset() const {
  int inset = 0;
  if (GetShowCaptionButtonsWhenNotInOverview())
    inset += caption_button_container_->GetPreferredSize().width();
  return inset;
}

bool BrowserNonClientFrameViewChromeOS::GetShouldPaint() const {
  // Floated windows show their frame as they need to be dragged or hidden.
  if (IsFloated())
    return true;

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  // Normal windows that have a WebUI-based tab strip do not need a browser
  // frame as no tab strip is drawn on top of the browser frame.
  if (UseWebUITabStrip())
    return false;
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

  // We need to paint when the top-of-window views are revealed in immersive
  // fullscreen.
  ImmersiveModeController* immersive_mode_controller =
      browser_view()->immersive_mode_controller();
  if (immersive_mode_controller->IsEnabled())
    return immersive_mode_controller->IsRevealed();

  return !frame()->IsFullscreen();
}

void BrowserNonClientFrameViewChromeOS::OnAddedToOrRemovedFromOverview() {
  const bool should_show_caption_buttons = GetShowCaptionButtons();
  caption_button_container_->SetVisible(should_show_caption_buttons);
  if (!chromeos::features::AreOverviewSessionInitOptimizationsEnabled() ||
      browser_view()->GetIsWebAppType()) {
    // The WebAppFrameToolbarView is part of the BrowserView, so make sure the
    // BrowserView is re-layed out to take into account these changes.
    browser_view()->InvalidateLayout();
  }
}

std::unique_ptr<chromeos::FrameHeader>
BrowserNonClientFrameViewChromeOS::CreateFrameHeader() {
  std::unique_ptr<chromeos::FrameHeader> header;
  Browser* browser = browser_view()->browser();
  if (!UsePackagedAppHeaderStyle(browser)) {
    header = std::make_unique<BrowserFrameHeaderChromeOS>(
        frame(), this, this, caption_button_container_);
  } else {
    header = std::make_unique<chromeos::DefaultFrameHeader>(
        frame(), this, caption_button_container_);
  }

  header->SetLeftHeaderView(window_icon_);
  return header;
}

void BrowserNonClientFrameViewChromeOS::UpdateTopViewInset() {
  // In immersive fullscreen mode, the top view inset property should be 0.
  const bool immersive =
      browser_view()->immersive_mode_controller()->IsEnabled();
  const bool tab_strip_visible = browser_view()->GetTabStripVisible();
  const int inset = (tab_strip_visible || immersive ||
                     (AppIsPwaWithBorderlessDisplayMode() &&
                      browser_view()->IsBorderlessModeEnabled()))
                        ? 0
                        : GetTopInset(/*restored=*/false);
  frame()->GetNativeWindow()->SetProperty(aura::client::kTopViewInset, inset);
}

bool BrowserNonClientFrameViewChromeOS::GetShowProfileIndicatorIcon() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // We only show the profile indicator for the teleported browser windows
  // between multi-user sessions. Note that you can't teleport an incognito
  // window.
  Browser* browser = browser_view()->browser();
  if (browser->profile()->IsIncognitoProfile())
    return false;

  if (browser->is_type_popup())
    return false;

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  // TODO(http://crbug.com/1059514): This check shouldn't be necessary.  Provide
  // an appropriate affordance for the profile icon with the webUI tabstrip and
  // remove this block.
  if (!browser_view()->GetTabStripVisible())
    return false;
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

  return MultiUserWindowManagerHelper::ShouldShowAvatar(
      browser_view()->GetNativeWindow());
#else
  // Multi-signin support is deprecated in Lacros.
  return false;
#endif
}

void BrowserNonClientFrameViewChromeOS::UpdateProfileIcons() {
  // Multi-signin support is deprecated in Lacros, so only do this for ash.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  View* root_view = frame()->GetRootView();
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
    if (root_view)
      root_view->DeprecatedLayoutImmediately();
  }
#endif
}

void BrowserNonClientFrameViewChromeOS::UpdateWindowRoundedCorners() {
  DCHECK(GetWidget());

  aura::Window* frame_window = GetWidget()->GetNativeWindow();

  const int corner_radius = chromeos::GetFrameCornerRadius(frame_window);
  frame_window->SetProperty(aura::client::kWindowCornerRadiusKey,
                            corner_radius);

  if (frame_header_) {
    frame_header_->SetHeaderCornerRadius(corner_radius);
  }

  if (browser_view()->IsWindowControlsOverlayEnabled()) {
    // With window controls overlay enabled, the caption_button_container is
    // drawn above the client view. The container has a background that extends
    // over the curvature of the top-right corner, requiring its rounding.
    caption_button_container_->layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(0, corner_radius, 0, 0));
    caption_button_container_->layer()->SetIsFastRoundedCorner(/*enable=*/true);
  }

  if (chromeos::features::IsRoundedWindowsEnabled()) {
    GetWidget()->client_view()->UpdateWindowRoundedCorners(corner_radius);
  }
}

void BrowserNonClientFrameViewChromeOS::LayoutProfileIndicator() {
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

bool BrowserNonClientFrameViewChromeOS::GetOverviewMode() const {
  return GetFrameWindow()->GetProperty(chromeos::kIsShowingInOverviewKey);
}

bool BrowserNonClientFrameViewChromeOS::GetHideCaptionButtonsForFullscreen()
    const {
  if (!frame()->IsFullscreen())
    return false;

  auto* immersive_controller = browser_view()->immersive_mode_controller();

  // In fullscreen view, but not in immersive mode. Hide the caption buttons.
  if (!immersive_controller || !immersive_controller->IsEnabled())
    return true;

  return immersive_controller->ShouldHideTopViews();
}

void BrowserNonClientFrameViewChromeOS::OnUpdateFrameColor() {
  aura::Window* window = frame()->GetNativeWindow();
  window->SetProperty(chromeos::kFrameActiveColorKey,
                      GetFrameColor(BrowserFrameActiveState::kActive));
  window->SetProperty(chromeos::kFrameInactiveColorKey,
                      GetFrameColor(BrowserFrameActiveState::kInactive));

  if (frame_header_)
    frame_header_->UpdateFrameColors();
}

void BrowserNonClientFrameViewChromeOS::MaybeAnimateThemeChanged() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!browser_view())
    return;

  Browser* browser = browser_view()->browser();
  if (!browser)
    return;

  // Theme change events are only animated for system web apps which explicitly
  // request the behavior.
  bool animate_theme_change_for_swa =
      ash::IsSystemWebApp(browser) &&
      browser->app_controller()->system_app()->ShouldAnimateThemeChanges();
  if (!animate_theme_change_for_swa)
    return;

  views::WebView* web_view = browser_view()->contents_web_view();
  ui::Layer* layer = GetNativeViewLayer(web_view);
  content::RenderWidgetHost* render_widget_host = GetRenderWidgetHost(web_view);
  if (!layer || !render_widget_host)
    return;

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
      [](const base::WeakPtr<BrowserNonClientFrameViewChromeOS>& self,
         base::TimeTicks theme_changed_time, bool success) {
        if (!self || !self->browser_view())
          return;

        views::WebView* web_view = self->browser_view()->contents_web_view();
        ui::Layer* layer = GetNativeViewLayer(web_view);
        if (!layer)
          return;

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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

bool BrowserNonClientFrameViewChromeOS::IsFloated() const {
  return GetFrameWindow()->GetProperty(chromeos::kWindowStateTypeKey) ==
         chromeos::WindowStateType::kFloated;
}

bool BrowserNonClientFrameViewChromeOS::ShouldEnableImmersiveModeController(
    bool on_tablet_enabled) const {
  // Do not support immersive mode in kiosk.
  if (chromeos::IsKioskSession()) {
    return false;
  }

  // Enabling immersive mode controller would allow for the user to exit
  // fullscreen. We don't want this for locked fullscreen windows.
  if (!CanUserExitFullscreen()) {
    return false;
  }

  // If tablet mode is just enabled, we should exit immersive mode for TabStrip.
  // Note that we can still enter immersive mode if it's toggled after entering
  // tablet mode.
  if (on_tablet_enabled && browser_view()->GetSupportsTabStrip()) {
    return false;
  }

  if (display::Screen::GetScreen()->InTabletMode()) {
    // No immersive mode for minimized windows as they aren't visible, and
    // floated windows need a permanent header to drag.
    if (frame()->IsMinimized() || IsFloated()) {
      return false;
    }

    return true;
  }

  // In clamshell mode, we want immersive mode if fullscreen.
  return frame()->IsFullscreen();
}

bool BrowserNonClientFrameViewChromeOS::ShouldEnableFullscreenMode(
    bool on_tablet_enabled) const {
  // In kiosk mode, we always want to be fullscreen.
  if (chromeos::IsKioskSession()) {
    return true;
  }

  // If user cannot exit fullscreen, we always want to be fullscreen. This must
  // comes before the tablet mode condition since there is a case where the user
  // is not allowed to exit fullscreen while tablet mode (LockedFullscreen).
  if (!CanUserExitFullscreen()) {
    return true;
  }

  // If tablet mode is just enabled, we should exit fullscreen mode for
  // TabStrip. Note that we can still enter immersive mode if it's toggled after
  // entering tablet mode.
  if (on_tablet_enabled && browser_view()->GetSupportsTabStrip()) {
    return false;
  }

  return frame()->IsFullscreen();
}

bool BrowserNonClientFrameViewChromeOS::UseWebUITabStrip() const {
  return WebUITabStripContainerView::UseTouchableTabStrip(
             browser_view()->browser()) &&
         browser_view()->GetSupportsTabStrip();
}

const aura::Window* BrowserNonClientFrameViewChromeOS::GetFrameWindow() const {
  return const_cast<BrowserNonClientFrameViewChromeOS*>(this)->GetFrameWindow();
}

aura::Window* BrowserNonClientFrameViewChromeOS::GetFrameWindow() {
  return frame()->GetNativeWindow();
}

BEGIN_METADATA(BrowserNonClientFrameViewChromeOS)
ADD_READONLY_PROPERTY_METADATA(bool, ShowCaptionButtons)
ADD_READONLY_PROPERTY_METADATA(bool, ShowCaptionButtonsWhenNotInOverview)
ADD_READONLY_PROPERTY_METADATA(int, ToolbarLeftInset)
ADD_READONLY_PROPERTY_METADATA(int, TabStripLeftInset)
ADD_READONLY_PROPERTY_METADATA(int, TabStripRightInset)
ADD_READONLY_PROPERTY_METADATA(bool, ShouldPaint)
ADD_READONLY_PROPERTY_METADATA(bool, ShowProfileIndicatorIcon)
ADD_READONLY_PROPERTY_METADATA(bool, OverviewMode)

END_METADATA
