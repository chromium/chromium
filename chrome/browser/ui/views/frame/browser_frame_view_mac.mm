// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_mac.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/cocoa/fullscreen/fullscreen_menubar_tracker.h"
#include "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/fullscreen_util_mac.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/views/frame/caption_button_placeholder_container.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/tab_strip_view_interface.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_utils.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom-shared.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "ui/base/hit_test.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/widget/widget.h"

namespace {

FullscreenToolbarStyle GetUserPreferredToolbarStyle(bool always_show) {
  // In Kiosk mode, we don't show top Chrome UI.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode)) {
    return FullscreenToolbarStyle::TOOLBAR_NONE;
  }
  return always_show ? FullscreenToolbarStyle::TOOLBAR_PRESENT
                     : FullscreenToolbarStyle::TOOLBAR_HIDDEN;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameViewMac, public:

BrowserFrameViewMac::BrowserFrameViewMac(BrowserWidget* frame,
                                         BrowserView* browser_view)
    : BrowserFrameView(frame, browser_view),
      fullscreen_session_timer_(std::make_unique<base::OneShotTimer>()) {
  if (web_app::AppBrowserController::IsWebApp(browser_view->browser())) {
    auto* provider =
        web_app::WebAppProvider::GetForWebApps(browser_view->GetProfile());
    always_show_toolbar_in_fullscreen_observation_.Observe(
        &provider->registrar_unsafe());
  } else {
    show_fullscreen_toolbar_.Init(
        prefs::kShowFullscreenToolbar, browser_view->GetProfile()->GetPrefs(),
        base::BindRepeating(&BrowserFrameViewMac::UpdateFullscreenTopUI,
                            base::Unretained(this)));
  }
  if (!browser_view->UsesImmersiveFullscreenMode()) {
    fullscreen_toolbar_controller_ =
        [[FullscreenToolbarController alloc] initWithBrowserView:browser_view];
    [fullscreen_toolbar_controller_
        setToolbarStyle:GetUserPreferredToolbarStyle(
                            fullscreen_utils::IsAlwaysShowToolbarEnabled(
                                browser_view->browser()))];
  }

  if (browser_view->GetIsWebAppType()) {
    if (browser_view->IsWindowControlsOverlayEnabled()) {
      caption_button_placeholder_container_ =
          AddChildView(std::make_unique<CaptionButtonPlaceholderContainer>());
    }
  }
}

BrowserFrameViewMac::~BrowserFrameViewMac() {
  if ([fullscreen_toolbar_controller_ isInFullscreen]) {
    [fullscreen_toolbar_controller_ exitFullscreenMode];
  }
  EmitFullscreenSessionHistograms();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameViewMac, BrowserFrameView implementation:

void BrowserFrameViewMac::OnFullscreenStateChanged() {
  // Record the start of a browser fullscreen session. Content fullscreen is
  // ignored.
  if (browser_view()->IsFullscreen() &&
      !fullscreen_utils::IsInContentFullscreen(browser_view()->browser())) {
    fullscreen_session_start_ = base::TimeTicks::Now();

    // Add a backstop to emit the metric 24 hours from now. Any session lasting
    // more than 24 hours would be counted in the overflow bucket, so emit at 24
    // hours to get the count emitted faster.
    fullscreen_session_timer_->Start(
        FROM_HERE, base::Days(1),
        base::BindOnce(&BrowserFrameViewMac::EmitFullscreenSessionHistograms,
                       base::Unretained(this)));
  } else {
    fullscreen_session_timer_->Stop();
    EmitFullscreenSessionHistograms();
  }

  if (browser_view()->UsesImmersiveFullscreenMode()) {
    ImmersiveModeController::From(browser_view()->browser())
        ->SetEnabled(browser_view()->IsFullscreen());
    UpdateFullscreenTopUI();

    // browser_view()->DeprecatedLayoutImmediately() is not needed since top
    // chrome is in another widget.
    return;
  }

  if (browser_view()->IsFullscreen()) {
    [fullscreen_toolbar_controller_ enterFullscreenMode];
  } else {
    // Exiting tab fullscreen requires updating Top UI.
    // Called from here so we can capture exiting tab fullscreen both by
    // pressing 'ESC' key and by clicking green traffic light button.

    UpdateFullscreenTopUI();
    [fullscreen_toolbar_controller_ exitFullscreenMode];
  }
  browser_view()->DeprecatedLayoutImmediately();
}

bool BrowserFrameViewMac::CaptionButtonsOnLeadingEdge() const {
  // In "partial" RTL mode (where the OS is in LTR mode while Chrome is in RTL
  // mode, or vice versa), the traffic lights are on the trailing edge rather
  // than the leading edge.
  if (!GetWidget()) {
    return true;
  }

  NSWindow* const window = GetWidget()->GetNativeWindow().GetNativeNSWindow();
  if (!window) {
    return true;
  }

  NSUserInterfaceLayoutDirection direction =
      [window windowTitlebarLayoutDirection];
  return base::i18n::IsRTL() ==
         (direction == NSUserInterfaceLayoutDirectionRightToLeft);
}

gfx::Rect BrowserFrameViewMac::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size) const {
  // TODO(weili): In the future, we should hide the title bar, and show the
  // tab strip directly under the menu bar. For now, just lay our content
  // under the native title bar. Use the default title bar height to avoid
  // calling through private APIs.
  const bool restored =
      !browser_widget()->IsMaximized() && !browser_widget()->IsFullscreen();
  gfx::Rect bounds(0, GetTopInset(restored), width(),
                   tabstrip_minimum_size.height());

  // If we do not inset, the leftmost tab doesn't blend well with the bottom of
  // the tab strip. Normally, we would naturally have an inset from either the
  // caption buttons or the tab search button.
  if (browser_widget()->IsFullscreen()) {
    if (!browser_view()->UsesImmersiveFullscreenMode()) {
      bounds.Inset(
          gfx::Insets::TLBR(0, GetLayoutConstant(TOOLBAR_CORNER_RADIUS), 0, 0));
    }
  } else {
    // The bottom curve of the first/last tab swoops into the caption button
    // region, so account for this when calculating insets.
    const gfx::Insets insets = GetCaptionButtonInsets(
        /*visual_overlap=*/TabStyle::Get()->GetBottomCornerRadius());
    bounds.Inset(insets);
  }

  return bounds;
}

gfx::Rect BrowserFrameViewMac::GetBoundsForWebAppFrameToolbar(
    const gfx::Size& toolbar_preferred_size) const {
  gfx::Rect bounds(0, 0, width(),
                   toolbar_preferred_size.height() + kWebAppMenuMargin * 2);

  // Do not draw caption buttons on fullscreen.
  if (!browser_widget()->IsFullscreen()) {
    bounds.Inset(GetCaptionButtonInsets());
  }

  return bounds;
}

BrowserLayoutParams BrowserFrameViewMac::GetBrowserLayoutParams() const {
  auto params = BrowserFrameView::GetBrowserLayoutParams();
  if (browser_view()->IsFullscreen()) {
    // No insets for caption buttons in fullscreen, since caption buttons are on
    // a separate pane that slides in. However, preserve the height of the
    // caption area to ensure that the toolbar renders correctly (this is kind
    // of a hack but it prevents having to insert random hard-coded constants -
    // which is worse - see https://crbug.com/450817281).
    params.leading_exclusion.content.set_width(0);
    params.leading_exclusion.horizontal_padding = 0;
    params.trailing_exclusion.content.set_width(0);
    params.trailing_exclusion.horizontal_padding = 0;
  }
  return params;
}

int BrowserFrameViewMac::GetTopInset(bool restored) const {
  return 0;
}

void BrowserFrameViewMac::UpdateFullscreenTopUI() {
  Browser* browser = browser_view()->browser();
  // Update to the new toolbar style if needed.
  FullscreenToolbarStyle new_style;
  if (fullscreen_utils::IsInContentFullscreen(browser)) {
    new_style = FullscreenToolbarStyle::TOOLBAR_NONE;
  } else {
    bool always_show = fullscreen_utils::IsAlwaysShowToolbarEnabled(browser);
    new_style = GetUserPreferredToolbarStyle(always_show);
  }

  if (browser_view()->UsesImmersiveFullscreenMode()) {
    remote_cocoa::mojom::NativeWidgetNSWindow* ns_window_mojo =
        views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
            browser_view()->GetWidget()->GetNativeWindow())
            ->GetNSWindowMojo();
    static constexpr auto kStyleMap =
        base::MakeFixedFlatMap<FullscreenToolbarStyle,
                               remote_cocoa::mojom::ToolbarVisibilityStyle>(
            {{FullscreenToolbarStyle::TOOLBAR_PRESENT,
              remote_cocoa::mojom::ToolbarVisibilityStyle::kAlways},
             {FullscreenToolbarStyle::TOOLBAR_HIDDEN,
              remote_cocoa::mojom::ToolbarVisibilityStyle::kAutohide},
             {FullscreenToolbarStyle::TOOLBAR_NONE,
              remote_cocoa::mojom::ToolbarVisibilityStyle::kNone}});
    const auto it = kStyleMap.find(new_style);
    remote_cocoa::mojom::ToolbarVisibilityStyle mapped_style =
        it != kStyleMap.end()
            ? it->second
            : remote_cocoa::mojom::ToolbarVisibilityStyle::kAutohide;
    std::optional<remote_cocoa::mojom::ToolbarVisibilityStyle> old_style =
        std::exchange(current_toolbar_style_, mapped_style);
    ns_window_mojo->UpdateToolbarVisibility(mapped_style);

    // Update the immersive controller about content fullscreen changes.
    if (mapped_style == remote_cocoa::mojom::ToolbarVisibilityStyle::kNone) {
      ImmersiveModeController::From(browser_view()->browser())
          ->OnContentFullscreenChanged(true);
    } else if (old_style.has_value() &&
               old_style ==
                   remote_cocoa::mojom::ToolbarVisibilityStyle::kNone) {
      ImmersiveModeController::From(browser_view()->browser())
          ->OnContentFullscreenChanged(false);
    }

    // The layout changes further down are not needed in immersive fullscreen.
    return;
  }

