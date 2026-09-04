// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_win.h"

#include <dwmapi.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_caption_button_container_win.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/minimize_button_metrics_win.h"
#include "chrome/browser/ui/views/frame/safe_invoke/safe_invoke.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/win/titlebar_config.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle_win.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/win/hwnd_metrics.h"
#include "ui/color/color_provider_key.h"
#include "ui/display/win/dpi.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/win/icon_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/win/hwnd_util.h"
#include "ui/views/window/client_view.h"

std::array<HICON, BrowserFrameViewWin::kThrobberIconCount>
    BrowserFrameViewWin::throbber_icons_;

namespace {

// If nothing has been added to the left of the window title, match native
// Windows 10 UWP apps that don't have window icons.
// TODO(crbug.com/40890502): Avoid hardcoding sizes like this.
constexpr int kMinimumTitleLeftBorderMargin = 11;

// Additional left margin in the title bar when the window is maximized.
// TODO(crbug.com/40890502): Avoid hardcoding sizes like this.
constexpr int kMaximizedLeftMargin = 2;

constexpr int kIconTitleSpacing = 5;

}  // namespace



///////////////////////////////////////////////////////////////////////////////
// BrowserFrameViewWin, public:

BrowserFrameViewWin::BrowserFrameViewWin(BrowserWidget* widget,
                                         BrowserView* browser_view)
    : BrowserFrameView(widget, browser_view) {
  // We initialize all fields despite some of them being unused in some modes,
  // since it's possible for modes to flip dynamically (e.g. if the user enables
  // a high-contrast theme). Throbber icons are only used when ShowSystemIcon()
  // is true. Everything else here is only used when
  // ShouldBrowserCustomDrawTitlebar() is true.

  BrowserWindowInterface* browser = browser_view->browser();
  bool supports_title_bar =
      WindowFeatureController::From(browser)->SupportsWindowFeature(
          WindowFeatureController::WindowFeature::kFeatureTitleBar);

  // Only show icons if the browser supports title bars.
  if (supports_title_bar) {
    InitThrobberIcons();

    AddChildView(views::Builder<TabIconView>()
                     .CopyAddressTo(&window_icon_)
                     .SetModel(this)
                     .SetID(VIEW_ID_WINDOW_ICON)
                     // Stop the icon from intercepting clicks intended for the
                     // HTSYSMENU region of the window. Even though it does
                     // nothing on click, it will still prevent us from giving
                     // the event back to Windows to handle properly.
                     .SetCanProcessEventsWithinSubtree(false)
                     .Build());
  }

  // If this is a web app window, the window title will be part of the
  // BrowserView and thus we don't need to create another one here.
  if (!browser_view->GetIsWebAppType() && supports_title_bar) {
    window_title_ = new views::Label(browser_view->GetWindowTitle());
    window_title_->SetSubpixelRenderingEnabled(false);
    window_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    window_title_->SetID(VIEW_ID_WINDOW_TITLE);
    AddChildViewRaw(window_title_.get());
  }

  caption_button_container_ =
      AddChildView(std::make_unique<BrowserCaptionButtonContainer>(this));
}

BrowserFrameViewWin::~BrowserFrameViewWin() = default;

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameViewWin, BrowserFrameView implementation:

BrowserLayoutParams BrowserFrameViewWin::GetBrowserLayoutParams() const {
  BrowserLayoutParams params;
  params.visual_client_area = client_view_bounds_;
  const int top = params.visual_client_area.y();
  const auto& caption = caption_button_container_->bounds();
  // In some cases, the top of the client area is moved down, but slightly
  // overlaps the bottom of the caption container. In that case, don't count
  // the caption buttons in the exclusion area.
  if (top < caption.bottom() - 2) {
    if (CaptionButtonsOnLeadingEdge()) {
      params.leading_exclusion.content =
          gfx::SizeF(caption.right(), caption.bottom() - top);
    } else {
      params.trailing_exclusion.content =
          gfx::SizeF(width() - caption.x(), caption.bottom() - top);
      // In overlay mode, the icon is present but hidden.
      if (window_icon_ && !GetBrowserView()->IsWindowControlsOverlayEnabled()) {
        const auto& icon = window_icon_->bounds();
        params.leading_exclusion.content =
            gfx::SizeF(icon.right(), icon.bottom() - top);
        params.leading_exclusion.horizontal_padding = kIconTitleSpacing;
        params.leading_exclusion.vertical_padding = icon.y() - top;
      }
    }
  }
  return params;
}

