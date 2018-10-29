// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_win.h"

#include <dwmapi.h>

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/win/windows_version.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_window_property_manager_win.h"
#include "chrome/browser/ui/views/frame/system_menu_insertion_delegate_win.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/win/titlebar_config.h"
#include "chrome/common/chrome_constants.h"
#include "ui/base/theme_provider.h"
#include "ui/base/win/hwnd_metrics.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/menu/native_menu_win.h"

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostWin, public:

BrowserDesktopWindowTreeHostWin::BrowserDesktopWindowTreeHostWin(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame)
    : DesktopWindowTreeHostWin(native_widget_delegate,
                               desktop_native_widget_aura),
      browser_view_(browser_view),
      browser_frame_(browser_frame),
      did_gdi_clear_(false) {
}

BrowserDesktopWindowTreeHostWin::~BrowserDesktopWindowTreeHostWin() {
}

views::NativeMenuWin* BrowserDesktopWindowTreeHostWin::GetSystemMenu() {
  if (!system_menu_.get()) {
    SystemMenuInsertionDelegateWin insertion_delegate;
    system_menu_.reset(
        new views::NativeMenuWin(browser_frame_->GetSystemMenuModel(),
                                 GetHWND()));
    system_menu_->Rebuild(&insertion_delegate);
  }
  return system_menu_.get();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostWin, BrowserDesktopWindowTreeHost implementation:

views::DesktopWindowTreeHost*
    BrowserDesktopWindowTreeHostWin::AsDesktopWindowTreeHost() {
  return this;
}

int BrowserDesktopWindowTreeHostWin::GetMinimizeButtonOffset() const {
  return minimize_button_metrics_.GetMinimizeButtonOffsetX();
}

bool BrowserDesktopWindowTreeHostWin::UsesNativeSystemMenu() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostWin, views::DesktopWindowTreeHostWin overrides:

int BrowserDesktopWindowTreeHostWin::GetInitialShowState() const {
  STARTUPINFO si = {0};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  GetStartupInfo(&si);
  return si.wShowWindow;
}

bool BrowserDesktopWindowTreeHostWin::GetClientAreaInsets(
    gfx::Insets* insets,
    HMONITOR monitor) const {
  // Always use default insets for opaque frame.
  if (!ShouldUseNativeFrame())
    return false;

  // Use default insets for popups and apps, unless we are custom drawing the
  // titlebar.
  if (!ShouldCustomDrawSystemTitlebar() &&
      !browser_view_->IsBrowserTypeNormal())
    return false;

  if (GetWidget()->IsFullscreen()) {
    // In fullscreen mode there is no frame.
    *insets = gfx::Insets();
  } else {
    const int frame_thickness = ui::GetFrameThickness(monitor);
    // Reduce the Windows non-client border size because we extend the border
    // into our client area in UpdateDWMFrame(). The top inset must be 0 or
    // else Windows will draw a full native titlebar outside the client area.
    *insets = gfx::Insets(0, frame_thickness, frame_thickness, frame_thickness);
  }
  return true;
}

void BrowserDesktopWindowTreeHostWin::HandleCreate() {
  DesktopWindowTreeHostWin::HandleCreate();
  browser_window_property_manager_ =
      BrowserWindowPropertyManager::CreateBrowserWindowPropertyManager(
          browser_view_, GetHWND());
}

void BrowserDesktopWindowTreeHostWin::HandleDestroying() {
  browser_window_property_manager_.reset();
  DesktopWindowTreeHostWin::HandleDestroying();
}

void BrowserDesktopWindowTreeHostWin::HandleFrameChanged() {
  // Reinitialize the status bubble, since it needs to be initialized
  // differently depending on whether or not DWM composition is enabled
  browser_view_->InitStatusBubble();

  // We need to update the glass region on or off before the base class adjusts
  // the window region.
  UpdateDWMFrame();
  DesktopWindowTreeHostWin::HandleFrameChanged();
}

void BrowserDesktopWindowTreeHostWin::HandleWindowScaleFactorChanged(
    float window_scale_factor) {
  DesktopWindowTreeHostWin::HandleWindowScaleFactorChanged(window_scale_factor);
  minimize_button_metrics_.OnDpiChanged();
}

bool BrowserDesktopWindowTreeHostWin::PreHandleMSG(UINT message,
                                                   WPARAM w_param,
                                                   LPARAM l_param,
                                                   LRESULT* result) {
  switch (message) {
    case WM_ACTIVATE:
      if (LOWORD(w_param) != WA_INACTIVE)
        minimize_button_metrics_.OnHWNDActivated();
      return false;
    case WM_ENDSESSION:
      chrome::SessionEnding();
      return true;
    case WM_INITMENUPOPUP:
      GetSystemMenu()->UpdateStates();
      return true;
  }
  return DesktopWindowTreeHostWin::PreHandleMSG(
      message, w_param, l_param, result);
}

void BrowserDesktopWindowTreeHostWin::PostHandleMSG(UINT message,
                                                    WPARAM w_param,
                                                    LPARAM l_param) {
  HWND hwnd = GetHWND();
  switch (message) {
    case WM_CREATE:
      minimize_button_metrics_.Init(hwnd);
      break;
    case WM_WINDOWPOSCHANGED: {
      UpdateDWMFrame();

      // Windows lies to us about the position of the minimize button before a
      // window is visible. We use this position to place the incognito avatar
      // in RTL mode, so when the window is shown, we need to re-layout and
      // schedule a paint for the non-client frame view so that the icon top has
      // the correct position when the window becomes visible. This fixes bugs
      // where the icon appears to overlay the minimize button. Note that we
      // will call Layout every time SetWindowPos is called with SWP_SHOWWINDOW,
      // however callers typically are careful about not specifying this flag
      // unless necessary to avoid flicker. This may be invoked during creation
      // on XP and before the non_client_view has been created.
      WINDOWPOS* window_pos = reinterpret_cast<WINDOWPOS*>(l_param);
      views::NonClientView* non_client_view = GetWidget()->non_client_view();
      if (window_pos->flags & SWP_SHOWWINDOW && non_client_view) {
        non_client_view->Layout();
        non_client_view->SchedulePaint();
      }
      break;
    }
    case WM_ERASEBKGND: {
      gfx::Insets insets;
      if (!did_gdi_clear_ &&
          GetClientAreaInsets(
              &insets, MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST))) {
        // This is necessary to avoid white flashing in the titlebar area around
        // the minimize/maximize/close buttons.
        DCHECK_EQ(0, insets.top());
        HDC dc = GetDC(hwnd);
        MARGINS margins = GetDWMFrameMargins();
        RECT client_rect;
        GetClientRect(hwnd, &client_rect);
        HBRUSH brush = CreateSolidBrush(0);
        RECT rect = {0, 0, client_rect.right, margins.cyTopHeight};
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
        ReleaseDC(hwnd, dc);
        did_gdi_clear_ = true;
      }
      break;
    }
    case WM_DWMCOLORIZATIONCOLORCHANGED: {
      // The activation border may have changed color.
      views::NonClientView* non_client_view = GetWidget()->non_client_view();
      if (non_client_view)
        non_client_view->SchedulePaint();
      break;
    }
  }
}

