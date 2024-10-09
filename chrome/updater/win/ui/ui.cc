// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/ui.h"

#include <stdint.h>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/ui/ui_constants.h"
#include "chrome/updater/win/ui/ui_util.h"

namespace updater::ui {

const OmahaWnd::ControlAttributes OmahaWnd::kVisibleTextAttributes = {
    false, true, true, false, false};
const OmahaWnd::ControlAttributes OmahaWnd::kDefaultActiveButtonAttributes = {
    false, true, true, true, true};
const OmahaWnd::ControlAttributes OmahaWnd::kDisabledButtonAttributes = {
    false, false, false, true, false};
const OmahaWnd::ControlAttributes OmahaWnd::kNonDefaultActiveButtonAttributes =
    {false, true, true, true, false};
const OmahaWnd::ControlAttributes OmahaWnd::kVisibleImageAttributes = {
    false, true, false, false, false};
const OmahaWnd::ControlAttributes OmahaWnd::kDisabledNonButtonAttributes = {
    false, false, false, false, false};

void EnableFlatButtons(HWND hwnd_parent) {
  struct Local {
    static BOOL CALLBACK EnumProc(HWND hwnd, LPARAM) {
      CHECK(hwnd);
      CWindow wnd(hwnd);
      const DWORD style = wnd.GetStyle();
      if (style & BS_FLAT) {
        ::SetWindowTheme(wnd, _T(""), _T(""));
      }
      return true;
    }
  };

  ::EnumChildWindows(hwnd_parent, &Local::EnumProc, 0);
}

void HideWindowChildren(HWND hwnd_parent) {
  struct Local {
    static BOOL CALLBACK EnumProc(HWND hwnd, LPARAM) {
      CHECK(hwnd);
      ShowWindow(hwnd, SW_HIDE);
      return true;
    }
  };
  ::EnumChildWindows(hwnd_parent, &Local::EnumProc, 0);
}

OmahaWnd::OmahaWnd(int dialog_id, WTL::CMessageLoop* message_loop, HWND parent)
    : IDD(dialog_id),
      message_loop_(message_loop),
      parent_(parent),
      is_complete_(false),
      is_close_enabled_(true),
      events_sink_(nullptr),
      scope_(UpdaterScope::kUser) {
  CHECK(message_loop);
}

OmahaWnd::~OmahaWnd() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!IsWindow());
}

HRESULT OmahaWnd::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!Create(parent_)) {
    VLOG(1) << "Failed to create the window";
    return E_FAIL;
  }
  message_loop_->AddMessageFilter(this);

  return S_OK;
}

BOOL OmahaWnd::PreTranslateMessage(MSG* msg) {
  return CWindow::IsDialogMessage(msg);
}

void OmahaWnd::InitializeDialog() {
  SetWindowText(GetInstallerDisplayName(bundle_name()).c_str());

  CenterWindow(nullptr);
  ui::SetWindowIcon(m_hWnd, IDI_APP,
                    base::win::ScopedGDIObject<HICON>::Receiver(hicon_).get());

  // Disable the Maximize System Menu item.
  HMENU menu = ::GetSystemMenu(*this, false);
  CHECK(menu);
  ::EnableMenuItem(menu, SC_MAXIMIZE, MF_BYCOMMAND | MF_GRAYED);

  progress_bar_.SubclassWindow(GetDlgItem(IDC_PROGRESS));

  default_font_.CreatePointFont(90, kDialogFont);
  SendMessageToDescendants(
      WM_SETFONT, reinterpret_cast<WPARAM>(static_cast<HFONT>(default_font_)),
      0);

  font_.CreatePointFont(150, kDialogFont);
  GetDlgItem(IDC_INSTALLER_STATE_TEXT).SetFont(font_);
  GetDlgItem(IDC_INFO_TEXT).SetFont(font_);
  GetDlgItem(IDC_COMPLETE_TEXT).SetFont(font_);

  error_font_.CreatePointFont(110, kDialogFont);
  GetDlgItem(IDC_ERROR_TEXT).SetFont(error_font_);

  CreateOwnerDrawTitleBar(m_hWnd, GetDlgItem(IDC_TITLE_BAR_SPACER), kBkColor);
  SetCustomDlgColors(kTextColor, kBkColor);

  EnableFlatButtons(m_hWnd);
}

