// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALLER_SPLASH_WND_H_
#define CHROME_UPDATER_WIN_INSTALLER_SPLASH_WND_H_

#include <windows.h>

#include "base/win/scoped_gdi_object.h"
#include "chrome/updater/win/installer/installer_resource.h"
#include "ui/gfx/win/msg_util.h"
#include "ui/gfx/win/window_impl.h"

namespace updater::ui {

class SplashWnd : public gfx::WindowImpl {
 public:
  SplashWnd();
  SplashWnd(const SplashWnd&) = delete;
  SplashWnd& operator=(const SplashWnd&) = delete;
  ~SplashWnd() override;

  // Creates and shows the splash window as a `WS_POPUP | WS_VISIBLE` top-level
  // window. Returns the HWND on success.
  HWND Create(HWND parent);

  CR_BEGIN_MSG_MAP_EX(SplashWnd)
    CR_MESSAGE_HANDLER_EX(WM_CREATE, OnCreate)
    CR_MESSAGE_HANDLER_EX(WM_ERASEBKGND, OnEraseBkgnd)
    CR_MESSAGE_HANDLER_EX(WM_PAINT, OnPaint)
    CR_MESSAGE_HANDLER_EX(WM_DPICHANGED, OnDpiChanged)
    CR_MESSAGE_HANDLER_EX(WM_CLOSE, OnClose)
    CR_MESSAGE_HANDLER_EX(WM_DESTROY, OnDestroy)
  CR_END_MSG_MAP()

 private:
  LRESULT OnCreate(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnEraseBkgnd(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnPaint(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnDpiChanged(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnClose(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnDestroy(UINT msg, WPARAM wparam, LPARAM lparam);

  int GetScaledValue(int value, UINT dpi) const;

  base::win::ScopedGDIObject<HBITMAP> logo_bmp_;
  base::win::ScopedGDIObject<HICON> hicon_;
  SIZE logo_size_ = {0, 0};

  CR_MSG_MAP_CLASS_DECLARATIONS(SplashWnd)
};

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_INSTALLER_SPLASH_WND_H_
