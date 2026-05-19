// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_COMPLETE_WND_H_
#define CHROME_UPDATER_WIN_UI_COMPLETE_WND_H_

#include <windows.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/updater/win/ui/resources/resources.grh"
#include "chrome/updater/win/ui/ui.h"
#include "ui/gfx/win/msg_util.h"

namespace updater::ui {

class CompleteWndEvents : public OmahaWndEvents {
 public:
  // Launches the browser and returns true if the browser was successfully
  // launched.
  virtual bool DoLaunchBrowser(const std::string& url) = 0;
};

class CompleteWnd : public OmahaWnd {
 public:
  CompleteWnd(const CompleteWnd&) = delete;
  CompleteWnd& operator=(const CompleteWnd&) = delete;

  HRESULT Initialize() override;

  void SetEventSink(CompleteWndEvents* ev);

  void DisplayCompletionDialog(bool is_success,
                               const std::wstring& text,
                               const std::string& help_url);

  CR_BEGIN_MSG_MAP_EX(CompleteWnd)
    CR_MESSAGE_HANDLER_EX(WM_INITDIALOG, OnInitDialog)
    CR_COMMAND_HANDLER_EX(IDC_GET_HELP, BN_CLICKED, OnClickedGetHelp)
    CR_COMMAND_HANDLER_EX(IDC_CLOSE, BN_CLICKED, OnClickedButton)
    CR_CHAIN_MSG_MAP(OmahaWnd)
  CR_END_MSG_MAP()

 protected:
  CompleteWnd(int dialog_id,
              DWORD control_classes,
              MessageLoop* message_loop,
              HWND parent,
              const std::wstring& lang);
  ~CompleteWnd() override;

  // Message and command handlers.
  LRESULT OnInitDialog(UINT msg, WPARAM wparam, LPARAM lparam);
  void OnClickedGetHelp(UINT notify_code, int id, HWND wnd_ctl);
  void OnClickedButton(UINT notify_code, int id, HWND wnd_ctl);

 private:
  // Handles requests to close the window. Returns true if the window is closed.
  bool MaybeCloseWindow() override;

  HRESULT SetControlState(bool is_success);

  std::string help_url_;
  raw_ptr<CompleteWndEvents> events_sink_ = nullptr;
  const DWORD control_classes_;

  CR_MSG_MAP_CLASS_DECLARATIONS(CompleteWnd)
};

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_COMPLETE_WND_H_