  FullscreenToolbarStyle old_style =
      [fullscreen_toolbar_controller_ toolbarStyle];
  [fullscreen_toolbar_controller_ setToolbarStyle:new_style];
  if (![fullscreen_toolbar_controller_ isInFullscreen] ||
      old_style == new_style) {
    return;
  }

  // Notify browser that top ui state has been changed so that we can update
  // the bookmark bar state as well.
  browser->FullscreenTopUIStateChanged();

  // Re-layout if toolbar style changes in fullscreen mode.
  if (browser_widget()->IsFullscreen()) {
    browser_view()->DeprecatedLayoutImmediately();
  }
}

void BrowserFrameViewMac::OnAlwaysShowToolbarInFullscreenChanged(
    const webapps::AppId& app_id,
    bool show) {
  if (web_app::AppBrowserController::IsForWebApp(browser_view()->browser(),
                                                 app_id)) {
    UpdateFullscreenTopUI();
  }
}

void BrowserFrameViewMac::OnAppRegistrarDestroyed() {
  always_show_toolbar_in_fullscreen_observation_.Reset();
}

bool BrowserFrameViewMac::ShouldHideTopUIInFullscreen() const {
  return [fullscreen_toolbar_controller_ toolbarStyle] !=
         FullscreenToolbarStyle::TOOLBAR_PRESENT;
}

