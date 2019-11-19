// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_COMPLETE_WND_H_
#define CHROME_UPDATER_WIN_UI_COMPLETE_WND_H_

#include <windows.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/win/atl.h"
#include "chrome/updater/win/ui/resources/resources.grh"
#include "chrome/updater/win/ui/ui.h"

namespace updater {
namespace ui {

class CompleteWndEvents : public OmahaWndEvents {
 public:
  // Launches the browser and returns true if the browser was successfully
  // launched.
  virtual bool DoLaunchBrowser(const base::string16& url) = 0;
};

class CompleteWnd : public OmahaWnd {
 public:
  HRESULT Initialize() override;

  void SetEventSink(CompleteWndEvents* ev);

  void DisplayCompletionDialog(bool is_success,
                               const base::string16& text,
                               const base::string16& help_url);
  BEGIN_MSG_MAP(CompleteWnd)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    COMMAND_HANDLER(IDC_GET_HELP, BN_CLICKED, OnClickedGetHelp)
    COMMAND_HANDLER(IDC_CLOSE, BN_CLICKED, OnClickedButton)
    CHAIN_MSG_MAP(OmahaWnd)
  END_MSG_MAP()

 protected:
  CompleteWnd(int dialog_id,
              DWORD control_classes,
              WTL::CMessageLoop* message_loop,
              HWND parent);
  ~CompleteWnd() override;

  // Message and command handlers.
  LRESULT OnInitDialog(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  LRESULT OnClickedGetHelp(WORD notify_code,
                           WORD id,
                           HWND wnd_ctl,
                           BOOL& handled);  // NOLINT
  LRESULT OnClickedButton(WORD notify_code,
                          WORD id,
                          HWND wnd_ctl,
                          BOOL& handled);                          // NOLINT
  LRESULT OnUrlClicked(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);  // NOLINT

 private:
  // Handles requests to close the window. Returns true if the window is closed.
  bool MaybeCloseWindow() override;

  HRESULT SetControlState(bool is_success);

  base::string16 help_url_;
  CompleteWndEvents* events_sink_ = nullptr;
  const DWORD control_classes_;

  DISALLOW_COPY_AND_ASSIGN(CompleteWnd);
};

}  // namespace ui
}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UI_COMPLETE_WND_H_
