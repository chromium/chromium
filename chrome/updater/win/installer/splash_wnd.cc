// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer/splash_wnd.h"

#include "base/win/scoped_gdi_object.h"
#include "chrome/updater/win/installer/installer_resource.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/ui.h"
#include "chrome/updater/win/ui/ui_constants.h"
#include "chrome/updater/win/ui/ui_util.h"

namespace updater::ui {

SplashWnd::SplashWnd() = default;
SplashWnd::~SplashWnd() = default;

LRESULT SplashWnd::OnInitDialog(UINT /*msg*/,
                                WPARAM /*wparam*/,
                                LPARAM /*lparam*/,
                                BOOL& handled) {
  SetWindowText(GetInstallerDisplayName({}).c_str());
  SetDlgItemText(IDC_SPLASH_TEXT, GetLocalizedSplashScreenString().c_str());

  CenterWindow();
  SetWindowIcon(m_hWnd, IDI_MINI_INSTALLER,
                base::win::ScopedGDIObject<HICON>::Receiver(hicon_).get());

  CreateOwnerDrawTitleBar(m_hWnd, GetDlgItem(IDC_SPLASH_TITLE_BAR_SPACER),
                          kBkColor);
  SetCustomDlgColors(kTextColor, kBkColor);

  EnableFlatButtons(m_hWnd);

  handled = true;
  return 0;
}

LRESULT SplashWnd::OnClose(UINT /*msg*/,
                           WPARAM /*wparam*/,
                           LPARAM /*lparam*/,
                           BOOL& handled) {
  DestroyWindow();
  handled = true;
  return 0;
}

LRESULT SplashWnd::OnDestroy(UINT /*msg*/,
                             WPARAM /*wparam*/,
                             LPARAM /*lparam*/,
                             BOOL& /*handled*/) {
  ::PostQuitMessage(0);
  return 0;
}

}  // namespace updater::ui