views::FrameMode BrowserDesktopWindowTreeHostWin::GetFrameMode() const {
  if (IsOpaqueHostedAppFrame())
    return views::FrameMode::CUSTOM_DRAWN;

  const views::FrameMode system_frame_mode =
      ShouldCustomDrawSystemTitlebar()
          ? views::FrameMode::SYSTEM_DRAWN_NO_CONTROLS
          : views::FrameMode::SYSTEM_DRAWN;

  // We don't theme popup or app windows, so regardless of whether or not a
  // theme is active for normal browser windows, we don't want to use the custom
  // frame for popups/apps.
  if (!browser_view_->IsBrowserTypeNormal() &&
      DesktopWindowTreeHostWin::GetFrameMode() ==
          views::FrameMode::SYSTEM_DRAWN) {
    return system_frame_mode;
  }

  // Otherwise, we use the native frame when we're told we should by the theme
  // provider (e.g. no custom theme is active).
  return GetWidget()->GetThemeProvider()->ShouldUseNativeFrame()
             ? system_frame_mode
             : views::FrameMode::CUSTOM_DRAWN;
}

bool BrowserDesktopWindowTreeHostWin::ShouldUseNativeFrame() const {
  if (!views::DesktopWindowTreeHostWin::ShouldUseNativeFrame())
    return false;
  // This function can get called when the Browser window is closed i.e. in the
  // context of the BrowserView destructor.
  if (!browser_view_->browser())
    return false;

  if (IsOpaqueHostedAppFrame())
    return false;

  // We don't theme popup or app windows, so regardless of whether or not a
  // theme is active for normal browser windows, we don't want to use the custom
  // frame for popups/apps.
  if (!browser_view_->IsBrowserTypeNormal())
    return true;
  // Otherwise, we use the native frame when we're told we should by the theme
  // provider (e.g. no custom theme is active).
  return GetWidget()->GetThemeProvider()->ShouldUseNativeFrame();
}

