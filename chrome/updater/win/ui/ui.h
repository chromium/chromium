// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_UI_H_
#define CHROME_UPDATER_WIN_UI_UI_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "base/win/atl.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/updater/win/ui/owner_draw_controls.h"
#include "chrome/updater/win/ui/resources/resources.grh"

namespace updater {
namespace ui {

void EnableFlatButtons(HWND hwnd_parent);

void HideWindowChildren(HWND hwnd_parent);

class OmahaWndEvents {
 public:
  virtual ~OmahaWndEvents() = default;
  virtual void DoClose() = 0;
  virtual void DoExit() = 0;
};

// Implements the UI progress window.
class OmahaWnd : public CAxDialogImpl<OmahaWnd>,
                 public OwnerDrawTitleBar,
                 public CustomDlgColors,
                 public WTL::CMessageFilter {
  using Base = CAxDialogImpl<OmahaWnd>;

 public:
  const int IDD;

  ~OmahaWnd() override;

  virtual HRESULT Initialize();

  // Overrides for CMessageFilter.
  BOOL PreTranslateMessage(MSG* msg) override;

  void SetEventSink(OmahaWndEvents* ev) { events_sink_ = ev; }

  void set_is_machine(bool is_machine) { is_machine_ = is_machine; }
  void set_bundle_name(const base::string16& bundle_name) {
    bundle_name_ = bundle_name;
  }

  virtual void Show();

  BEGIN_MSG_MAP(OmahaWnd)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    MESSAGE_HANDLER(WM_NCDESTROY, OnNCDestroy)
    COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    CHAIN_MSG_MAP(Base)
    CHAIN_MSG_MAP(OwnerDrawTitleBar)
    CHAIN_MSG_MAP(CustomDlgColors)
  END_MSG_MAP()

 protected:
  struct ControlAttributes {
    const bool is_ignore_entry = false;
    const bool is_visible = false;
    const bool is_enabled = false;
    const bool is_button = false;
    const bool is_default = false;
  };

  OmahaWnd(int dialog_id, WTL::CMessageLoop* message_loop, HWND parent);

  // Message and command handlers.
  LRESULT OnClose(UINT msg,
                  WPARAM wparam,
                  LPARAM lparam,
                  BOOL& handled);  // NOLINT
  LRESULT OnNCDestroy(UINT msg,
                      WPARAM wparam,
                      LPARAM lparam,
                      BOOL& handled);  // NOLINT
  LRESULT OnCancel(WORD notify_code,
                   WORD id,
                   HWND wnd_ctl,
                   BOOL& handled);  // NOLINT

  // Returns true if the window is closed.
  virtual bool MaybeCloseWindow() = 0;

  // Returns true to indicate that the client continues handling OnComplete.
  bool OnComplete();

  HRESULT CloseWindow();
  void InitializeDialog();

  HRESULT EnableClose(bool enable);
  HRESULT EnableSystemCloseButton(bool enable);

  void SetControlAttributes(int control_id,
                            const ControlAttributes& attributes);

  void SetVisible(bool visible) {
    ShowWindow(visible ? SW_SHOWNORMAL : SW_HIDE);
  }

  WTL::CMessageLoop* message_loop() { return message_loop_; }
  bool is_complete() { return is_complete_; }
  bool is_close_enabled() { return is_close_enabled_; }
  bool is_machine() { return is_machine_; }
  const base::string16& bundle_name() { return bundle_name_; }

  static const ControlAttributes kVisibleTextAttributes;
  static const ControlAttributes kDefaultActiveButtonAttributes;
  static const ControlAttributes kDisabledButtonAttributes;
  static const ControlAttributes kNonDefaultActiveButtonAttributes;
  static const ControlAttributes kVisibleImageAttributes;
  static const ControlAttributes kDisabledNonButtonAttributes;

 private:
  HRESULT SetWindowIcon();

  void MaybeRequestExitProcess();
  void RequestExitProcess();

  THREAD_CHECKER(thread_checker_);

  WTL::CMessageLoop* message_loop_;
  HWND parent_;

  bool is_complete_;
  bool is_close_enabled_;

  OmahaWndEvents* events_sink_;

  bool is_machine_;
  base::string16 bundle_name_;

  // Handle to large icon to show when ALT-TAB
  base::win::ScopedGDIObject<HICON> hicon_;

  WTL::CFont default_font_;
  WTL::CFont font_;
  WTL::CFont error_font_;

  CustomProgressBarCtrl progress_bar_;

  DISALLOW_COPY_AND_ASSIGN(OmahaWnd);
};

// Registers the specified common control classes from the common control DLL.
// Calls are cumulative, meaning control_classes are added to existing classes.
// UIs that use common controls should call this function to ensure that the UI
// supports visual styles.
HRESULT InitializeCommonControls(DWORD control_classes);

}  // namespace ui
}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UI_UI_H_