void BrowserFrameViewMac::UpdateThrobber(bool running) {}

void BrowserFrameViewMac::PaintAsActiveChanged() {
  UpdateCaptionButtonPlaceholderContainerBackground();
  BrowserFrameView::PaintAsActiveChanged();
}

void BrowserFrameViewMac::OnThemeChanged() {
  UpdateCaptionButtonPlaceholderContainerBackground();
  BrowserFrameView::OnThemeChanged();
}

void BrowserFrameViewMac::LayoutWebAppWindowTitle(
    const gfx::Rect& available_space,
    views::Label& window_title_label) const {
  // LINT.IfChange(mac_title_padding_width_fraction)
  static constexpr double kTitlePaddingWidthFraction = 0.1;
  // LINT.ThenChange(//chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_browsertest.cc:mac_title_padding_width_fraction)

  gfx::Rect toolbar_bounds(0, 0, width(), available_space.height());
  gfx::Rect title_bounds = available_space;

  const int title_padding =
      base::ClampRound(width() * kTitlePaddingWidthFraction);
  title_bounds.Inset(gfx::Insets::VH(0, title_padding));

  // Center in the container and make it fit in the available space.
  int preferred_title_width =
      window_title_label
          .GetPreferredSize(views::SizeBounds(window_title_label.width(), {}))
          .width();
  toolbar_bounds.ClampToCenteredSize(
      gfx::Size(preferred_title_width, toolbar_bounds.height()));
  toolbar_bounds.AdjustToFit(title_bounds);

  window_title_label.SetBoundsRect(toolbar_bounds);

  // The background of the title area is always opaquely drawn, but when in
  // immersive fullscreen, it is drawn in a way that isn't detected by the
  // DCHECK in Label. As such, disable the DCHECK.
  window_title_label.SetSkipSubpixelRenderingOpacityCheck(
      ImmersiveModeController::From(browser_view()->browser())->IsEnabled());
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameViewMac, views::FrameView implementation:

gfx::Rect BrowserFrameViewMac::GetBoundsForClientView() const {
  return bounds();
}

gfx::Rect BrowserFrameViewMac::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_inset = GetTopInset(false);

  // If the operating system is handling drawing the window titlebar then the
  // titlebar height will not be included in |GetTopInset|, so we have to
  // explicitly add it. If a custom titlebar is being drawn, this calculation
  // will be zero.
  NSWindow* window = GetWidget()->GetNativeWindow().GetNativeNSWindow();
  DCHECK(window);
  top_inset += window.frame.size.height -
               [window contentRectForFrameRect:window.frame].size.height;

  return gfx::Rect(client_bounds.x(), client_bounds.y() - top_inset,
                   client_bounds.width(), client_bounds.height() + top_inset);
}