bool BrowserFrameViewWin::CaptionButtonsOnLeadingEdge() const {
  return false;
}

int BrowserFrameViewWin::GetTopInset(bool restored) const {
  if (GetBrowserView()->GetTabStripVisible()) {
    return TopAreaHeight(restored);
  }
  return TitlebarHeight(restored);
}

SkColor BrowserFrameViewWin::GetCaptionColor(
    BrowserFrameActiveState active_state) const {
  return GetColorProvider()->GetColor(ShouldPaintAsActiveForState(active_state)
                                          ? kColorCaptionForegroundActive
                                          : kColorCaptionForegroundInactive);
}

void BrowserFrameViewWin::UpdateThrobber(bool running) {
  if (ShouldShowWindowIcon(TitlebarType::kCustom)) {
    window_icon_->Update();
  } else if (ShouldShowWindowIcon(TitlebarType::kSystem)) {
    if (throbber_running_) {
      if (running) {
        DisplayNextThrobberFrame();
      } else {
        StopThrobber();
      }
    } else if (running) {
      StartThrobber();
    }
  }
}

gfx::Size BrowserFrameViewWin::GetMinimumSize() const {
  gfx::Size min_size(GetBrowserView()->GetMinimumSize());
  min_size.Enlarge(0, GetTopInset(false));

  gfx::Size titlebar_min_size(
      display::win::GetScreenWin()->GetSystemMetricsInDIP(SM_CXSIZEFRAME) +
          CaptionButtonsRegionWidth(),
      TitlebarHeight(false));
  if (ShouldShowWindowIcon(TitlebarType::kAny)) {
    titlebar_min_size.Enlarge(
        display::win::GetScreenWin()->GetSystemMetricsInDIP(SM_CXSMICON) +
            kIconTitleSpacing,
        0);
  }

  min_size.SetToMax(titlebar_min_size);

  return min_size;
}

void BrowserFrameViewWin::WindowControlsOverlayEnabledChanged() {
  caption_button_container_->OnWindowControlsOverlayEnabledChanged();
}

void BrowserFrameViewWin::LayoutWebAppWindowTitle(
    const gfx::Rect& available_space,
    views::Label& window_title_label) const {
  gfx::Rect bounds = available_space;
  if (bounds.x() < kMinimumTitleLeftBorderMargin) {
    bounds.SetHorizontalBounds(kMinimumTitleLeftBorderMargin, bounds.right());
  }
  window_title_label.SetSubpixelRenderingEnabled(false);
  window_title_label.SetHorizontalAlignment(gfx::ALIGN_LEFT);
  window_title_label.SetAutoColorReadabilityEnabled(false);
  window_title_label.SetBoundsRect(bounds);
}

BrowserFrameViewWin::BoundsAndMargins
BrowserFrameViewWin::GetCaptionButtonBounds() const {
  return BoundsAndMargins{gfx::RectF(caption_button_container_->bounds())};
}

