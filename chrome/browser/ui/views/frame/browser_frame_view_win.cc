// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/views/frame/browser_frame_view_win.h"

#include <dwmapi.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_caption_button_container_win.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/win/mica_titlebar.h"
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
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/win/hwnd_util.h"
#include "ui/views/window/client_view.h"

HICON BrowserFrameViewWin::throbber_icons_
    [BrowserFrameViewWin::kThrobberIconCount];

namespace {

// Converts the |image| to a Windows icon and returns the corresponding HICON
// handle. |image| is resized to desired |width| and |height| if needed.
base::win::ScopedHICON CreateHICONFromSkBitmapSizedTo(
    const gfx::ImageSkia& image,
    int width,
    int height) {
  return IconUtil::CreateHICONFromSkBitmap(
      width == image.width() && height == image.height()
          ? *image.bitmap()
          : skia::ImageOperations::Resize(*image.bitmap(),
                                          skia::ImageOperations::RESIZE_BEST,
                                          width, height));
}

// Additional left margin in the title bar when the window is maximized.
// TODO(crbug.com/40890502): Avoid hardcoding sizes like this.
constexpr int kMaximizedLeftMargin = 2;

constexpr int kIconTitleSpacing = 5;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameViewWin, public:

BrowserFrameViewWin::BrowserFrameViewWin(BrowserFrame* frame,
                                         BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view) {
  // We initialize all fields despite some of them being unused in some modes,
  // since it's possible for modes to flip dynamically (e.g. if the user enables
  // a high-contrast theme). Throbber icons are only used when ShowSystemIcon()
  // is true. Everything else here is only used when
  // ShouldBrowserCustomDrawTitlebar() is true.

  if (browser_view->GetSupportsIcon()) {
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
  if (!browser_view->GetIsWebAppType() && browser_view->GetSupportsTitle()) {
    window_title_ = new views::Label(browser_view->GetWindowTitle());
    window_title_->SetSubpixelRenderingEnabled(false);
    window_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    window_title_->SetID(VIEW_ID_WINDOW_TITLE);
    AddChildView(window_title_.get());
  }

  caption_button_container_ =
      AddChildView(std::make_unique<BrowserCaptionButtonContainer>(this));
}

BrowserFrameViewWin::~BrowserFrameViewWin() = default;

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameViewWin, BrowserNonClientFrameView implementation:

bool BrowserFrameViewWin::CaptionButtonsOnLeadingEdge() const {
  // Because we don't set WS_EX_LAYOUTRTL (which would conflict with Chrome's
  // own RTL layout logic), Windows always draws the caption buttons on the
  // right, even when we want to be RTL. See crbug.com/560619.
  return !ShouldBrowserCustomDrawTitlebar(browser_view()) &&
         base::i18n::IsRTL();
}

gfx::Rect BrowserFrameViewWin::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size) const {
  const int x = CaptionButtonsOnLeadingEdge() ? CaptionButtonsRegionWidth() : 0;
  int end_x = width();
  if (!CaptionButtonsOnLeadingEdge()) {
    end_x = std::min(width() - CaptionButtonsRegionWidth(), end_x);
  }
  return gfx::Rect(x, TopAreaHeight(false), std::max(0, end_x - x),
                   tabstrip_minimum_size.height());
}

gfx::Rect BrowserFrameViewWin::GetBoundsForWebAppFrameToolbar(
    const gfx::Size& toolbar_preferred_size) const {
  int x = display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXSIZEFRAME);
  if (IsMaximized()) {
    x += kMaximizedLeftMargin;
  }
  if (browser_view()->IsWindowControlsOverlayEnabled()) {
    x = 0;
  } else if (window_icon_) {
    // Add extra padding to the left of the toolbar to account for the window
    // icon.
    x += window_icon_->size().width() + kIconTitleSpacing;
  }

  int trailing_x = width() - CaptionButtonsRegionWidth();
  return gfx::Rect(x, WindowTopY(), std::max(0, trailing_x - x),
                   caption_button_container_->size().height());
}