int BrowserFrameViewMac::NonClientHitTest(const gfx::Point& point) {
  int super_component = BrowserFrameView::NonClientHitTest(point);
  if (super_component != HTNOWHERE) {
    return super_component;
  }

  // BrowserView::NonClientHitTest will return HTNOWHERE for points that hit
  // the native title bar. On Mac, we need to explicitly return HTCAPTION for
  // those points.
  const int component =
      browser_widget()->client_view()->NonClientHitTest(point);
  return (component == HTNOWHERE && bounds().Contains(point)) ? HTCAPTION
                                                              : component;
}

void BrowserFrameViewMac::UpdateMinimumSize() {
  GetWidget()->OnSizeConstraintsChanged();
}

void BrowserFrameViewMac::WindowControlsOverlayEnabledChanged() {
  if (browser_view()->IsWindowControlsOverlayEnabled()) {
    caption_button_placeholder_container_ =
        AddChildView(std::make_unique<CaptionButtonPlaceholderContainer>());
    UpdateCaptionButtonPlaceholderContainerBackground();
  } else {
    RemoveChildViewT(caption_button_placeholder_container_.ExtractAsDangling());
  }
}
///////////////////////////////////////////////////////////////////////////////
// BrowserFrameViewMac, views::View implementation:

gfx::Size BrowserFrameViewMac::GetMinimumSize() const {
  gfx::Size client_size = browser_widget()->client_view()->GetMinimumSize();
  if (browser_view()->browser()->is_type_normal()) {
    client_size.SetToMax(browser_view()->tab_strip_view()->GetMinimumSize());
  }

  // macOS apps generally don't allow their windows to get shorter than a
  // certain height, which empirically seems to be related to their *minimum*
  // width rather than their current width. This 4:3 ratio was chosen
  // empirically because it looks decent for both tabbed and untabbed browsers.
  client_size.SetToMax(gfx::Size(0, (client_size.width() * 3) / 4));

  return client_size;
}