bool BrowserDesktopWindowTreeHostWin::ShouldWindowContentsBeTransparent()
    const {
  return !ShouldCustomDrawSystemTitlebar() &&
         views::DesktopWindowTreeHostWin::ShouldWindowContentsBeTransparent();
}

void BrowserDesktopWindowTreeHostWin::FrameTypeChanged() {
  views::DesktopWindowTreeHostWin::FrameTypeChanged();
  did_gdi_clear_ = false;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostWin, private:

void BrowserDesktopWindowTreeHostWin::UpdateDWMFrame() {
  // For "normal" windows on Aero, we always need to reset the glass area
  // correctly, even if we're not currently showing the native frame (e.g.
  // because a theme is showing), so we explicitly check for that case rather
  // than checking browser_frame_->ShouldUseNativeFrame() here.  Using that here
  // would mean we wouldn't reset the glass area to zero when moving from the
  // native frame to an opaque frame, leading to graphical glitches behind the
  // opaque frame.  Instead, we use that function below to tell us whether the
  // frame is currently native or opaque.
  if (!GetWidget()->client_view() || !browser_view_->IsBrowserTypeNormal() ||
      !DesktopWindowTreeHostWin::ShouldUseNativeFrame())
    return;

  MARGINS margins = GetDWMFrameMargins();

  DwmExtendFrameIntoClientArea(GetHWND(), &margins);
}

MARGINS BrowserDesktopWindowTreeHostWin::GetDWMFrameMargins() const {
  // Don't extend the glass in at all if it won't be visible.
  if (!ShouldUseNativeFrame() || GetWidget()->IsFullscreen() ||
      ShouldCustomDrawSystemTitlebar())
    return MARGINS{0};

  // The glass should extend to the bottom of the tabstrip.
  HWND hwnd = GetHWND();
  gfx::Rect tabstrip_bounds(
      browser_frame_->GetBoundsForTabStrip(browser_view_->tabstrip()));
  tabstrip_bounds =
      display::win::ScreenWin::DIPToClientRect(hwnd, tabstrip_bounds);

  // The 2 px (not DIP) at the inner edges of Win 7 glass are a light and dark
  // line, so we must inset further to account for those.
  constexpr int kWin7GlassInset = 2;
  const int inset =
      (base::win::GetVersion() < base::win::VERSION_WIN8) ? kWin7GlassInset : 0;
  return MARGINS{inset, inset, tabstrip_bounds.bottom() + inset, inset};
}

bool BrowserDesktopWindowTreeHostWin::IsOpaqueHostedAppFrame() const {
  // TODO(https://crbug.com/868239): Support Windows 7 Aero glass for hosted app
  // window titlebar controls.
  return browser_view_->IsBrowserTypeHostedApp() &&
         base::win::GetVersion() < base::win::VERSION_WIN10;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHost, public:

// static
BrowserDesktopWindowTreeHost*
    BrowserDesktopWindowTreeHost::CreateBrowserDesktopWindowTreeHost(
        views::internal::NativeWidgetDelegate* native_widget_delegate,
        views::DesktopNativeWidgetAura* desktop_native_widget_aura,
        BrowserView* browser_view,
        BrowserFrame* browser_frame) {
  return new BrowserDesktopWindowTreeHostWin(native_widget_delegate,
                                             desktop_native_widget_aura,
                                             browser_view,
                                             browser_frame);
}