void BrowserFrameViewWin::LayoutWebAppWindowTitle(
    const gfx::Rect& available_space,
    views::Label& window_title_label) const {
  gfx::Rect bounds = available_space;
  // If nothing has been added to the left, match native Windows 10 UWP apps
  // that don't have window icons.
  // TODO(crbug.com/40890502): Avoid hardcoding sizes like this.
  constexpr int kMinimumTitleLeftBorderMargin = 11;
  if (bounds.x() < kMinimumTitleLeftBorderMargin) {
    bounds.SetHorizontalBounds(kMinimumTitleLeftBorderMargin, bounds.right());
  }
  window_title_label.SetSubpixelRenderingEnabled(false);
  window_title_label.SetHorizontalAlignment(gfx::ALIGN_LEFT);
  window_title_label.SetAutoColorReadabilityEnabled(false);
  window_title_label.SetBoundsRect(bounds);
}

int BrowserFrameViewWin::GetTopInset(bool restored) const {
  if (browser_view()->GetTabStripVisible() || IsWebUITabStrip()) {
    return TopAreaHeight(restored);
  }
  return ShouldBrowserCustomDrawTitlebar(browser_view())
             ? TitlebarHeight(restored)
             : 0;
}

bool BrowserFrameViewWin::HasVisibleBackgroundTabShapes(
    BrowserFrameActiveState active_state) const {
  DCHECK(GetWidget());

  // Enabling high contrast mode disables the custom-drawn titlebar (so the
  // system-drawn frame will respect the native frame colors) and enables the
  // IncreasedContrastThemeSupplier (which does not respect the native frame
  // colors).
  // TODO(pkasting): https://crbug.com/831769  Change the architecture of the
  // high contrast support to respect system colors, then remove this.
  if (GetNativeTheme()->UserHasContrastPreference()) {
    return true;
  }

  return BrowserNonClientFrameView::HasVisibleBackgroundTabShapes(active_state);
}

SkColor BrowserFrameViewWin::GetCaptionColor(
    BrowserFrameActiveState active_state) const {
  return GetColorProvider()->GetColor(ShouldPaintAsActive(active_state)
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
  gfx::Size min_size(browser_view()->GetMinimumSize());
  min_size.Enlarge(0, GetTopInset(false));

  gfx::Size titlebar_min_size(
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXSIZEFRAME) +
          CaptionButtonsRegionWidth(),
      TitlebarHeight(false));
  if (ShouldShowWindowIcon(TitlebarType::kAny)) {
    titlebar_min_size.Enlarge(
        display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXSMICON) +
            kIconTitleSpacing,
        0);
  }

  min_size.SetToMax(titlebar_min_size);

  return min_size;
}

void BrowserFrameViewWin::WindowControlsOverlayEnabledChanged() {
  caption_button_container_->OnWindowControlsOverlayEnabledChanged();
}

