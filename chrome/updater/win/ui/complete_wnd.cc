// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/complete_wnd.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/updater/win/ui/constants.h"
#include "chrome/updater/win/ui/util.h"
#include "chrome/updater/win/util.h"

namespace updater {
namespace ui {

// dialog_id specifies the dialog resource to use.
// control_classes specifies the control classes required for dialog_id.
CompleteWnd::CompleteWnd(int dialog_id,
                         DWORD control_classes,
                         WTL::CMessageLoop* message_loop,
                         HWND parent)
    : OmahaWnd(dialog_id, message_loop, parent),
      events_sink_(nullptr),
      control_classes_(control_classes | ICC_STANDARD_CLASSES) {}

CompleteWnd::~CompleteWnd() = default;

HRESULT CompleteWnd::Initialize() {
  HRESULT hr = InitializeCommonControls(control_classes_);
  if (FAILED(hr))
    return hr;
  return OmahaWnd::Initialize();
}

void CompleteWnd::SetEventSink(CompleteWndEvents* ev) {
  events_sink_ = ev;
  OmahaWnd::SetEventSink(events_sink_);
}

LRESULT CompleteWnd::OnInitDialog(UINT message,
                                  WPARAM w_param,
                                  LPARAM l_param,
                                  BOOL& handled) {
  // TODO(sorin): remove this when https://crbug.com/1010653 is fixed.
  HideWindowChildren(*this);
  InitializeDialog();

  SetControlAttributes(IDC_COMPLETE_TEXT, kDisabledNonButtonAttributes);
  SetControlAttributes(IDC_ERROR_TEXT, kDisabledNonButtonAttributes);
  SetControlAttributes(IDC_GET_HELP, kDisabledButtonAttributes);
  SetControlAttributes(IDC_CLOSE, kDefaultActiveButtonAttributes);

  handled = true;
  return 1;  // Let the system set the focus.
}

LRESULT CompleteWnd::OnClickedButton(WORD notify_code,
                                     WORD id,
                                     HWND wnd_ctl,
                                     BOOL& handled) {
  DCHECK(id == IDC_CLOSE);
  DCHECK(is_complete());

  CloseWindow();

  handled = true;
  return 0;
}

LRESULT CompleteWnd::OnClickedGetHelp(WORD notify_code,
                                      WORD id,
                                      HWND wnd_ctl,
                                      BOOL& handled) {
  DCHECK(events_sink_);
  if (events_sink_)
    events_sink_->DoLaunchBrowser(help_url_);

  handled = true;
  return 1;
}

bool CompleteWnd::MaybeCloseWindow() {
  CloseWindow();
  return true;
}

void CompleteWnd::DisplayCompletionDialog(bool is_success,
                                          const base::string16& text,
                                          const base::string16& help_url) {
  if (!OmahaWnd::OnComplete())
    return;

  base::string16 s;
  LoadString(IDS_CLOSE, &s);
  SetDlgItemText(IDC_CLOSE, s.c_str());

  DCHECK(!text.empty());

  // FormatMessage() converts all LFs to CRLFs, which are rendered as little
  // squares in UI. To avoid this, convert all CRLFs to LFs.
  base::string16 display_text;
  base::ReplaceChars(text, L"\r\n", L"\n", &display_text);

  if (is_success) {
    SetDlgItemText(IDC_COMPLETE_TEXT, display_text.c_str());
  } else {
    SetDlgItemText(IDC_ERROR_TEXT, display_text.c_str());

    if (!help_url.empty()) {
      help_url_ = help_url.c_str();
      LoadString(IDS_GET_HELP_TEXT, &s);
      SetDlgItemText(IDC_GET_HELP, s.c_str());
    }
  }

  SetControlState(is_success);
  return;
}

HRESULT CompleteWnd::SetControlState(bool is_success) {
  SetControlAttributes(is_success ? IDC_COMPLETE_TEXT : IDC_ERROR_TEXT,
                       kVisibleTextAttributes);
  if (!is_success)
    SetControlAttributes(IDC_ERROR_ILLUSTRATION, kVisibleImageAttributes);
  if (!help_url_.empty())
    SetControlAttributes(IDC_GET_HELP, kNonDefaultActiveButtonAttributes);
  SetControlAttributes(IDC_CLOSE, kDefaultActiveButtonAttributes);
  return S_OK;
}

}  // namespace ui
}  // namespace updater
