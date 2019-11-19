// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_win.h"

#include <windows.h>

#include <dwmapi.h>
#include <uxtheme.h>

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_window_property_manager_win.h"
#include "chrome/browser/ui/views/frame/system_menu_insertion_delegate_win.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/win/app_icon.h"
#include "chrome/browser/win/titlebar_config.h"
#include "chrome/common/chrome_constants.h"
#include "ui/base/theme_provider.h"
#include "ui/base/win/hwnd_metrics.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image_family.h"
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
      browser_frame_(browser_frame) {
  profile_observer_.Add(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage());
}

BrowserDesktopWindowTreeHostWin::~BrowserDesktopWindowTreeHostWin() {}

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

void BrowserDesktopWindowTreeHostWin::Init(
    const views::Widget::InitParams& params) {
  DesktopWindowTreeHostWin::Init(std::move(params));
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;  // VirtualDesktopManager isn't support pre Win-10.

  // Virtual Desktops on Windows are best-effort and may not always be
  // available.
  if (FAILED(::CoCreateInstance(__uuidof(VirtualDesktopManager), nullptr,
                                CLSCTX_ALL,
                                IID_PPV_ARGS(&virtual_desktop_manager_)))) {
    return;
  }

  if (!params.workspace.empty()) {
    GUID guid = GUID_NULL;
    HRESULT hr =
        CLSIDFromString(base::UTF8ToUTF16(params.workspace).c_str(), &guid);
    if (SUCCEEDED(hr)) {
      // There are valid reasons MoveWindowToDesktop can fail, e.g.,
      // the desktop was deleted. If it fails, the window will open on the
      // current desktop.
      virtual_desktop_manager_->MoveWindowToDesktop(GetHWND(), guid);
    }
  }
}

std::string BrowserDesktopWindowTreeHostWin::GetWorkspace() const {
  std::string workspace_id;
  if (virtual_desktop_manager_) {
    GUID workspace_guid;
    HRESULT hr = virtual_desktop_manager_->GetWindowDesktopId(GetHWND(),
                                                              &workspace_guid);
    if (FAILED(hr) || workspace_guid == GUID_NULL)
      return workspace_.value_or("");

    LPOLESTR workspace_widestr;
    StringFromCLSID(workspace_guid, &workspace_widestr);
    workspace_id = base::WideToUTF8(workspace_widestr);
    workspace_ = workspace_id;
    CoTaskMemFree(workspace_widestr);
  }
  return workspace_id;
}

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

bool BrowserDesktopWindowTreeHostWin::GetDwmFrameInsetsInPixels(
    gfx::Insets* insets) const {
  // For "normal" windows on Aero, we always need to reset the glass area
  // correctly, even if we're not currently showing the native frame (e.g.
  // because a theme is showing), so we explicitly check for that case rather
  // than checking ShouldUseNativeFrame() here.  Using that here would mean we
  // wouldn't reset the glass area to zero when moving from the native frame to
  // an opaque frame, leading to graphical glitches behind the opaque frame.
  // Instead, we use that function below to tell us whether the frame is
  // currently native or opaque.
  if (!GetWidget()->client_view() || !browser_view_->IsBrowserTypeNormal() ||
      !DesktopWindowTreeHostWin::ShouldUseNativeFrame())
    return false;

  // Don't extend the glass in at all if it won't be visible.
  if (!ShouldUseNativeFrame() || GetWidget()->IsFullscreen() ||
      ShouldCustomDrawSystemTitlebar()) {
    *insets = gfx::Insets();
  } else {
    // The glass should extend to the bottom of the tabstrip.
    HWND hwnd = GetHWND();
    gfx::Rect tabstrip_region_bounds(
        browser_frame_->GetBoundsForTabStripRegion(browser_view_->tabstrip()));
    tabstrip_region_bounds =
        display::win::ScreenWin::DIPToClientRect(hwnd, tabstrip_region_bounds);

    // The 2 px (not DIP) at the inner edges of Win 7 glass are a light and dark
    // line, so we must inset further to account for those.
    constexpr int kWin7GlassInset = 2;
    const int inset = (base::win::GetVersion() < base::win::Version::WIN8)
                          ? kWin7GlassInset
                          : 0;
    *insets = gfx::Insets(tabstrip_region_bounds.bottom() + inset, inset, inset,
                          inset);
  }
  return true;
}

void BrowserDesktopWindowTreeHostWin::HandleCreate() {
  DesktopWindowTreeHostWin::HandleCreate();
  browser_window_property_manager_ =
      BrowserWindowPropertyManager::CreateBrowserWindowPropertyManager(
          browser_view_, GetHWND());

  // Use the profile icon as the browser window icon, if there is more
  // than one profile. This makes alt-tab preview tabs show the profile-specific
  // icon in the multi-profile case.
  if (g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetNumberOfProfiles() > 1) {
    SetWindowIcon(/*badged=*/true);
  }
}