void BrowserFrameViewWin::PaintAsActiveChanged() {
  BrowserNonClientFrameView::PaintAsActiveChanged();

  // When window controls overlay is enabled, the caption button container is
  // painted to a layer and is not repainted by
  // BrowserNonClientFrameView::PaintAsActiveChanged. Schedule a re-paint here
  // to update the caption button colors.
  if (caption_button_container_->layer()) {
    caption_button_container_->SchedulePaint();
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameViewWin, views::NonClientFrameView implementation:

gfx::Rect BrowserFrameViewWin::GetBoundsForClientView() const {
  return client_view_bounds_;
}

gfx::Rect BrowserFrameViewWin::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  HWND hwnd = views::HWNDForWidget(frame());
  if (!browser_view()->GetTabStripVisible() && hwnd) {
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
  int super_component = BrowserNonClientFrameView::NonClientHitTest(point);
  if (super_component != HTNOWHERE) {
    return super_component;
  }

  // For app windows and popups without a custom titlebar we haven't customized
  // the frame at all so Windows can figure it out.
  if (!ShouldBrowserCustomDrawTitlebar(browser_view()) &&
      !browser_view()->GetIsNormalType()) {
    return HTNOWHERE;
  }

  // If the point isn't within our bounds, then it's in the native portion of
  // the frame so again Windows can figure it out.
  if (!bounds().Contains(point)) {
    return HTNOWHERE;
  }

  // At the window corners the resize area is not actually bigger, but the 16
  // pixels at the end of the top and bottom edges trigger diagonal resizing.
  constexpr int kResizeCornerWidth = 16;

  const int top_border_thickness = GetLayoutConstant(TAB_STRIP_PADDING);

  const int window_component = GetHTComponentForFrame(
      point, gfx::Insets::TLBR(top_border_thickness, 0, 0, 0),
      top_border_thickness, kResizeCornerWidth - FrameBorderThickness(),
      frame()->widget_delegate()->CanResize());

  const int frame_component = frame()->client_view()->NonClientHitTest(point);

  // See if we're in the sysmenu region.  We still have to check the tabstrip
  // first so that clicks in a tab don't get treated as sysmenu clicks.
  if (frame_component != HTCLIENT && ShouldShowWindowIcon(TitlebarType::kAny)) {
    gfx::Rect sys_menu_region(
        0, display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYSIZEFRAME),
        display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXSMICON),
        display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYSMICON));
    if (sys_menu_region.Contains(point)) {
      return HTSYSMENU;
    }
  }

  if (frame_component != HTNOWHERE) {
    // If the clientview  registers a hit within it's bounds, it's still
    // possible that the hit target should be top resize since the tabstrip
    // region paints to the top of the frame. If the frame registered a hit for
    // the Top resize, override the client frame target.
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

  // On Windows, the caption buttons are almost butted up to the top right
  // corner of the window. This code ensures the mouse isn't set to a size
  // cursor while hovering over the caption buttons, thus giving the incorrect
  // impression that the user can resize the window.
  RECT button_bounds = {0};
  if (SUCCEEDED(DwmGetWindowAttribute(views::HWNDForWidget(frame()),
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

  if (window_component != HTNOWHERE) {
    return window_component;
  }

  // Fall back to the caption if no other component matches.
  TabStripRegionView::ReportCaptionHitTestInReservedGrabHandleSpace(false);
  return HTCAPTION;
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
  BrowserNonClientFrameView::ResetWindowControls();
  caption_button_container_->ResetWindowControls();
}

void BrowserFrameViewWin::OnThemeChanged() {
  BrowserNonClientFrameView::OnThemeChanged();
  if (!ShouldBrowserCustomDrawTitlebar(browser_view())) {
    SetSystemMicaTitlebarAttributes();
  }
}

bool BrowserFrameViewWin::ShouldTabIconViewAnimate() const {
  if (!ShouldShowWindowIcon(TitlebarType::kCustom)) {
    return false;
  }

  // Web apps use their app icon and shouldn't show a throbber.
  if (browser_view()->GetIsWebAppType()) {
    return false;
  }

  content::WebContents* current_tab = browser_view()->GetActiveWebContents();
  return current_tab && current_tab->IsLoading();
}

ui::ImageModel BrowserFrameViewWin::GetFaviconForTabIconView() {
  DCHECK(ShouldShowWindowIcon(TitlebarType::kCustom));
  return frame()->widget_delegate()->GetWindowIcon();
}

bool BrowserFrameViewWin::IsMaximized() const {
  return frame()->IsMaximized();
}

bool BrowserFrameViewWin::IsWebUITabStrip() const {
  return WebUITabStripContainerView::UseTouchableTabStrip(
      browser_view()->browser());
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameViewWin, views::View overrides:

void BrowserFrameViewWin::OnPaint(gfx::Canvas* canvas) {
  TRACE_EVENT0("views.frame", "BrowserFrameViewWin::OnPaint");
  if (ShouldBrowserCustomDrawTitlebar(browser_view())) {
    PaintTitlebar(canvas);
  }
}

void BrowserFrameViewWin::Layout(PassKey) {
  TRACE_EVENT0("views.frame", "BrowserFrameViewWin::Layout");

  LayoutCaptionButtons();
  if (!browser_view()->IsWindowControlsOverlayEnabled()) {
    LayoutTitleBar();
  }
  LayoutClientView();
  LayoutSuperclass<BrowserNonClientFrameView>(this);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameViewWin, private:

int BrowserFrameViewWin::FrameBorderThickness() const {
  return (IsMaximized() || frame()->IsFullscreen())
             ? 0
             : display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXSIZEFRAME);
}

int BrowserFrameViewWin::FrameTopBorderThickness(bool restored) const {
  const bool is_fullscreen =
      (frame()->IsFullscreen() || IsMaximized()) && !restored;
  if (!is_fullscreen) {
    if (browser_view()->GetTabStripVisible()) {
      // Restored windows have a smaller top resize handle than the system
      // default. When maximized, the OS sizes the window such that the border
      // extends beyond the screen edges. In that case, we must return the
      // default value.
      const int kTopResizeFrameArea = 0;
      return kTopResizeFrameArea;
    }

    // There is no top border in tablet mode when the window is "restored"
    // because it is still tiled into either the left or right pane of the
    // display takes up the entire vertical extent of the screen. Note that a
    // rendering bug in Windows may still cause the very top of the window to be
    // cut off intermittently, but that's an OS issue that affects all
    // applications, not specifically Chrome.
    if (IsWebUITabStrip()) {
      return 0;
    }
  }

  // Mouse and touch locations are floored but GetSystemMetricsInDIP is rounded,
  // so we need to floor instead or else the difference will cause the hittest
  // to fail when it ought to succeed.
  return std::floor(
      FrameTopBorderThicknessPx(restored) /
      display::win::ScreenWin::GetScaleFactorForHWND(HWNDForView(this)));
}

int BrowserFrameViewWin::FrameTopBorderThicknessPx(bool restored) const {
  // Distinct from FrameBorderThickness() because we can't inset the top
  // border, otherwise Windows will give us a standard titlebar.
  // For maximized windows this is not true, and the top border must be
  // inset in order to avoid overlapping the monitor above.
  // See comments in BrowserDesktopWindowTreeHostWin::GetClientAreaInsets().
  const bool needs_no_border =
      (ShouldBrowserCustomDrawTitlebar(browser_view()) &&
       frame()->IsMaximized()) ||
      frame()->IsFullscreen();
  if (needs_no_border && !restored) {
    return 0;
  }

  // Note that this method assumes an equal resize handle thickness on all
  // sides of the window.
  // TODO(dfried): Consider having it return a gfx::Insets object instead.
  return ui::GetFrameThickness(
      MonitorFromWindow(HWNDForView(this), MONITOR_DEFAULTTONEAREST));
}

int BrowserFrameViewWin::TopAreaHeight(bool restored) const {
  if (frame()->IsFullscreen() && !restored) {
    return 0;
  }

  const bool maximized = IsMaximized() && !restored;
  int top = FrameTopBorderThickness(restored);
  if (IsWebUITabStrip()) {
    // Caption bar is default Windows size in maximized mode but full size when
    // windows are tiled in tablet mode (baesd on behavior of first-party
    // Windows applications).
    top += maximized ? TitlebarMaximizedVisualHeight()
                     : caption_button_container_->GetPreferredSize().height();
    return top;
  }

  // The tabstrip controls its own top padding.
  return top;
}

int BrowserFrameViewWin::TitlebarMaximizedVisualHeight() const {
  int maximized_height =
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYCAPTION);
  // Adding 2 dip of vertical padding puts at least 1 dip of space on the top
  // and bottom of the element.
  constexpr int kVerticalPadding = 2;
  if (!browser_view()->GetWebAppFrameToolbarPreferredSize().IsEmpty()) {
    maximized_height =
        std::max(maximized_height,
                 browser_view()->GetWebAppFrameToolbarPreferredSize().height() +
                     kVerticalPadding);
  }
  return maximized_height;
}

int BrowserFrameViewWin::TitlebarHeight(bool restored) const {
  if (frame()->IsFullscreen() && !restored) {
    return 0;
  }

  // The titlebar's actual height is the same in restored and maximized, but
  // some of it is above the screen in maximized mode. See the comment in
  // FrameTopBorderThicknessPx(). For WebUI,
  return (IsWebUITabStrip()
              ? caption_button_container_->GetPreferredSize().height()
              : TitlebarMaximizedVisualHeight()) +
         FrameTopBorderThickness(false);
}

int BrowserFrameViewWin::GetFrameHeight() const {
  if (browser_view()->GetTabStripVisible()) {
    return browser_view()->tab_strip_region_view()->GetMinimumSize().height() -
           WindowTopY() - GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP);
  }
  return IsMaximized() ? TitlebarMaximizedVisualHeight()
                       : TitlebarHeight(false);
}

int BrowserFrameViewWin::WindowTopY() const {
  // The window top is SM_CYSIZEFRAME pixels when maximized (see the comment in
  // FrameTopBorderThickness()) and floor(system dsf) pixels when restored.
  // Unfortunately we can't represent either of those at hidpi without using
  // non-integral dips, so we return the closest reasonable values instead.
  if (IsMaximized()) {
    return FrameTopBorderThickness(false);
  }
  return IsWebUITabStrip() ? FrameTopBorderThickness(true) : 1;
}

int BrowserFrameViewWin::CaptionButtonsRegionWidth() const {
  int system_caption_buttons_width =
      width() - frame()->GetMinimizeButtonOffset();

  int total_width = caption_button_container_->size().width();
  if (!ShouldBrowserCustomDrawTitlebar(browser_view())) {
    total_width += system_caption_buttons_width;
  }

  return total_width;
}

bool BrowserFrameViewWin::ShouldShowWindowIcon(TitlebarType type) const {
  if (type == TitlebarType::kCustom &&
      !ShouldBrowserCustomDrawTitlebar(browser_view())) {
    return false;
  }
  if (type == TitlebarType::kSystem &&
      ShouldBrowserCustomDrawTitlebar(browser_view())) {
    return false;
  }
  if (frame()->IsFullscreen()) {
    return false;
  }
  return browser_view()->ShouldShowWindowIcon();
}

bool BrowserFrameViewWin::ShouldShowWindowTitle(TitlebarType type) const {
  if (type == TitlebarType::kCustom &&
      !ShouldBrowserCustomDrawTitlebar(browser_view())) {
    return false;
  }
  if (type == TitlebarType::kSystem &&
      ShouldBrowserCustomDrawTitlebar(browser_view())) {
    return false;
  }
  if (frame()->IsFullscreen()) {
    return false;
  }
  return browser_view()->ShouldShowWindowTitle();
}

void BrowserFrameViewWin::TabletModeChanged() {
  if (!ShouldBrowserCustomDrawTitlebar(browser_view())) {
    SetSystemMicaTitlebarAttributes();
  }
}

void BrowserFrameViewWin::SetSystemMicaTitlebarAttributes() {
  CHECK(SystemTitlebarCanUseMicaMaterial());

  const BOOL dark_titlebar_enabled =
      frame()->GetColorMode() == ui::ColorProviderKey::ColorMode::kDark;
  DwmSetWindowAttribute(views::HWNDForWidget(frame()),
                        DWMWA_USE_IMMERSIVE_DARK_MODE, &dark_titlebar_enabled,
                        sizeof(dark_titlebar_enabled));

  const DWM_SYSTEMBACKDROP_TYPE dwm_backdrop_type =
      browser_view()->GetTabStripVisible() ? DWMSBT_TABBEDWINDOW
                                           : DWMSBT_MAINWINDOW;
  DwmSetWindowAttribute(views::HWNDForWidget(frame()),
                        DWMWA_SYSTEMBACKDROP_TYPE, &dwm_backdrop_type,
                        sizeof(dwm_backdrop_type));
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
  // BrowserDesktopWindowTreeHostWin::UpdateDWMFrame()), but this requires
  // that we leave part of the client surface transparent. If we draw this
  // ourselves, we can make the client surface fully opaque and avoid the
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

  const int titlebar_height =
      browser_view()->GetTabStripVisible()
          ? GetBoundsForTabStripRegion(
                browser_view()->tab_strip_region_view()->GetMinimumSize())
                .bottom()
          : TitlebarHeight(false);
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
  if (window_icon_) {
    window_icon_->SetVisible(show_icon);
  }
  if (window_title_) {
    window_title_->SetVisible(show_title);
  }
  if (!show_icon && !show_title) {
    return;
  }

  const int icon_size =
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYSMICON);
  const int titlebar_visual_height =
      IsMaximized() ? TitlebarMaximizedVisualHeight() : TitlebarHeight(false);
  // Don't include the area above the screen when maximized. However it only
  // looks centered if we start from y=0 when restored.
  const int window_top = IsMaximized() ? WindowTopY() : 0;
  int next_leading_x =
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXSIZEFRAME);
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
    window_title_->SetText(browser_view()->GetWindowTitle());
    const int max_text_width = std::max(0, next_trailing_x - next_leading_x);
    LayoutWebAppWindowTitle(
        gfx::Rect(next_leading_x, window_icon_bounds.y(), max_text_width,
                  window_icon_bounds.height()),
        *window_title_);
  }
}

