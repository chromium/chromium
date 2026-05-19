// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer/splash_wnd.h"

#include <windows.h>

#include "base/strings/utf_string_conversions.h"
#include "base/win/current_module.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/win/installer/installer_resource.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/ui_util.h"
#include "ui/gfx/geometry/rect.h"

namespace updater::ui {

SplashWnd::SplashWnd() {
  set_window_style(WS_POPUP | WS_VISIBLE);
  set_window_class_name(L"UpdaterSplashWnd");

  logo_bmp_.reset(static_cast<HBITMAP>(
      ::LoadImage(CURRENT_MODULE(), MAKEINTRESOURCE(IDB_LOGO), IMAGE_BITMAP, 0,
                  0, LR_CREATEDIBSECTION)));
  if (!logo_bmp_.is_valid()) {
    return;
  }

  BITMAP bm = {};
  ::GetObject(logo_bmp_.get(), sizeof(bm), &bm);
  logo_size_.cx = bm.bmWidth;
  logo_size_.cy = bm.bmHeight;
}

SplashWnd::~SplashWnd() = default;

HWND SplashWnd::Create(HWND parent) {
  Init(parent, gfx::Rect());
  return hwnd();
}

LRESULT SplashWnd::OnCreate(UINT, WPARAM, LPARAM) {
  if (!logo_bmp_.is_valid()) {
    return -1;
  }

  // Center window based on the DPI.
  const UINT dpi = ::GetDpiForWindow(hwnd());
  const int width = GetScaledValue(logo_size_.cx, dpi);
  const int height = GetScaledValue(logo_size_.cy, dpi);
  ::SetWindowPos(hwnd(), HWND_TOPMOST,
                 /*x=*/(::GetSystemMetrics(SM_CXSCREEN) - width) / 2,
                 /*y=*/(::GetSystemMetrics(SM_CYSCREEN) - height) / 2, width,
                 height, SWP_SHOWWINDOW);

  SetWindowIcon(hwnd(), IDI_MINI_INSTALLER,
                base::win::ScopedGDIObject<HICON>::Receiver(hicon_).get());

  return 0;
}

LRESULT SplashWnd::OnEraseBkgnd(UINT, WPARAM, LPARAM) {
  // Don't actually draw -- avoid flicker by skipping the default background
  // erase. Returning non-zero indicates the background was already erased.
  return 1;
}

LRESULT SplashWnd::OnPaint(UINT, WPARAM, LPARAM) {
  PAINTSTRUCT ps = {};
  HDC hdc = ::BeginPaint(hwnd(), &ps);
  if (!hdc) {
    return 0;
  }
  if (!logo_bmp_.is_valid()) {
    ::EndPaint(hwnd(), &ps);
    return 0;
  }

  RECT client_rect = {0};
  ::GetClientRect(hwnd(), &client_rect);

  HDC hdc_mem = ::CreateCompatibleDC(hdc);
  const HGDIOBJ old_bm = ::SelectObject(hdc_mem, logo_bmp_.get());

  // Set high-quality `HALFTONE` scaling mode.
  ::SetStretchBltMode(hdc, HALFTONE);
  ::SetBrushOrgEx(hdc, 0, 0, nullptr);

  // Scale and draw the logo.
  ::StretchBlt(hdc, 0, 0, client_rect.right, client_rect.bottom, hdc_mem, 0, 0,
               logo_size_.cx, logo_size_.cy, SRCCOPY);

  ::SelectObject(hdc_mem, old_bm);
  ::DeleteDC(hdc_mem);
  ::EndPaint(hwnd(), &ps);
  return 0;
}

LRESULT SplashWnd::OnDpiChanged(UINT, WPARAM, LPARAM lparam) {
  // Resize the window.
  const RECT* new_window_rect = reinterpret_cast<RECT*>(lparam);
  ::SetWindowPos(hwnd(), nullptr, new_window_rect->left, new_window_rect->top,
                 new_window_rect->right - new_window_rect->left,
                 new_window_rect->bottom - new_window_rect->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);

  // Force a full repaint to redraw the logo at the new scale.
  ::InvalidateRect(hwnd(), nullptr, TRUE);
  return 0;
}

int SplashWnd::GetScaledValue(int value, UINT dpi) const {
  // Standard Win32 scaling formula: (value * dpi) / 96.
  return ::MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
}

LRESULT SplashWnd::OnClose(UINT, WPARAM, LPARAM) {
  ::DestroyWindow(hwnd());
  return 0;
}

LRESULT SplashWnd::OnDestroy(UINT, WPARAM, LPARAM) {
  ::PostQuitMessage(0);
  return 0;
}

}  // namespace updater::ui
