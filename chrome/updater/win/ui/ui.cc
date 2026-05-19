// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/ui.h"

#include <windows.h>

#include <uxtheme.h>

#include <cstdint>

#include "base/check_op.h"
#include "base/logging.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/ui/ui_constants.h"
#include "chrome/updater/win/ui/ui_util.h"

namespace updater::ui {

namespace {

// Creates a font given a point size in tenths of a point at the system DPI.
// Mirrors WTL's `CFont::CreatePointFont` helper.
HFONT CreatePointFontW(int point_size_tenths, LPCWSTR face_name) {
  HDC screen_dc = ::GetDC(nullptr);
  const int logical_pixels_y = ::GetDeviceCaps(screen_dc, LOGPIXELSY);
  ::ReleaseDC(nullptr, screen_dc);
  // Height in logical pixels: -MulDiv(point_size_tenths, dpi, 720)
  // (720 = 72 points/inch * 10 tenths).
  const int height = -::MulDiv(point_size_tenths, logical_pixels_y, 720);
  return ::CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                       face_name);
}

void SetItemFont(HWND parent, int item_id, HFONT font) {
  HWND ctl = ::GetDlgItem(parent, item_id);
  if (ctl) {
    ::SendMessageW(ctl, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  }
}

}  // namespace

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
  ::EnumChildWindows(
      hwnd_parent,
      [](HWND hwnd, LPARAM) {
        CHECK(hwnd);
        const DWORD style =
            static_cast<DWORD>(::GetWindowLongW(hwnd, GWL_STYLE));
        if (style & BS_FLAT) {
          ::SetWindowTheme(hwnd, L"", L"");
        }
        return TRUE;
      },
      0);
}

void HideWindowChildren(HWND hwnd_parent) {
  ::EnumChildWindows(
      hwnd_parent,
      [](HWND hwnd, LPARAM) {
        CHECK(hwnd);
        ::ShowWindow(hwnd, SW_HIDE);
        return TRUE;
      },
      0);
}

OmahaWnd::OmahaWnd(int dialog_id,
                   MessageLoop* message_loop,
                   HWND parent,
                   const std::wstring& lang)
    : IDD(dialog_id),
      message_loop_(message_loop),
      parent_(parent),
      lang_(lang),
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

  if (!Create(IDD, parent_)) {
    VLOG(1) << "Failed to create the window";
    return E_FAIL;
  }
  message_loop_->AddMessageFilter(this);

  return S_OK;
}

BOOL OmahaWnd::PreTranslateMessage(MSG* msg) {
  return ::IsDialogMessageW(hwnd(), msg);
}

void OmahaWnd::InitializeDialog() {
  ::SetWindowTextW(hwnd(),
                   GetInstallerDisplayName(bundle_name(), lang()).c_str());

  CenterWindow(hwnd(), nullptr);
  ui::SetWindowIcon(hwnd(), IDI_APP,
                    base::win::ScopedGDIObject<HICON>::Receiver(hicon_).get());

  // Disable the maximize system menu item.
  HMENU menu = ::GetSystemMenu(hwnd(), FALSE);
  VLOG_IF(2, !menu) << "Failed to find system menu";
  if (menu) {
    ::EnableMenuItem(menu, SC_MAXIMIZE, MF_BYCOMMAND | MF_GRAYED);
  }

  progress_bar_.SubclassWindow(::GetDlgItem(hwnd(), IDC_PROGRESS));

  default_font_.reset(CreatePointFontW(90, kDialogFont));
  SendMessageToDescendants(hwnd(), WM_SETFONT,
                           reinterpret_cast<WPARAM>(default_font_.get()), 0);

  font_.reset(CreatePointFontW(150, kDialogFont));
  SetItemFont(hwnd(), IDC_INSTALLER_STATE_TEXT, font_.get());
  SetItemFont(hwnd(), IDC_INFO_TEXT, font_.get());
  SetItemFont(hwnd(), IDC_COMPLETE_TEXT, font_.get());

  error_font_.reset(CreatePointFontW(110, kDialogFont));
  SetItemFont(hwnd(), IDC_ERROR_TEXT, error_font_.get());

  CreateOwnerDrawTitleBar(hwnd(), ::GetDlgItem(hwnd(), IDC_TITLE_BAR_SPACER),
                          kBkColor);
  SetCustomDlgColors(kTextColor, kBkColor);

  EnableFlatButtons(hwnd());
}

