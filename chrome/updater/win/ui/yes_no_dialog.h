// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_YES_NO_DIALOG_H_
#define CHROME_UPDATER_WIN_UI_YES_NO_DIALOG_H_

#include <windows.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/win/atl.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/updater/win/ui/owner_draw_controls.h"
#include "chrome/updater/win/ui/resources/resources.grh"

namespace updater {
namespace ui {

class YesNoDialog : public CAxDialogImpl<YesNoDialog>,
                    public OwnerDrawTitleBar,
                    public CustomDlgColors,
                    public WTL::CMessageFilter {
  using Base = CAxDialogImpl<YesNoDialog>;

 public:
  static constexpr int IDD = IDD_YES_NO;

  YesNoDialog(WTL::CMessageLoop* message_loop, HWND parent);
  ~YesNoDialog() override;

  HRESULT Initialize(const base::string16& yes_no_title,
                     const base::string16& yes_no_text);
  HRESULT Show();

  bool yes_clicked() const { return yes_clicked_; }

  // Overrides for CMessageFilter.
  BOOL PreTranslateMessage(MSG* msg) override;

  BEGIN_MSG_MAP(YesNoDialog)
    COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedButton)
    COMMAND_ID_HANDLER(IDCANCEL, OnClickedButton)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    MESSAGE_HANDLER(WM_NCDESTROY, OnNCDestroy)
    CHAIN_MSG_MAP(Base)
    CHAIN_MSG_MAP(OwnerDrawTitleBar)
    CHAIN_MSG_MAP(CustomDlgColors)
  END_MSG_MAP()

 private:
  // Message and command handlers.
  LRESULT OnClickedButton(WORD notify_code,
                          WORD id,
                          HWND wnd_ctl,
                          BOOL& handled);  // NOLINT(runtime/references)
  LRESULT OnClose(UINT msg,
                  WPARAM wparam,
                  LPARAM lparam,
                  BOOL& handled);  // NOLINT(runtime/references)
  LRESULT OnNCDestroy(UINT msg,
                      WPARAM wparam,
                      LPARAM lparam,
                      BOOL& handled);  // NOLINT(runtime/references)

  WTL::CMessageLoop* message_loop_;
  HWND parent_;
  bool yes_clicked_;

  // Handle to large icon to show when ALT-TAB.
  base::win::ScopedGDIObject<HICON> hicon_;

  WTL::CFont default_font_;

  DISALLOW_COPY_AND_ASSIGN(YesNoDialog);
};

}  // namespace ui
}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UI_YES_NO_DIALOG_H_
