// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/yes_no_dialog.h"

#include "chrome/updater/win/ui/constants.h"
#include "chrome/updater/win/ui/ui.h"
#include "chrome/updater/win/ui/util.h"

namespace updater {
namespace ui {

YesNoDialog::YesNoDialog(WTL::CMessageLoop* message_loop, HWND parent)
    : message_loop_(message_loop), parent_(parent), yes_clicked_(false) {
  DCHECK(message_loop);
}

YesNoDialog::~YesNoDialog() {
  DCHECK(!IsWindow());
}

BOOL YesNoDialog::PreTranslateMessage(MSG* msg) {
  return CWindow::IsDialogMessage(msg);
}

HRESULT YesNoDialog::Initialize(const base::string16& yes_no_title,
                                const base::string16& yes_no_text) {
  DCHECK(!IsWindow());

  if (!Create(parent_))
    return E_FAIL;

  message_loop_->AddMessageFilter(this);

  SetWindowText(yes_no_title.c_str());
  SetDlgItemText(IDC_YES_NO_TEXT, yes_no_text.c_str());

  base::string16 text_yes;
  LoadString(IDS_YES, &text_yes);
  SetDlgItemText(IDOK, text_yes.c_str());

  base::string16 text_no;
  LoadString(IDS_NO, &text_no);
  SetDlgItemText(IDCANCEL, text_no.c_str());

  HRESULT hr =
      SetWindowIcon(m_hWnd, IDI_APP,
                    base::win::ScopedGDIObject<HICON>::Receiver(hicon_).get());
  if (FAILED(hr))
    VLOG(1) << "Failed to SetWindowIcon" << hr;

  default_font_.CreatePointFont(90, kDialogFont);
  SendMessageToDescendants(
      WM_SETFONT, reinterpret_cast<WPARAM>(static_cast<HFONT>(default_font_)),
      0);

  CreateOwnerDrawTitleBar(m_hWnd, GetDlgItem(IDC_TITLE_BAR_SPACER), kBkColor);
  SetCustomDlgColors(kTextColor, kBkColor);

  EnableFlatButtons(m_hWnd);

  return S_OK;
}

HRESULT YesNoDialog::Show() {
  DCHECK(IsWindow());
  DCHECK(!IsWindowVisible());

  CenterWindow(nullptr);
  ShowWindow(SW_SHOWNORMAL);

  return S_OK;
}

LRESULT YesNoDialog::OnClickedButton(WORD notify_code,
                                     WORD id,
                                     HWND wnd_ctl,
                                     BOOL& handled) {
  DCHECK(id == IDOK || id == IDCANCEL);

  switch (id) {
    case IDOK:
      yes_clicked_ = true;
      break;

    case IDCANCEL:
      yes_clicked_ = false;
      break;

    default:
      DCHECK(false);
      yes_clicked_ = false;
      break;
  }

  handled = true;
  SendMessage(WM_CLOSE, 0, 0);

  return 0;
}

LRESULT YesNoDialog::OnClose(UINT message,
                             WPARAM wparam,
                             LPARAM lparam,
                             BOOL& handled) {
  DestroyWindow();

  handled = TRUE;
  return 0;
}

LRESULT YesNoDialog::OnNCDestroy(UINT message,
                                 WPARAM wparam,
                                 LPARAM lparam,
                                 BOOL& handled) {
  message_loop_->RemoveMessageFilter(this);

  ::PostQuitMessage(0);

  handled = FALSE;  // Let ATL default processing handle the WM_NCDESTROY.
  return 0;
}

}  // namespace ui
}  // namespace updater