void BrowserFrameViewWin::PaintAsActiveChanged() {
  BrowserFrameView::PaintAsActiveChanged();

  // When window controls overlay is enabled, the caption button container is
  // painted to a layer and is not repainted by
  // BrowserFrameView::PaintAsActiveChanged. Schedule a re-paint here
  // to update the caption button colors.
  if (caption_button_container_->layer()) {
    caption_button_container_->SchedulePaint();
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameViewWin, views::FrameView implementation:

gfx::Rect BrowserFrameViewWin::GetBoundsForClientView() const {
  return client_view_bounds_;
}

gfx::Rect BrowserFrameViewWin::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  HWND hwnd = views::HWNDForWidget(browser_widget());
  if (!GetBrowserView()->GetTabStripVisible() && hwnd) {
    // If we don't have a tabstrip, we're either a popup or an app window, in
    // which case we have a standard size non-client area and can just use
    // AdjustWindowRectEx to obtain it. We check for a non-null window handle in
    // case this gets called before the window is actually created.
    RECT rect = client_bounds.ToRECT();
    AdjustWindowRectEx(&rect, GetWindowLong(hwnd, GWL_STYLE), FALSE,
                       GetWindowLong(hwnd, GWL_EXSTYLE));
    return gfx::Rect(rect);
  }

  const int top_inset = GetTopInset(false);
  return gfx::Rect(client_bounds.x(),
                   std::max(0, client_bounds.y() - top_inset),
                   client_bounds.width(), client_bounds.height() + top_inset);
}

int BrowserFrameViewWin::NonClientHitTest(const gfx::Point& point) {
  int super_component = BrowserFrameView::NonClientHitTest(point);
  if (super_component != HTNOWHERE) {
    return super_component;
  }

  // If the point isn't within our bounds, then it's in the native portion of
  // the frame so again Windows can figure it out.
  if (!bounds().Contains(point)) {
    return HTNOWHERE;
  }

  // At the window corners the resize area is not actually bigger, but the 16
  // pixels at the end of the top and bottom edges trigger diagonal resizing.
  constexpr int kResizeCornerWidth = 16;

  const int top_border_thickness =
      (GetBrowserView()->GetIsWebAppType() || IsFrameCondensed())
          ? FrameTopBorderThickness(false)
          : GetLayoutConstant(LayoutConstant::kTabStripPadding);

  const int window_component = GetHTComponentForFrame(
      point, gfx::Insets::TLBR(top_border_thickness, 0, 0, 0),
      top_border_thickness, kResizeCornerWidth - FrameBorderThickness(),
      browser_widget()->widget_delegate()->CanResize());

  const int frame_component =
      browser_widget()->client_view()->NonClientHitTest(point);

  // In fullscreen there is no draggable or resizable frame, so window
  // controls overlay hits outside the caption buttons must stay HTCLIENT;
  // HTCAPTION would swallow clicks meant for the overlaid app UI.
  const bool is_fullscreen_with_overlay =
      browser_widget()->IsFullscreen() &&
      GetBrowserView()->IsWindowControlsOverlayEnabled();

  // See if we're in the sysmenu region.  We still have to check the tabstrip
  // first so that clicks in a tab don't get treated as sysmenu clicks.
  if (frame_component != HTCLIENT && ShouldShowWindowIcon(TitlebarType::kAny)) {
    gfx::Rect sys_menu_region(
        0, display::win::GetScreenWin()->GetSystemMetricsInDIP(SM_CYSIZEFRAME),
        display::win::GetScreenWin()->GetSystemMetricsInDIP(SM_CXSMICON),
        display::win::GetScreenWin()->GetSystemMetricsInDIP(SM_CYSMICON));
    if (sys_menu_region.Contains(point)) {
      return HTSYSMENU;
    }
  }

  if (frame_component != HTNOWHERE) {
    if (is_fullscreen_with_overlay) {
      return HTCLIENT;
    }

    // If the clientview registers a hit within its bounds, it's still possible
    // that the hit target should be top resize since the tabstrip region paints
    // to the top of the frame. If the frame registered a hit for the Top
    // resize, override the client frame target.
    if (window_component == HTTOP && !IsMaximized()) {
      return window_component;
    }
    return frame_component;
  }

  // Then see if the point is within any of the window controls.
  const gfx::Point local_point =
      ConvertPointToTarget(parent(), caption_button_container_, point);
  if (caption_button_container_->HitTestPoint(local_point)) {
    const int hit_test_result =
        caption_button_container_->NonClientHitTest(local_point);
    if (hit_test_result != HTNOWHERE) {
      return hit_test_result;
    }
  }

  if (is_fullscreen_with_overlay) {
    return HTCLIENT;
  }

  // On Windows, the caption buttons are almost butted up to the top right
  // corner of the window. This code ensures the mouse isn't set to a size
  // cursor while hovering over the caption buttons, thus giving the incorrect
  // impression that the user can resize the window.
  RECT button_bounds = {0};
  if (SUCCEEDED(DwmGetWindowAttribute(views::HWNDForWidget(browser_widget()),
                                      DWMWA_CAPTION_BUTTON_BOUNDS,
                                      &button_bounds, sizeof(button_bounds)))) {
    gfx::RectF button_bounds_in_dips = gfx::ConvertRectToDips(
        gfx::Rect(button_bounds), display::win::GetDPIScale());
    // TODO(crbug.com/40150311): GetMirroredRect() requires an integer rect,
    // but the size in DIPs may not be an integer with a fractional device
    // scale factor. If we want to keep using integers, the choice to use
    // ToFlooredRectDeprecated() seems to be doing the wrong thing given the
    // comment below about insetting 1 DIP instead of 1 physical pixel. We
    // should probably use ToEnclosedRect() and then we could have inset 1
    // physical pixel here.
    gfx::Rect buttons =
        GetMirroredRect(gfx::ToFlooredRectDeprecated(button_bounds_in_dips));

    // There is a small one-pixel strip right above the caption buttons in
    // which the resize border "peeks" through.
    constexpr int kCaptionButtonTopInset = 1;
    // The sizing region at the window edge above the caption buttons is
    // 1 px regardless of scale factor. If we inset by 1 before converting
    // to DIPs, the precision loss might eliminate this region entirely. The
    // best we can do is to inset after conversion. This guarantees we'll
    // show the resize cursor when resizing is possible. The cost of which
    // is also maybe showing it over the portion of the DIP that isn't the
    // outermost pixel.
    buttons.Inset(gfx::Insets::TLBR(kCaptionButtonTopInset, 0, 0, 0));
    if (buttons.Contains(point)) {
      return HTNOWHERE;
    }
  }

  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void BrowserFrameViewWin::UpdateWindowIcon() {
  if (window_icon_ && window_icon_->GetVisible()) {
    window_icon_->SchedulePaint();
  }
}

void BrowserFrameViewWin::UpdateWindowTitle() {
  LayoutTitleBar();
  if (window_title_ && window_title_->GetVisible()) {
    window_title_->SchedulePaint();
  }
}

void BrowserFrameViewWin::ResetWindowControls() {
  BrowserFrameView::ResetWindowControls();
  caption_button_container_->ResetWindowControls();
}

void BrowserFrameViewWin::OnThemeChanged() {
  BrowserFrameView::OnThemeChanged();
}

gfx::RoundedCornersF BrowserFrameViewWin::GetWindowRoundedCorners() const {
  const auto* const widget = GetWidget();
  if (widget && !widget->IsMaximized() && !widget->IsFullscreen() &&
      !IsWindowArranged(views::HWNDForWidget(widget))) {
    return gfx::RoundedCornersF(
        GetLayoutConstant(LayoutConstant::kToolbarCornerRadius));
  }
  return gfx::RoundedCornersF();
}

gfx::Point BrowserFrameViewWin::GetKeyboardContextMenuLocation() {
  gfx::Point point(0, 0);
  ConvertPointToScreen(this, &point);
  return point;
}

bool BrowserFrameViewWin::ShouldTabIconViewAnimate() const {
  if (!ShouldShowWindowIcon(TitlebarType::kCustom)) {
    return false;
  }

  // Web apps use their app icon and shouldn't show a throbber.
  if (GetBrowserView()->GetIsWebAppType()) {
    return false;
  }

  content::WebContents* current_tab = GetBrowserView()->GetActiveWebContents();
  return current_tab && current_tab->ShouldShowLoadingUI();
}

ui::ImageModel BrowserFrameViewWin::GetFaviconForTabIconView() {
  // A paint may race a fullscreen transition before the next titlebar layout
  // hides the icon view; don't assert in that transient state.
  if (!ShouldShowWindowIcon(TitlebarType::kCustom)) {
    return ui::ImageModel();
  }
  return browser_widget()->widget_delegate()->GetWindowIcon();
}

bool BrowserFrameViewWin::IsMaximized() const {
  return browser_widget()->IsMaximized();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameViewWin, views::View overrides:

void BrowserFrameViewWin::OnPaint(gfx::Canvas* canvas) {
  TRACE_EVENT0("views.frame", "BrowserFrameViewWin::OnPaint");
  PaintTitlebar(canvas);
}

void BrowserFrameViewWin::Layout(PassKey) {
  TRACE_EVENT0("views.frame", "BrowserFrameViewWin::Layout");

  LayoutCaptionButtons();
  if (!GetBrowserView()->IsWindowControlsOverlayEnabled()) {
    LayoutTitleBar();
  }
  LayoutClientView();
  LayoutSuperclass<BrowserFrameView>(this);
}


///////////////////////////////////////////////////////////////////////////////
// BrowserFrameViewWin, private:

int BrowserFrameViewWin::FrameBorderThickness() const {
  return (IsMaximized() || browser_widget()->IsFullscreen())
             ? 0
             : display::win::GetScreenWin()->GetSystemMetricsInDIP(
                   SM_CXSIZEFRAME);
}

int BrowserFrameViewWin::FrameTopBorderThickness(bool restored) const {
  const bool is_fullscreen =
      (browser_widget()->IsFullscreen() || IsMaximized()) && !restored;
  if (!is_fullscreen && GetBrowserView()->GetTabStripVisible()) {
    // Restored windows have a smaller top resize handle than the system
    // default. When maximized, the OS sizes the window such that the border
    // extends beyond the screen edges. In that case, we must return the
    // default value.
    return 0;
  }

  // Mouse and touch locations are floored but GetSystemMetricsInDIP is rounded,
  // so we need to floor instead or else the difference will cause the hittest
  // to fail when it ought to succeed.
  return std::floor(
      FrameTopBorderThicknessPx(restored) /
      display::win::GetScreenWin()->GetScaleFactorForHWND(HWNDForView(this)));
}

int BrowserFrameViewWin::FrameTopBorderThicknessPx(bool restored) const {
  // Distinct from FrameBorderThickness() because we can't inset the top
  // border, otherwise Windows will give us a standard titlebar.
  // For maximized windows this is not true, and the top border must be
  // inset in order to avoid overlapping the monitor above.
  // See comments in BrowserDesktopWindowTreeHostWin::GetClientAreaInsets().
  const bool needs_no_border =
      (ShouldBrowserCustomDrawTitlebar(GetBrowserView()) &&
       browser_widget()->IsMaximized()) ||
      browser_widget()->IsFullscreen();
  if (needs_no_border && !restored) {
    return 0;
  }

  // Note that this method assumes an equal resize handle thickness on all
  // sides of the window.
  // TODO(dfried): Consider having it return a gfx::Insets object instead.
  return ui::GetFrameThicknessFromWindow(HWNDForView(this),
                                         MONITOR_DEFAULTTONEAREST);
}

int BrowserFrameViewWin::TopAreaHeight(bool restored) const {
  if (browser_widget()->IsFullscreen() && !restored) {
    return 0;
  }

  // The tabstrip controls its own top padding.
  return FrameTopBorderThickness(restored);
}

int BrowserFrameViewWin::TitlebarMaximizedVisualHeight() const {
  int maximized_height =
      display::win::GetScreenWin()->GetSystemMetricsInDIP(SM_CYCAPTION);
  // Adding 2 dip of vertical padding puts at least 1 dip of space on the top
  // and bottom of the element.
  constexpr int kVerticalPadding = 2;
  const auto toolbar_height =
      GetClientFrameElementInfo().toolbar_minimum_height;
  if (toolbar_height > 0) {
    maximized_height =
        std::max(maximized_height, toolbar_height + kVerticalPadding);
  }
  return maximized_height;
}

int BrowserFrameViewWin::TitlebarHeight(bool restored) const {
  if (browser_widget()->IsFullscreen() && !restored) {
    return 0;
  }

  // The titlebar's actual height is the same in restored and maximized, but
  // some of it is above the screen in maximized mode. See the comment in
  // FrameTopBorderThicknessPx().
  return TitlebarMaximizedVisualHeight() + FrameTopBorderThickness(false);
}

int BrowserFrameViewWin::GetFrameHeight() const {
  const auto info = GetClientFrameElementInfo();
  if (info.tabstrip_preferred_height) {
    return info.tabstrip_preferred_height - WindowTopY() -
           GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap);
  }
  return IsMaximized() ? TitlebarMaximizedVisualHeight()
                       : TitlebarHeight(false);
}

int BrowserFrameViewWin::WindowTopY() const {
  // The window top is SM_CYSIZEFRAME pixels when maximized (see the comment in
  // FrameTopBorderThickness()) and floor(system dsf) pixels when restored.
  // Unfortunately we can't represent either of those at hidpi without using
  // non-integral dips, so we return the closest reasonable values instead.
  return IsMaximized() ? FrameTopBorderThickness(false) : 1;
}

int BrowserFrameViewWin::CaptionButtonsRegionWidth() const {
  return caption_button_container_->size().width();
}

bool BrowserFrameViewWin::ShouldShowWindowIcon(TitlebarType type) const {
  if (type == TitlebarType::kSystem) {
    return false;
  }
  if (browser_widget()->IsFullscreen()) {
    return false;
  }
  return GetBrowserView()->ShouldShowWindowIcon();
}

bool BrowserFrameViewWin::ShouldShowWindowTitle(TitlebarType type) const {
  if (type == TitlebarType::kSystem) {
    return false;
  }
  if (browser_widget()->IsFullscreen()) {
    return false;
  }
  return GetBrowserView()->ShouldShowWindowTitle();
}

SkColor BrowserFrameViewWin::GetTitlebarColor() const {
  return GetFrameColor(BrowserFrameActiveState::kUseCurrent);
}

void BrowserFrameViewWin::PaintTitlebar(gfx::Canvas* canvas) const {
  TRACE_EVENT0("views.frame", "BrowserFrameViewWin::PaintTitlebar");

  // This is the pixel-accurate version of WindowTopY(). Scaling the DIP values
  // here compounds precision error, which exposes unpainted client area. When
  // restored it uses the system dsf instead of the per-monitor dsf to match
  // Windows' behavior.
  const int y = IsMaximized() ? FrameTopBorderThicknessPx(false)
                              : std::floor(display::win::GetDPIScale());

  // Draw the top of the accent border.
  //
  // We let the DWM do this for the other sides of the window by insetting the
  // client area to leave nonclient area available. However, along the top
  // window edge, we have to have zero nonclient area or the DWM will draw a
  // full native titlebar outside our client area. See
  // BrowserDesktopWindowTreeHostWin::GetClientAreaInsets().
  //
  // We could ask the DWM to draw the top accent border in the client area (by
  // calling DwmExtendFrameIntoClientArea() in
  // BrowserDesktopWindowTreeHostWin::UpdateDWMbrowser_widget()), but this
  // requires that we leave part of the client surface transparent. If we draw
  // this ourselves, we can make the client surface fully opaque and avoid the
  // power consumption needed for DWM to blend the window contents.
  //
  // So the accent border also has to be opaque. We can blend the titlebar
  // color with the accent border to approximate the native effect.
  const SkColor titlebar_color = GetTitlebarColor();
  gfx::ScopedCanvas scoped_canvas(canvas);
  float scale = canvas->UndoDeviceScaleFactor();
  cc::PaintFlags flags;
  flags.setColor(color_utils::GetResultingPaintColor(
      GetColorProvider()->GetColor(ShouldPaintAsActive()
                                       ? kColorAccentBorderActive
                                       : kColorAccentBorderInactive),
      titlebar_color));
  canvas->DrawRect(gfx::RectF(0, 0, width() * scale, y), flags);

  const auto info = GetClientFrameElementInfo();
  const int titlebar_height =
      std::max(TitlebarHeight(false),
               TopAreaHeight(false) + info.tabstrip_preferred_height);
  const gfx::Rect titlebar_rect = gfx::ToEnclosingRect(
      gfx::RectF(0, y, width() * scale, titlebar_height * scale - y));
  // Paint the titlebar first so we have a background if an area isn't covered
  // by the theme image.
  flags.setColor(titlebar_color);
  canvas->DrawRect(titlebar_rect, flags);
  const gfx::ImageSkia frame_image = GetFrameImage();
  if (!frame_image.isNull()) {
    canvas->TileImageInt(frame_image, 0,
                         ThemeProperties::kFrameHeightAboveTabs -
                             GetTopInset(false) + titlebar_rect.y(),
                         titlebar_rect.x(), titlebar_rect.y(),
                         titlebar_rect.width(), titlebar_rect.height(), scale,
                         SkTileMode::kRepeat, SkTileMode::kMirror);
  }
  const gfx::ImageSkia frame_overlay_image = GetFrameOverlayImage();
  if (!frame_overlay_image.isNull()) {
    canvas->DrawImageInt(frame_overlay_image, 0, 0, frame_overlay_image.width(),
                         frame_overlay_image.height(), titlebar_rect.x(),
                         titlebar_rect.y(), frame_overlay_image.width() * scale,
                         frame_overlay_image.height() * scale, true);
  }

  if (ShouldShowWindowTitle(TitlebarType::kCustom) && window_title_) {
    window_title_->SetEnabledColor(
        GetCaptionColor(BrowserFrameActiveState::kUseCurrent));
  }
}

void BrowserFrameViewWin::LayoutTitleBar() {
  TRACE_EVENT0("views.frame", "BrowserFrameViewWin::LayoutTitleBar");
  const bool show_icon = ShouldShowWindowIcon(TitlebarType::kCustom);
  const bool show_title = ShouldShowWindowTitle(TitlebarType::kCustom);
  SafeInvoke(window_icon_.get()).Then(&views::View::SetVisible, show_icon);
  SafeInvoke(window_title_.get()).Then(&views::View::SetVisible, show_title);
  if (!show_icon && !show_title) {
    return;
  }

  const int icon_size =
      display::win::GetScreenWin()->GetSystemMetricsInDIP(SM_CYSMICON);
  const int titlebar_visual_height =
      IsMaximized() ? TitlebarMaximizedVisualHeight() : TitlebarHeight(false);
  // Don't include the area above the screen when maximized. However it only
  // looks centered if we start from y=0 when restored.
  const int window_top = IsMaximized() ? WindowTopY() : 0;
  int next_leading_x =
      display::win::GetScreenWin()->GetSystemMetricsInDIP(SM_CXSIZEFRAME);
  if (IsMaximized()) {
    next_leading_x += kMaximizedLeftMargin;
  }
  int next_trailing_x = width() - CaptionButtonsRegionWidth();

  const int y = window_top + (titlebar_visual_height - icon_size) / 2;
  const gfx::Rect window_icon_bounds =
      gfx::Rect(next_leading_x, y, icon_size, icon_size);

  if (show_icon) {
    window_icon_->SetBoundsRect(window_icon_bounds);
    next_leading_x = window_icon_bounds.right() + kIconTitleSpacing;
  }

  if (show_title && window_title_) {
    window_title_->SetText(GetBrowserView()->GetWindowTitle());
    const int max_text_width = std::max(0, next_trailing_x - next_leading_x);
    LayoutWebAppWindowTitle(
        gfx::Rect(next_leading_x, window_icon_bounds.y(), max_text_width,
                  window_icon_bounds.height()),
        *window_title_);
  }
}

void BrowserFrameViewWin::LayoutCaptionButtons() {
  TRACE_EVENT0("views.frame", "BrowserFrameViewWin::LayoutCaptionButtons");

  caption_button_container_->SetVisible(
      !browser_widget()->IsFullscreen() ||
      GetBrowserView()->IsWindowControlsOverlayEnabled());

  const gfx::Size preferred_size =
      caption_button_container_->GetPreferredSize();

  const int height =
      !GetBrowserView()->GetWebAppFrameToolbarPreferredSize().IsEmpty()
          ? (TitlebarHeight(false) - WindowTopY())
          : GetFrameHeight();

  caption_button_container_->SetBounds(width() - preferred_size.width(),
                                       WindowTopY(), preferred_size.width(),
                                       height);
}

void BrowserFrameViewWin::LayoutClientView() {
  client_view_bounds_ = GetLocalBounds();
  int top_inset = GetTopInset(false);
  if (GetBrowserView()->IsWindowControlsOverlayEnabled() ||
      !GetBrowserView()->GetWebAppFrameToolbarPreferredSize().IsEmpty()) {
    top_inset = browser_widget()->IsFullscreen() ? 0 : WindowTopY();
  }
  client_view_bounds_.Inset(gfx::Insets::TLBR(top_inset, 0, 0, 0));
}

void BrowserFrameViewWin::StartThrobber() {
  DCHECK(ShouldShowWindowIcon(TitlebarType::kSystem));
  if (!throbber_running_) {
    throbber_running_ = true;
    throbber_frame_ = 0;
    InitThrobberIcons();
    SendMessage(views::HWNDForWidget(browser_widget()), WM_SETICON,
                static_cast<WPARAM>(ICON_SMALL),
                reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
  }
}

void BrowserFrameViewWin::StopThrobber() {
  DCHECK(ShouldShowWindowIcon(TitlebarType::kSystem));
  if (throbber_running_) {
    throbber_running_ = false;

    base::win::ScopedGDIObject<HICON> previous_small_icon;
    base::win::ScopedGDIObject<HICON> previous_big_icon;
    HICON small_icon = nullptr;
    HICON big_icon = nullptr;

    gfx::ImageSkia icon =
        GetBrowserView()->GetWindowIcon().Rasterize(GetColorProvider());

    if (!icon.isNull()) {
      // Keep previous icons alive as long as they are referenced by the HWND.
      previous_small_icon = std::move(small_window_icon_);
      previous_big_icon = std::move(big_window_icon_);

      // Take responsibility for eventually destroying the created icons.
      small_window_icon_ = IconUtil::CreateHICONFromSkBitmapSizedTo(
          *icon.bitmap(), GetSystemMetrics(SM_CXSMICON),
          GetSystemMetrics(SM_CYSMICON));
      big_window_icon_ = IconUtil::CreateHICONFromSkBitmapSizedTo(
          *icon.bitmap(), GetSystemMetrics(SM_CXICON),
          GetSystemMetrics(SM_CYICON));

      small_icon = small_window_icon_.get();
      big_icon = big_window_icon_.get();
    }

    // Fallback to class icon.
    if (!small_icon) {
      small_icon = reinterpret_cast<HICON>(GetClassLongPtr(
          views::HWNDForWidget(browser_widget()), GCLP_HICONSM));
    }
    if (!big_icon) {
      big_icon = reinterpret_cast<HICON>(
          GetClassLongPtr(views::HWNDForWidget(browser_widget()), GCLP_HICON));
    }

    // This will reset the icon which we set in the throbber code.
    // WM_SETICON with null icon restores the icon for title bar but not
    // for taskbar. See http://crbug.com/40334833
    SendMessage(views::HWNDForWidget(browser_widget()), WM_SETICON,
                static_cast<WPARAM>(ICON_SMALL),
                reinterpret_cast<LPARAM>(small_icon));

    SendMessage(views::HWNDForWidget(browser_widget()), WM_SETICON,
                static_cast<WPARAM>(ICON_BIG),
                reinterpret_cast<LPARAM>(big_icon));
  }
}

void BrowserFrameViewWin::DisplayNextThrobberFrame() {
  throbber_frame_ = (throbber_frame_ + 1) % kThrobberIconCount;
  SendMessage(views::HWNDForWidget(browser_widget()), WM_SETICON,
              static_cast<WPARAM>(ICON_SMALL),
              reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
}

// static
void BrowserFrameViewWin::InitThrobberIcons() {
  static bool initialized = false;
  if (!initialized) {
    for (int i = 0; i < kThrobberIconCount; ++i) {
      throbber_icons_[i] =
          ui::LoadThemeIconFromResourcesDataDLL(IDI_THROBBER_01 + i);
      DCHECK(throbber_icons_[i]);
    }
    initialized = true;
  }
}

BEGIN_METADATA(BrowserFrameViewWin)
END_METADATA
