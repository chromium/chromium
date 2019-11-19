// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"

#include <dwmapi.h>
#include <utility>

#include "base/trace_event/common/trace_event_common.h"
#include "base/win/windows_version.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/win/titlebar_config.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle_win.h"
#include "ui/base/theme_provider.h"
#include "ui/base/win/hwnd_metrics.h"
#include "ui/display/win/dpi.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/win/hwnd_util.h"
#include "ui/views/window/client_view.h"

HICON GlassBrowserFrameView::throbber_icons_[
    GlassBrowserFrameView::kThrobberIconCount];

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

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, public:

constexpr char GlassBrowserFrameView::kClassName[];

SkColor GlassBrowserFrameView::GetReadableFeatureColor(
    SkColor background_color) {
  // color_utils::GetColorWithMaxContrast()/IsDark() aren't used here because
  // they switch based on the Chrome light/dark endpoints, while we want to use
  // the system native behavior below.
  const auto windows_luma = [](SkColor c) {
    return 0.25f * SkColorGetR(c) + 0.625f * SkColorGetG(c) +
           0.125f * SkColorGetB(c);
  };
  return windows_luma(background_color) <= 128.0f ? SK_ColorWHITE
                                                  : SK_ColorBLACK;
}

GlassBrowserFrameView::GlassBrowserFrameView(BrowserFrame* frame,
                                             BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      window_icon_(nullptr),
      window_title_(nullptr),
      minimize_button_(nullptr),
      maximize_button_(nullptr),
      restore_button_(nullptr),
      close_button_(nullptr),
      throbber_running_(false),
      throbber_frame_(0) {
  // We initialize all fields despite some of them being unused in some modes,
  // since it's possible for modes to flip dynamically (e.g. if the user enables
  // a high-contrast theme). Throbber icons are only used when ShowSystemIcon()
  // is true. Everything else here is only used when
  // ShouldCustomDrawSystemTitlebar() is true.

  if (browser_view->ShouldShowWindowIcon()) {
    InitThrobberIcons();

    window_icon_ = new TabIconView(this, nullptr);
    window_icon_->set_is_light(true);
    window_icon_->SetID(VIEW_ID_WINDOW_ICON);
    // Stop the icon from intercepting clicks intended for the HTSYSMENU region
    // of the window. Even though it does nothing on click, it will still
    // prevent us from giving the event back to Windows to handle properly.
    window_icon_->set_can_process_events_within_subtree(false);
    AddChildView(window_icon_);
  }

  if (browser_view->ShouldShowWindowTitle()) {
    window_title_ = new views::Label(browser_view->GetWindowTitle());
    window_title_->SetSubpixelRenderingEnabled(false);
    window_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    window_title_->SetID(VIEW_ID_WINDOW_TITLE);
    AddChildView(window_title_);
  }

  web_app::AppBrowserController* controller =
      browser_view->browser()->app_controller();
  if (controller && controller->HasTitlebarToolbar()) {
    // TODO(alancutter): Avoid snapshotting GetCaptionColor() values here and
    // call it on demand in WebAppFrameToolbarView::UpdateIconsColor() via a
    // delegate interface.
    set_web_app_frame_toolbar(
        AddChildView(std::make_unique<WebAppFrameToolbarView>(
            frame, browser_view,
            GetCaptionColor(BrowserFrameActiveState::kActive),
            GetCaptionColor(BrowserFrameActiveState::kInactive))));
  }

  minimize_button_ =
      CreateCaptionButton(VIEW_ID_MINIMIZE_BUTTON, IDS_APP_ACCNAME_MINIMIZE);
  maximize_button_ =
      CreateCaptionButton(VIEW_ID_MAXIMIZE_BUTTON, IDS_APP_ACCNAME_MAXIMIZE);
  restore_button_ =
      CreateCaptionButton(VIEW_ID_RESTORE_BUTTON, IDS_APP_ACCNAME_RESTORE);
  close_button_ =
      CreateCaptionButton(VIEW_ID_CLOSE_BUTTON, IDS_APP_ACCNAME_CLOSE);

  // Because currently focus mode uses a vertically-expanded titlebar, there is
  // no need to add extra space for a grab handle. However, traditional PWA and
  // full browser mode require the extra space when the window is not maximized.
  constexpr int kTopResizeFrameArea = 5;
  drag_handle_padding_ =
      browser_view->browser()->is_focus_mode() ? 0 : kTopResizeFrameArea;
}

GlassBrowserFrameView::~GlassBrowserFrameView() {
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, BrowserNonClientFrameView implementation:

bool GlassBrowserFrameView::CaptionButtonsOnLeadingEdge() const {
  // Because we don't set WS_EX_LAYOUTRTL (which would conflict with Chrome's
  // own RTL layout logic), Windows always draws the caption buttons on the
  // right, even when we want to be RTL. See crbug.com/560619.
  return !ShouldCustomDrawSystemTitlebar() && base::i18n::IsRTL();
}

gfx::Rect GlassBrowserFrameView::GetBoundsForTabStripRegion(
    const views::View* tabstrip) const {
  const int x = CaptionButtonsOnLeadingEdge()
                    ? (width() - frame()->GetMinimizeButtonOffset())
                    : 0;
  int end_x = width();
  if (!CaptionButtonsOnLeadingEdge())
    end_x = std::min(MinimizeButtonX(), end_x);
  return gfx::Rect(x, TopAreaHeight(false), std::max(0, end_x - x),
                   tabstrip->GetPreferredSize().height());
}

int GlassBrowserFrameView::GetTopInset(bool restored) const {
  if (browser_view()->IsTabStripVisible())
    return TopAreaHeight(restored);
  return ShouldCustomDrawSystemTitlebar() ? TitlebarHeight(restored) : 0;
}

int GlassBrowserFrameView::GetThemeBackgroundXInset() const {
  return 0;
}

bool GlassBrowserFrameView::HasVisibleBackgroundTabShapes(
    BrowserFrameActiveState active_state) const {
  // Pre-Win 8, tabs never match the glass frame appearance.
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return true;

  // Enabling high contrast mode disables the custom-drawn titlebar (so the
  // system-drawn frame will respect the native frame colors) and enables the
  // IncreasedContrastThemeSupplier (which does not respect the native frame
  // colors).
  // TODO(pkasting): https://crbug.com/831769  Change the architecture of the
  // high contrast support to respect system colors, then remove this.
  if (ui::NativeTheme::GetInstanceForNativeUi()->UsesHighContrastColors())
    return true;

  return BrowserNonClientFrameView::HasVisibleBackgroundTabShapes(active_state);
}

bool GlassBrowserFrameView::CanDrawStrokes() const {
  // On Win 7, the tabs are drawn as flat shapes against the glass frame, so
  // the active tab always has a visible shape and strokes are unnecessary.
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return false;

  return BrowserNonClientFrameView::CanDrawStrokes();
}

SkColor GlassBrowserFrameView::GetCaptionColor(
    BrowserFrameActiveState active_state) const {
  const SkAlpha title_alpha = ShouldPaintAsActive(active_state)
                                  ? SK_AlphaOPAQUE
                                  : kInactiveTitlebarFeatureAlpha;
  return SkColorSetA(GetReadableFeatureColor(GetFrameColor(active_state)),
                     title_alpha);
}

void GlassBrowserFrameView::UpdateThrobber(bool running) {
  if (ShowCustomIcon())
    window_icon_->Update();

  if (!ShowSystemIcon())
    return;

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

gfx::Size GlassBrowserFrameView::GetMinimumSize() const {
  gfx::Size min_size(browser_view()->GetMinimumSize());
  min_size.Enlarge(0, GetTopInset(false));

  return min_size;
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, views::NonClientFrameView implementation:

gfx::Rect GlassBrowserFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

gfx::Rect GlassBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  HWND hwnd = views::HWNDForWidget(frame());
  if (!browser_view()->IsTabStripVisible() && hwnd) {
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

namespace {

bool HitTestCaptionButton(Windows10CaptionButton* button,
                          const gfx::Point& point) {
  return button && button->GetVisible() &&
         button->GetMirroredBounds().Contains(point);
}

}  // namespace

int GlassBrowserFrameView::NonClientHitTest(const gfx::Point& point) {
  int super_component = BrowserNonClientFrameView::NonClientHitTest(point);
  if (super_component != HTNOWHERE)
    return super_component;

  // For app windows and popups without a custom titlebar we haven't customized
  // the frame at all so Windows can figure it out.
  if (!ShouldCustomDrawSystemTitlebar() &&
      !browser_view()->IsBrowserTypeNormal())
    return HTNOWHERE;

  // If the point isn't within our bounds, then it's in the native portion of
  // the frame so again Windows can figure it out.
  if (!bounds().Contains(point))
    return HTNOWHERE;

  int frame_component = frame()->client_view()->NonClientHitTest(point);

  // See if we're in the sysmenu region.  We still have to check the tabstrip
  // first so that clicks in a tab don't get treated as sysmenu clicks.
  if (browser_view()->ShouldShowWindowIcon() && frame_component != HTCLIENT) {
    gfx::Rect sys_menu_region(
        0, display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYSIZEFRAME),
        display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXSMICON),
        display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYSMICON));
    if (sys_menu_region.Contains(point))
      return HTSYSMENU;
  }

  if (frame_component != HTNOWHERE)
    return frame_component;

  // Then see if the point is within any of the window controls.
  if (HitTestCaptionButton(minimize_button_, point))
    return HTMINBUTTON;
  if (HitTestCaptionButton(maximize_button_, point))
    return HTMAXBUTTON;
  if (HitTestCaptionButton(restore_button_, point))
    return HTMAXBUTTON;
  if (HitTestCaptionButton(close_button_, point))
    return HTCLOSE;

  // On Windows 8+, the caption buttons are almost butted up to the top right
  // corner of the window. This code ensures the mouse isn't set to a size
  // cursor while hovering over the caption buttons, thus giving the incorrect
  // impression that the user can resize the window.
  if (base::win::GetVersion() >= base::win::Version::WIN8) {
    RECT button_bounds = {0};
    if (SUCCEEDED(DwmGetWindowAttribute(views::HWNDForWidget(frame()),
                                        DWMWA_CAPTION_BUTTON_BOUNDS,
                                        &button_bounds,
                                        sizeof(button_bounds)))) {
      gfx::Rect buttons = gfx::ConvertRectToDIP(display::win::GetDPIScale(),
                                                gfx::Rect(button_bounds));

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
      buttons.Inset(0, kCaptionButtonTopInset, 0, 0);
      if (buttons.Contains(point))
        return HTNOWHERE;
    }
  }

  int top_border_thickness = FrameTopBorderThickness(false);
  // At the window corners the resize area is not actually bigger, but the 16
  // pixels at the end of the top and bottom edges trigger diagonal resizing.
  constexpr int kResizeCornerWidth = 16;
  int window_component = GetHTComponentForFrame(
      point, top_border_thickness, 0, top_border_thickness,
      kResizeCornerWidth - FrameBorderThickness(),
      frame()->widget_delegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void GlassBrowserFrameView::UpdateWindowIcon() {
  if (ShowCustomIcon() && !frame()->IsFullscreen())
    window_icon_->SchedulePaint();
}

void GlassBrowserFrameView::UpdateWindowTitle() {
  if (ShowCustomTitle() && !frame()->IsFullscreen()) {
    LayoutTitleBar();
    window_title_->SchedulePaint();
  }
}

void GlassBrowserFrameView::ResetWindowControls() {
  BrowserNonClientFrameView::ResetWindowControls();
  minimize_button_->SetState(views::Button::STATE_NORMAL);
  maximize_button_->SetState(views::Button::STATE_NORMAL);
  restore_button_->SetState(views::Button::STATE_NORMAL);
  close_button_->SetState(views::Button::STATE_NORMAL);
}

void GlassBrowserFrameView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (sender == minimize_button_)
    frame()->Minimize();
  else if (sender == maximize_button_)
    frame()->Maximize();
  else if (sender == restore_button_)
    frame()->Restore();
  else if (sender == close_button_)
    frame()->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
}

bool GlassBrowserFrameView::ShouldTabIconViewAnimate() const {
  DCHECK(ShowCustomIcon());
  content::WebContents* current_tab = browser_view()->GetActiveWebContents();
  return current_tab && current_tab->IsLoading();
}

gfx::ImageSkia GlassBrowserFrameView::GetFaviconForTabIconView() {
  DCHECK(ShowCustomIcon());
  return frame()->widget_delegate()->GetWindowIcon();
}

bool GlassBrowserFrameView::IsMaximized() const {
  return frame()->IsMaximized();
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, views::View overrides:

const char* GlassBrowserFrameView::GetClassName() const {
  return kClassName;
}

void GlassBrowserFrameView::OnPaint(gfx::Canvas* canvas) {
  TRACE_EVENT0("views.frame", "GlassBrowserFrameView::OnPaint");
  if (ShouldCustomDrawSystemTitlebar())
    PaintTitlebar(canvas);
}

void GlassBrowserFrameView::Layout() {
  TRACE_EVENT0("views.frame", "GlassBrowserFrameView::Layout");
  if (ShouldCustomDrawSystemTitlebar())
    LayoutCaptionButtons();

  if (ShouldCustomDrawSystemTitlebar())
    LayoutTitleBar();

  LayoutClientView();
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, private:

int GlassBrowserFrameView::FrameBorderThickness() const {
  return (IsMaximized() || frame()->IsFullscreen())
             ? 0
             : display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXSIZEFRAME);
}

int GlassBrowserFrameView::FrameTopBorderThickness(bool restored) const {
  // Restored windows have a smaller top resize handle than the system default.
  // When maximized, the OS sizes the window such that the border extends beyond
  // the screen edges. In that case, we must return the default value.
  if (browser_view()->IsTabStripVisible() &&
      ((!frame()->IsFullscreen() && !IsMaximized()) || restored)) {
    return drag_handle_padding_;
  }

  // Mouse and touch locations are floored but GetSystemMetricsInDIP is rounded,
  // so we need to floor instead or else the difference will cause the hittest
  // to fail when it ought to succeed.
  return std::floor(
      FrameTopBorderThicknessPx(restored) /
      display::win::ScreenWin::GetScaleFactorForHWND(HWNDForView(this)));
}

int GlassBrowserFrameView::FrameTopBorderThicknessPx(bool restored) const {
  // Distinct from FrameBorderThickness() because Windows gives maximized
  // windows an offscreen region around the edges. The left/right/bottom edges
  // don't worry about this because we cancel them out in
  // BrowserDesktopWindowTreeHostWin::GetClientAreaInsets() so the offscreen
  // area is non-client as far as Windows is concerned. However we can't do this
  // with the top inset because otherwise Windows will give us a standard
  // titlebar. Thus we must compensate here to avoid having UI elements drift
  // off the top of the screen.
  if (frame()->IsFullscreen() && !restored)
    return 0;

  // Note that this method assumes an equal resize handle thickness on all
  // sides of the window.
  // TODO(dfried): Consider having it return a gfx::Insets object instead.
  return ui::GetFrameThickness(
      MonitorFromWindow(HWNDForView(this), MONITOR_DEFAULTTONEAREST));
}

int GlassBrowserFrameView::TopAreaHeight(bool restored) const {
  if (frame()->IsFullscreen() && !restored)
    return 0;

  int top = FrameTopBorderThickness(restored);
  if (IsMaximized() && !restored)
    return top;

  // Besides the frame border, there's empty space atop the window in restored
  // mode, to use to drag the window around.
  constexpr int kNonClientRestoredExtraThickness = 4;
  int thickness = kNonClientRestoredExtraThickness;
  if (EverHasVisibleBackgroundTabShapes()) {
    thickness =
        std::max(thickness, BrowserNonClientFrameView::kMinimumDragHeight);
  }
  return top + thickness;
}

int GlassBrowserFrameView::TitlebarMaximizedVisualHeight() const {
  int maximized_height =
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYCAPTION);
  if (web_app_frame_toolbar()) {
    // Adding 2px of vertical padding puts at least 1 px of space on the top and
    // bottom of the element.
    constexpr int kVerticalPadding = 2;
    maximized_height = std::max(
        maximized_height, web_app_frame_toolbar()->GetPreferredSize().height() +
                              kVerticalPadding);
  }
  return maximized_height;
}

int GlassBrowserFrameView::TitlebarHeight(bool restored) const {
  if (frame()->IsFullscreen() && !restored)
    return 0;
  // The titlebar's actual height is the same in restored and maximized, but
  // some of it is above the screen in maximized mode. See the comment in
  // FrameTopBorderThicknessPx().
  return TitlebarMaximizedVisualHeight() + FrameTopBorderThickness(false);
}

int GlassBrowserFrameView::WindowTopY() const {
  // The window top is SM_CYSIZEFRAME pixels when maximized (see the comment in
  // FrameTopBorderThickness()) and floor(system dsf) pixels when restored.
  // Unfortunately we can't represent either of those at hidpi without using
  // non-integral dips, so we return the closest reasonable values instead.
  return IsMaximized() ? FrameTopBorderThickness(false) : 1;
}

int GlassBrowserFrameView::MinimizeButtonX() const {
  // When CaptionButtonsOnLeadingEdge() is true call
  // frame()->GetMinimizeButtonOffset() directly, because minimize_button_->x()
  // will give the wrong edge of the button.
  DCHECK(!CaptionButtonsOnLeadingEdge());
  // If we're drawing the button we can query the layout directly, otherwise we
  // need to ask Windows where the minimize button is.
  // TODO(bsep): Ideally these would always be the same. When we're always
  // custom drawing the caption buttons, remove GetMinimizeButtonOffset().
  return ShouldCustomDrawSystemTitlebar() ? minimize_button_->x()
                                          : frame()->GetMinimizeButtonOffset();
}

bool GlassBrowserFrameView::IsToolbarVisible() const {
  return browser_view()->IsToolbarVisible() &&
      !browser_view()->toolbar()->GetPreferredSize().IsEmpty();
}

bool GlassBrowserFrameView::ShowCustomIcon() const {
  // Web-app windows don't include the window icon as per UI mocks.
  return !web_app_frame_toolbar() && ShouldCustomDrawSystemTitlebar() &&
         browser_view()->ShouldShowWindowIcon();
}

bool GlassBrowserFrameView::ShowCustomTitle() const {
  return ShouldCustomDrawSystemTitlebar() &&
         browser_view()->ShouldShowWindowTitle();
}

bool GlassBrowserFrameView::ShowSystemIcon() const {
  return !ShouldCustomDrawSystemTitlebar() &&
         browser_view()->ShouldShowWindowIcon();
}

SkColor GlassBrowserFrameView::GetTitlebarColor() const {
  return GetFrameColor();
}

Windows10CaptionButton* GlassBrowserFrameView::CreateCaptionButton(
    ViewID button_type,
    int accessible_name_resource_id) {
  Windows10CaptionButton* button = new Windows10CaptionButton(
      this, button_type,
      l10n_util::GetStringUTF16(accessible_name_resource_id));
  AddChildView(button);
  return button;
}

void GlassBrowserFrameView::PaintTitlebar(gfx::Canvas* canvas) const {
  TRACE_EVENT0("views.frame", "GlassBrowserFrameView::PaintTitlebar");

  cc::PaintFlags flags;
  gfx::ScopedCanvas scoped_canvas(canvas);
  float scale = canvas->UndoDeviceScaleFactor();
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
  // So the accent border also has to be opaque. Native inactive borders are
  // #555555 with 50% alpha. We can blend the titlebar color with this to
  // approximate the native effect.
  const SkColor titlebar_color = GetTitlebarColor();
  flags.setColor(
      ShouldPaintAsActive()
          ? GetThemeProvider()->GetColor(ThemeProperties::COLOR_ACCENT_BORDER)
          : color_utils::AlphaBlend(SkColorSetRGB(0x55, 0x55, 0x55),
                                    titlebar_color, 0.5f));
  canvas->DrawRect(gfx::RectF(0, 0, width() * scale, y), flags);

  const int titlebar_height =
      browser_view()->IsTabStripVisible()
          ? GetBoundsForTabStripRegion(browser_view()->tabstrip()).bottom()
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

  if (ShowCustomTitle())
    window_title_->SetEnabledColor(
        GetCaptionColor(BrowserFrameActiveState::kUseCurrent));
}

void GlassBrowserFrameView::LayoutTitleBar() {
  TRACE_EVENT0("views.frame", "GlassBrowserFrameView::LayoutTitleBar");
  if (!ShowCustomIcon() && !ShowCustomTitle())
    return;

  gfx::Rect window_icon_bounds;
  const int icon_size =
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYSMICON);
  const int titlebar_visual_height =
      IsMaximized() ? TitlebarMaximizedVisualHeight() : TitlebarHeight(false);
  // Don't include the area above the screen when maximized. However it only
  // looks centered if we start from y=0 when restored.
  const int window_top = IsMaximized() ? WindowTopY() : 0;
  int next_leading_x =
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXSIZEFRAME);
  constexpr int kMaximizedLeftMargin = 2;
  if (IsMaximized())
    next_leading_x += kMaximizedLeftMargin;
  int next_trailing_x = MinimizeButtonX();

  const int y = window_top + (titlebar_visual_height - icon_size) / 2;
  window_icon_bounds = gfx::Rect(next_leading_x, y, icon_size, icon_size);

  constexpr int kIconTitleSpacing = 5;
  if (ShowCustomIcon()) {
    window_icon_->SetBoundsRect(window_icon_bounds);
    next_leading_x = window_icon_bounds.right() + kIconTitleSpacing;
  }

  if (web_app_frame_toolbar()) {
    std::pair<int, int> remaining_bounds =
        web_app_frame_toolbar()->LayoutInContainer(next_leading_x,
                                                   next_trailing_x, window_top,
                                                   titlebar_visual_height);
    next_leading_x = remaining_bounds.first;
    next_trailing_x = remaining_bounds.second;
  }

  if (ShowCustomTitle()) {
    // If nothing has been added to the left, match native Windows 10 UWP apps
    // that don't have window icons.
    constexpr int kMinimumTitleLeftBorderMargin = 11;
    next_leading_x = std::max(next_leading_x, kMinimumTitleLeftBorderMargin);

    window_title_->SetText(browser_view()->GetWindowTitle());
    const int max_text_width = std::max(0, next_trailing_x - next_leading_x);
    window_title_->SetBounds(next_leading_x, window_icon_bounds.y(),
                             max_text_width, window_icon_bounds.height());
    window_title_->SetAutoColorReadabilityEnabled(false);
  }
}

void GlassBrowserFrameView::LayoutCaptionButton(Windows10CaptionButton* button,
                                                int previous_button_x) {
  TRACE_EVENT0("views.frame", "GlassBrowserFrameView::LayoutCaptionButton");
  gfx::Size button_size = button->GetPreferredSize();
  button->SetBounds(previous_button_x - button_size.width(), WindowTopY(),
                    button_size.width(), button_size.height());
}

void GlassBrowserFrameView::LayoutCaptionButtons() {
  TRACE_EVENT0("views.frame", "GlassBrowserFrameView::LayoutCaptionButtons");
  LayoutCaptionButton(close_button_, width());

  LayoutCaptionButton(restore_button_, close_button_->x());
  restore_button_->SetVisible(IsMaximized());

  LayoutCaptionButton(maximize_button_, close_button_->x());
  maximize_button_->SetVisible(!IsMaximized());

  LayoutCaptionButton(minimize_button_, maximize_button_->x());
}

void GlassBrowserFrameView::LayoutClientView() {
  client_view_bounds_ = GetLocalBounds();
  client_view_bounds_.Inset(0, GetTopInset(false), 0, 0);
}

void GlassBrowserFrameView::StartThrobber() {
  DCHECK(ShowSystemIcon());
  if (!throbber_running_) {
    throbber_running_ = true;
    throbber_frame_ = 0;
    InitThrobberIcons();
    SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
                static_cast<WPARAM>(ICON_SMALL),
                reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
  }
}

void GlassBrowserFrameView::StopThrobber() {
  DCHECK(ShowSystemIcon());
  if (throbber_running_) {
    throbber_running_ = false;

    base::win::ScopedHICON previous_small_icon;
    base::win::ScopedHICON previous_big_icon;
    HICON small_icon = nullptr;
    HICON big_icon = nullptr;

    gfx::ImageSkia icon = browser_view()->GetWindowIcon();
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

void GlassBrowserFrameView::DisplayNextThrobberFrame() {
  throbber_frame_ = (throbber_frame_ + 1) % kThrobberIconCount;
  SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
              static_cast<WPARAM>(ICON_SMALL),
              reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
}

// static
void GlassBrowserFrameView::InitThrobberIcons() {
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