LRESULT OmahaWnd::OnClose(UINT, WPARAM, LPARAM, BOOL& handled) {
  MaybeCloseWindow();
  handled = true;
  return 0;
}

HRESULT OmahaWnd::CloseWindow() {
  HRESULT hr = DestroyWindow() ? S_OK : HRESULTFromLastError();
  if (events_sink_) {
    events_sink_->DoClose();
  }
  return hr;
}

void OmahaWnd::MaybeRequestExitProcess() {
  if (!is_complete_) {
    return;
  }

  RequestExitProcess();
}

void OmahaWnd::RequestExitProcess() {
  if (events_sink_) {
    events_sink_->DoExit();
  }
}

LRESULT OmahaWnd::OnNCDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
  message_loop_->RemoveMessageFilter(this);
  MaybeRequestExitProcess();
  handled = false;  // Let ATL default processing handle the WM_NCDESTROY.
  return 0;
}

// Called when ESC key is pressed.
LRESULT OmahaWnd::OnCancel(WORD, WORD id, HWND, BOOL& handled) {
  CHECK_EQ(id, IDCANCEL);

  if (!is_close_enabled_) {
    return 0;
  }

  MaybeCloseWindow();
  handled = true;
  return 0;
}

void OmahaWnd::Show() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow() || IsWindowVisible()) {
    return;
  }

  CenterWindow(nullptr);
  SetVisible(true);

  if (!::SetForegroundWindow(*this)) {
    PLOG(WARNING) << __func__ << ": ::SetForegroundWindow failed";
  }
}

bool OmahaWnd::OnComplete() {
  if (!IsWindow()) {
    RequestExitProcess();
    return false;
  }

  is_complete_ = true;

  EnableClose(true);

  return true;
}

void OmahaWnd::SetControlAttributes(int control_id,
                                    const ControlAttributes& attributes) {
  if (attributes.is_ignore_entry) {
    return;
  }

  HWND hwnd = GetDlgItem(control_id);
  CHECK(hwnd);
  ::ShowWindow(hwnd, attributes.is_visible ? SW_SHOW : SW_HIDE);
  ::EnableWindow(hwnd, attributes.is_enabled);
  if (attributes.is_button && attributes.is_default) {
    // We ask the dialog manager to give the default push button the focus, to
    // have the <Enter> key work as expected.
    GotoDlgCtrl(hwnd);
    LONG style = ::GetWindowLong(hwnd, GWL_STYLE);
    if (style) {
      style |= BS_DEFPUSHBUTTON;
      ::SetWindowLong(hwnd, GWL_STYLE, style);
    }
  }
}

HRESULT OmahaWnd::EnableClose(bool enable) {
  is_close_enabled_ = enable;
  return EnableSystemCloseButton(is_close_enabled_);
}

HRESULT OmahaWnd::EnableSystemCloseButton(bool enable) {
  HMENU menu = ::GetSystemMenu(*this, false);
  CHECK(menu);
  uint32_t flags = MF_BYCOMMAND;
  flags |= enable ? MF_ENABLED : MF_GRAYED;
  ::EnableMenuItem(menu, SC_CLOSE, flags);
  RecalcLayout();
  return S_OK;
}

HRESULT InitializeCommonControls(DWORD control_classes) {
  INITCOMMONCONTROLSEX init_ctrls = {sizeof(INITCOMMONCONTROLSEX), 0};
  CHECK_EQ(init_ctrls.dwSize, sizeof(init_ctrls));
  init_ctrls.dwICC = control_classes;
  if (!::InitCommonControlsEx(&init_ctrls)) {
    const DWORD error = ::GetLastError();
    if (error != ERROR_CLASS_ALREADY_EXISTS) {
      return HRESULT_FROM_WIN32(error);
    }
  }

  return S_OK;
}

}  // namespace updater::ui