void BrowserFrameViewWin::LayoutCaptionButtons() {
  TRACE_EVENT0("views.frame", "BrowserFrameViewWin::LayoutCaptionButtons");

  caption_button_container_->SetVisible(!frame()->IsFullscreen());

  const gfx::Size preferred_size =
      caption_button_container_->GetPreferredSize();

  const int system_caption_buttons_width =
      ShouldBrowserCustomDrawTitlebar(browser_view())
          ? 0
          : width() - frame()->GetMinimizeButtonOffset();

  caption_button_container_->SetBounds(
      CaptionButtonsOnLeadingEdge()
          ? system_caption_buttons_width
          : width() - system_caption_buttons_width - preferred_size.width(),
      WindowTopY(), preferred_size.width(), GetFrameHeight());
}

void BrowserFrameViewWin::LayoutClientView() {
  client_view_bounds_ = GetLocalBounds();
  int top_inset = GetTopInset(false);
  if (browser_view()->IsWindowControlsOverlayEnabled() ||
      !browser_view()->GetWebAppFrameToolbarPreferredSize().IsEmpty()) {
    top_inset = frame()->IsFullscreen() ? 0 : WindowTopY();
  }
  client_view_bounds_.Inset(gfx::Insets::TLBR(top_inset, 0, 0, 0));
}

