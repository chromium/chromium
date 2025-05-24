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

class SplashWnd : public CDialogImpl<SplashWnd> {
 public:
  static constexpr int IDD = IDD_SPLASH;

  SplashWnd();
  ~SplashWnd() override;

  BEGIN_MSG_MAP(SplashWnd)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT /*msg*/,
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

 private:
  base::win::ScopedGDIObject<HICON> hicon_;
};

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_INSTALLER_SPLASH_WND_H_