void BrowserDesktopWindowTreeHostWin::HandleDestroying() {
  // TODO(crbug/976176): Move all access to |virtual_desktop_manager_| off of
  // the ui thread to prevent reentrancy bugs due to COM objects pumping
  // messages. For now, Reset() so COM object destructor is called before
  // |this| is in the process of being deleted.
  virtual_desktop_manager_.Reset();
  browser_window_property_manager_.reset();
  DesktopWindowTreeHostWin::HandleDestroying();
}

void BrowserDesktopWindowTreeHostWin::HandleFrameChanged() {
  // Reinitialize the status bubble, since it needs to be initialized
  // differently depending on whether or not DWM composition is enabled
  browser_view_->InitStatusBubble();
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
  switch (message) {
    case WM_SETFOCUS: {
      // GetWorkspace sets |workspace_|, so stash prev value.
      std::string prev_workspace = workspace_.value_or("");
      if (prev_workspace != GetWorkspace())
        OnHostWorkspaceChanged();
      break;
    }
    case WM_CREATE:
      minimize_button_metrics_.Init(GetHWND());
      break;
    case WM_WINDOWPOSCHANGED: {
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

////////////////////////////////////////////////////////////////////////////////
// ProfileAttributesStorage::Observer overrides:

void BrowserDesktopWindowTreeHostWin::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  // If we're currently badging the window icon (>1 available profile),
  // and this window's profile's avatar changed, update the window icon.
  if (browser_view_->browser()->profile()->GetPath() == profile_path &&
      g_browser_process->profile_manager()
              ->GetProfileAttributesStorage()
              .GetNumberOfProfiles() > 1) {
    // If we went from 1 to 2 profiles, window icons should be badged.
    SetWindowIcon(/*badged=*/true);
  }
}

void BrowserDesktopWindowTreeHostWin::OnProfileAdded(
    const base::FilePath& profile_path) {
  if (g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetNumberOfProfiles() == 2) {
    SetWindowIcon(/*badged=*/true);
  }
}

void BrowserDesktopWindowTreeHostWin::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  if (g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetNumberOfProfiles() == 1) {
    // If we went from 2 profiles to 1, window icons should not be badged.
    SetWindowIcon(/*badged=*/false);
  }
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostWin, private:
bool BrowserDesktopWindowTreeHostWin::IsOpaqueHostedAppFrame() const {
  // TODO(https://crbug.com/868239): Support Windows 7 Aero glass for web-app
  // window titlebar controls.
  return browser_view_->IsBrowserTypeWebApp() &&
         base::win::GetVersion() < base::win::Version::WIN10;
}

SkBitmap GetBadgedIconBitmapForProfile(Profile* profile) {
  std::unique_ptr<gfx::ImageFamily> family = GetAppIconImageFamily();
  if (!family)
    return SkBitmap();

  SkBitmap app_icon_bitmap = family
                                 ->CreateExact(profiles::kShortcutIconSizeWin,
                                               profiles::kShortcutIconSizeWin)
                                 .AsBitmap();
  if (app_icon_bitmap.isNull())
    return SkBitmap();

  SkBitmap avatar_bitmap_1x;
  SkBitmap avatar_bitmap_2x;

  ProfileAttributesEntry* entry = nullptr;
  if (!g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile->GetPath(), &entry))
    return SkBitmap();

  profiles::GetWinAvatarImages(entry, &avatar_bitmap_1x, &avatar_bitmap_2x);
  return profiles::GetBadgedWinIconBitmapForAvatar(app_icon_bitmap,
                                                   avatar_bitmap_1x, 1);
}

void BrowserDesktopWindowTreeHostWin::SetWindowIcon(bool badged) {
  // Hold onto the previous icon so that the currently displayed
  // icon is valid until replaced with the new icon.
  base::win::ScopedHICON previous_icon = std::move(icon_handle_);
  if (badged) {
    icon_handle_ = IconUtil::CreateHICONFromSkBitmap(
        GetBadgedIconBitmapForProfile(browser_view_->browser()->profile()));
  } else {
    icon_handle_.reset(GetAppIcon());
  }
  SendMessage(GetHWND(), WM_SETICON, ICON_SMALL,
              reinterpret_cast<LPARAM>(icon_handle_.get()));
  SendMessage(GetHWND(), WM_SETICON, ICON_BIG,
              reinterpret_cast<LPARAM>(icon_handle_.get()));
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
                                             browser_view, browser_frame);
}