void BrowserFrameViewWin::StartThrobber() {
  DCHECK(ShouldShowWindowIcon(TitlebarType::kSystem));
  if (!throbber_running_) {
    throbber_running_ = true;
    throbber_frame_ = 0;
    InitThrobberIcons();
    SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
                static_cast<WPARAM>(ICON_SMALL),
                reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
  }
}

void BrowserFrameViewWin::StopThrobber() {
  DCHECK(ShouldShowWindowIcon(TitlebarType::kSystem));
  if (throbber_running_) {
    throbber_running_ = false;

    base::win::ScopedHICON previous_small_icon;
    base::win::ScopedHICON previous_big_icon;
    HICON small_icon = nullptr;
    HICON big_icon = nullptr;

    gfx::ImageSkia icon =
        browser_view()->GetWindowIcon().Rasterize(GetColorProvider());

    if (!icon.isNull()) {
      // Keep previous icons alive as long as they are referenced by the HWND.
      previous_small_icon = std::move(small_window_icon_);
      previous_big_icon = std::move(big_window_icon_);

      // Take responsibility for eventually destroying the created icons.
      small_window_icon_ = CreateHICONFromSkBitmapSizedTo(
          icon, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
      big_window_icon_ = CreateHICONFromSkBitmapSizedTo(
          icon, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));

      small_icon = small_window_icon_.get();
      big_icon = big_window_icon_.get();
    }

    // Fallback to class icon.
    if (!small_icon) {
      small_icon = reinterpret_cast<HICON>(
          GetClassLongPtr(views::HWNDForWidget(frame()), GCLP_HICONSM));
    }
    if (!big_icon) {
      big_icon = reinterpret_cast<HICON>(
          GetClassLongPtr(views::HWNDForWidget(frame()), GCLP_HICON));
    }

    // This will reset the icon which we set in the throbber code.
    // WM_SETICON with null icon restores the icon for title bar but not
    // for taskbar. See http://crbug.com/29996
    SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
                static_cast<WPARAM>(ICON_SMALL),
                reinterpret_cast<LPARAM>(small_icon));

    SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
                static_cast<WPARAM>(ICON_BIG),
                reinterpret_cast<LPARAM>(big_icon));
  }
}

void BrowserFrameViewWin::DisplayNextThrobberFrame() {
  throbber_frame_ = (throbber_frame_ + 1) % kThrobberIconCount;
  SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
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