void BrowserFrameViewMac::PaintChildren(const views::PaintInfo& info) {
  // In immersive fullscreen, the browser view's top container relies on the
  // non-client frame view to paint the frame (see comment in
  // TopContainerView::PaintChildren). We want the frame view to paint *only*
  // the frame but not its child (i.e. the BrowserView).
  // TODO(kerenzhu): we need this workaround due to the design of NonClientView,
  // that the frame part is not an independent child view. If it is an
  // independent view, overriding PaintChildren() will not be necessary.
  //
  // Tabbed immersive fullscreen paints its own background. In this case we
  // allow painting of the frame's children, which fixes a flickering bug:
  // 1400287.
  if (browser_view()->UsesImmersiveFullscreenTabbedMode() ||
      !ImmersiveModeController::From(browser_view()->browser())->IsRevealed()) {
    BrowserFrameView::PaintChildren(info);
  }
}

BrowserFrameViewMac::BoundsAndMargins
BrowserFrameViewMac::GetCaptionButtonBounds() const {
  BoundsAndMargins result;

  // These are empirically determined; feel free to change them if they're
  // not precise.
  if (@available(macOS 26, *)) {
    result.bounds = gfx::RectF(12, 10, 62, 18);
    result.margins = gfx::OutsetsF::VH(10, 12);
  } else {
    result.bounds = gfx::RectF(20, 11, 54, 16);
    result.margins = gfx::OutsetsF::VH(11, 20);
  }

  // Mirror for when caption buttons are on the "wrong" side.
  if (!CaptionButtonsOnLeadingEdge()) {
    result.bounds.set_x(width() - (result.bounds.x() + result.bounds.width()));
  }

  return result;
}

// LINT.IfChange(MacTabStripInsets)
gfx::Insets BrowserFrameViewMac::GetCaptionButtonInsets(
    int visual_overlap) const {
  const gfx::Rect bounds = GetCaptionButtonBounds().ToEnclosingRect();
  int caption_button_inset =
      CaptionButtonsOnLeadingEdge() ? bounds.right() : width() - bounds.x();

  // Subtract out the overlap, if any.
  caption_button_inset = std::max(0, caption_button_inset - visual_overlap);

  // Which side the inset goes on depends on which side the caption buttons are
  // on.
  if (CaptionButtonsOnLeadingEdge()) {
    return gfx::Insets::TLBR(0, caption_button_inset, 0, 0);
  } else {
    return gfx::Insets::TLBR(0, 0, 0, caption_button_inset);
  }
}
// LINT.ThenChange(//chrome/browser/ui/views/frame/immersive_mode_controller_mac.mm:MacTabStripInsets)

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameViewMac, protected:

// views::View:

void BrowserFrameViewMac::OnPaint(gfx::Canvas* canvas) {
  if (!browser_view()->GetIsNormalType() &&
      !browser_view()->GetIsWebAppType()) {
    return;
  }

  SkColor frame_color = GetFrameColor(BrowserFrameActiveState::kUseCurrent);
  canvas->DrawColor(frame_color);

  auto* theme_service =
      ThemeServiceFactory::GetForProfile(browser_view()->browser()->profile());
  if (!theme_service->UsingSystemTheme()) {
    PaintThemedFrame(canvas);
  }
}