LRESULT OmahaWnd::OnClose(UINT, WPARAM, LPARAM) {
  MaybeCloseWindow();
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

LRESULT OmahaWnd::OnNCDestroy(UINT, WPARAM, LPARAM) {
  message_loop_->RemoveMessageFilter(this);
  MaybeRequestExitProcess();
  SetMsgHandled(FALSE);  // Let default processing handle the WM_NCDESTROY.
  return 0;
}

LRESULT OmahaWnd::OnDpiChanged(UINT, WPARAM wparam, LPARAM lparam) {
  // Resize window to the OS-suggested rect.
  const RECT* new_rect = reinterpret_cast<RECT*>(lparam);
  ::SetWindowPos(hwnd(), nullptr, new_rect->left, new_rect->top,
                 new_rect->right - new_rect->left,
                 new_rect->bottom - new_rect->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);

  // Re-render text/graphics for the new DPI.
  ApplyDpiScaling(/*new_dpi=*/HIWORD(wparam));

  // Resize the title bar.
  RecalcLayout(hwnd(), ::GetDlgItem(hwnd(), IDC_TITLE_BAR_SPACER));

  // Force a full redraw of everything.
  ::RedrawWindow(hwnd(), nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
  return 0;
}

// Called when ESC key is pressed.
void OmahaWnd::OnCancel(UINT, int id, HWND) {
  CHECK_EQ(id, IDCANCEL);

  if (!is_close_enabled_) {
    return;
  }

  MaybeCloseWindow();
}

void OmahaWnd::Show() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow() || ::IsWindowVisible(hwnd())) {
    return;
  }

  CenterWindow(hwnd(), nullptr);
  SetVisible(true);

  if (!::SetForegroundWindow(hwnd())) {
    PLOG(WARNING) << __func__ << ": ::SetForegroundWindow failed";
  }
}

void OmahaWnd::ApplyDpiScaling(int dpi) {
  // Calculate new font height: (DesiredPointSize * dpi) / 72. Use a negative
  // number for height to request the character height.
  const int font_height = -::MulDiv(9, dpi, 72);

  default_font_.reset(::CreateFontW(
      font_height,                  // nHeight
      0,                            // nWidth
      0,                            // nEscapement
      0,                            // nOrientation
      FW_NORMAL,                    // nWeight
      FALSE,                        // bItalic
      FALSE,                        // bUnderline
      0,                            // cStrikeOut
      DEFAULT_CHARSET,              // nCharSet
      OUT_DEFAULT_PRECIS,           // nOutPrecision
      CLIP_DEFAULT_PRECIS,          // nClipPrecision
      CLEARTYPE_QUALITY,            // nQuality (Forces ClearType)
      DEFAULT_PITCH | FF_DONTCARE,  // nPitchAndFamily
      kDialogFont                   // lpszFacename
      ));

  // Tell all child controls to use the new font.
  SendMessageToDescendants(hwnd(), WM_SETFONT,
                           reinterpret_cast<WPARAM>(default_font_.get()), TRUE);
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

  HWND ctl = ::GetDlgItem(hwnd(), control_id);
  CHECK(ctl);
  ::ShowWindow(ctl, attributes.is_visible ? SW_SHOW : SW_HIDE);
  ::EnableWindow(ctl, attributes.is_enabled);
  if (attributes.is_button && attributes.is_default) {
    // Ask the dialog manager to give the default push button the focus, so
    // that the <Enter> key works as expected.
    GotoDlgCtrl(hwnd(), ctl);
    LONG style = ::GetWindowLong(ctl, GWL_STYLE);
    if (style) {
      style |= BS_DEFPUSHBUTTON;
      ::SetWindowLong(ctl, GWL_STYLE, style);
    }
  }
}

HRESULT OmahaWnd::EnableClose(bool enable) {
  is_close_enabled_ = enable;
  return EnableSystemCloseButton(is_close_enabled_);
}

HRESULT OmahaWnd::EnableSystemCloseButton(bool enable) {
  HMENU menu = ::GetSystemMenu(hwnd(), FALSE);
  VLOG_IF(2, !menu) << "Failed to find system menu";
  if (!menu) {
    return E_FAIL;
  }
  ::EnableMenuItem(menu, SC_CLOSE,
                   MF_BYCOMMAND | (enable ? MF_ENABLED : MF_GRAYED));
  RecalcLayout(hwnd(), ::GetDlgItem(hwnd(), IDC_TITLE_BAR_SPACER));
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
