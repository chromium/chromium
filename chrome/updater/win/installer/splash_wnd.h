// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALLER_SPLASH_WND_H_
#define CHROME_UPDATER_WIN_INSTALLER_SPLASH_WND_H_

#include <windows.h>

#include "base/win/atl.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/updater/win/installer/installer_resource.h"

namespace updater::ui {

class SplashWnd : public CWindowImpl<SplashWnd,
                                     CWindow,
                                     CWinTraits<WS_POPUP | WS_VISIBLE, 0>> {
 public:
  SplashWnd();
  ~SplashWnd() override;

  BEGIN_MSG_MAP(SplashWnd)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
    MESSAGE_HANDLER(WM_PAINT, OnPaint)
    MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
  END_MSG_MAP()

 private:
  LRESULT OnCreate(UINT /*msg*/,
                   WPARAM /*wparam*/,
                   LPARAM /*lparam*/,
                   BOOL& handled);
  LRESULT OnEraseBkgnd(UINT /*msg*/,
                       WPARAM /*wparam*/,
                       LPARAM /*lparam*/,
                       BOOL& handled);
  LRESULT OnPaint(UINT /*msg*/,
                  WPARAM /*wparam*/,
                  LPARAM /*lparam*/,
                  BOOL& handled);
  LRESULT OnDpiChanged(UINT /*msg*/,
                       WPARAM /*wparam*/,
                       LPARAM /*lparam*/,
                       BOOL& handled);
  LRESULT OnClose(UINT /*msg*/,
                  WPARAM /*wparam*/,
                  LPARAM /*lparam*/,
                  BOOL& handled);
  LRESULT OnDestroy(UINT /*msg*/,
                    WPARAM /*wparam*/,
                    LPARAM /*lparam*/,
                    BOOL& /*handled*/);

  int GetScaledValue(int value, UINT dpi) const;

  base::win::ScopedGDIObject<HBITMAP> logo_bmp_;
  base::win::ScopedGDIObject<HICON> hicon_;
  SIZE logo_size_ = {0, 0};
};

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_INSTALLER_SPLASH_WND_H_