void BrowserFrameViewMac::Layout(PassKey) {
  if (browser_view()->IsWindowControlsOverlayEnabled()) {
    LayoutWindowControlsOverlay();
  }
  LayoutSuperclass<BrowserFrameView>(this);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameViewMac, private:

gfx::Rect BrowserFrameViewMac::GetCenteredTitleBounds(
    gfx::Rect frame,
    gfx::Rect available_space,
    int preferred_title_width) {
  // Center in container.
  frame.ClampToCenteredSize(gfx::Size(preferred_title_width, frame.height()));

  // Make it fit in available space.
  frame.AdjustToFit(available_space);

  return frame;
}

void BrowserFrameViewMac::PaintThemedFrame(gfx::Canvas* canvas) {
  // On macOS the origin of the BrowserFrameViewMac is (0,0) so no
  // further modification is necessary. See
  // TopContainerBackground::PaintThemeCustomImage for details.
  gfx::Point theme_image_offset =
      browser_view()->GetThemeOffsetFromBrowserView();

  gfx::ImageSkia image = GetFrameImage();
  canvas->TileImageInt(image, theme_image_offset.x(), theme_image_offset.y(), 0,
                       TopUIFullscreenYOffset(), width(), image.height(),
                       /*tile_scale=*/1.0f, SkTileMode::kRepeat,
                       SkTileMode::kMirror);
  gfx::ImageSkia overlay = GetFrameOverlayImage();
  canvas->DrawImageInt(overlay, 0, 0);
}

int BrowserFrameViewMac::TopUIFullscreenYOffset() const {
  if (!browser_view()->GetTabStripVisible() ||
      !browser_view()->IsFullscreen() ||
      browser_view()->UsesImmersiveFullscreenMode()) {
    return 0;
  }

  CGFloat menu_bar_height =
      [[[NSApplication sharedApplication] mainMenu] menuBarHeight];
  // If there's a camera notch, the window is already below where the menu bar
  // will be, so we shouldn't account for it.
  if (@available(macos 12.0.1, *)) {
    id screen = [GetWidget()->GetNativeWindow().GetNativeNSWindow() screen];
    NSEdgeInsets insets = [screen safeAreaInsets];
    if (insets.top != 0) {
      menu_bar_height = 0;
    }
  }
  CGFloat title_bar_height =
      NSHeight([NSWindow frameRectForContentRect:NSZeroRect
                                       styleMask:NSWindowStyleMaskTitled]);
  return [[fullscreen_toolbar_controller_ menubarTracker] menubarFraction] *
         (menu_bar_height + title_bar_height);
}

void BrowserFrameViewMac::LayoutWindowControlsOverlay() {
  const int frame_available_height =
      browser_view()->GetWebAppFrameToolbarPreferredSize().height() +
      2 * kWebAppMenuMargin;
  gfx::Rect container_bounds = GetCaptionButtonBounds().ToEnclosingRect();
  container_bounds.set_height(frame_available_height);

  // Layout CaptionButtonPlaceholderContainer which would have the traffic
  // lights.
  caption_button_placeholder_container_->SetBoundsRect(container_bounds);
}

void BrowserFrameViewMac::UpdateCaptionButtonPlaceholderContainerBackground() {
  if (caption_button_placeholder_container_) {
    caption_button_placeholder_container_->SetBackground(
        views::CreateSolidBackground(
            GetFrameColor(BrowserFrameActiveState::kUseCurrent)));
  }
}

void BrowserFrameViewMac::EmitFullscreenSessionHistograms() {
  if (!fullscreen_session_start_.has_value()) {
    return;
  }
  base::TimeDelta delta =
      base::TimeTicks::Now() - fullscreen_session_start_.value();
  fullscreen_session_start_.reset();

  // Max duration of 1 day.
  UMA_HISTOGRAM_CUSTOM_TIMES("Session.BrowserFullscreen.DurationUpTo24H", delta,
                             base::Milliseconds(1), base::Days(1), 100);
}
