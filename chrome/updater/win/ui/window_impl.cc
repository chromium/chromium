// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/window_impl.h"

#include <windows.h>

#include <commctrl.h>

#include "base/check.h"
#include "base/win/current_module.h"

namespace updater::ui {

namespace {

constexpr UINT_PTR kSubclassId = 0xC7;

}  // namespace

void CenterWindow(HWND hwnd, HWND parent) {
  if (!hwnd || !::IsWindow(hwnd)) {
    return;
  }

  RECT window_rect = {};
  ::GetWindowRect(hwnd, &window_rect);
  const int width = window_rect.right - window_rect.left;
  const int height = window_rect.bottom - window_rect.top;

  RECT target_rect = {};
  if (parent && ::IsWindow(parent)) {
    ::GetWindowRect(parent, &target_rect);
  } else {
    HMONITOR monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    if (monitor && ::GetMonitorInfoW(monitor, &mi)) {
      target_rect = mi.rcWork;
    } else {
      target_rect.right = ::GetSystemMetrics(SM_CXSCREEN);
      target_rect.bottom = ::GetSystemMetrics(SM_CYSCREEN);
    }
  }

  const int target_width = target_rect.right - target_rect.left;
  const int target_height = target_rect.bottom - target_rect.top;
  const int x = target_rect.left + (target_width - width) / 2;
  const int y = target_rect.top + (target_height - height) / 2;

  ::SetWindowPos(hwnd, nullptr, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

namespace {

struct SendMessageContext {
  UINT msg;
  WPARAM wparam;
  LPARAM lparam;
};

BOOL CALLBACK SendMessageToDescendantsProc(HWND hwnd, LPARAM lparam) {
  auto* ctx = reinterpret_cast<SendMessageContext*>(lparam);
  ::SendMessageW(hwnd, ctx->msg, ctx->wparam, ctx->lparam);
  return TRUE;
}

}  // namespace

void SendMessageToDescendants(HWND hwnd,
                              UINT msg,
                              WPARAM wparam,
                              LPARAM lparam) {
  SendMessageContext ctx{msg, wparam, lparam};
  ::EnumChildWindows(hwnd, &SendMessageToDescendantsProc,
                     reinterpret_cast<LPARAM>(&ctx));
}

void GotoDlgCtrl(HWND dlg, HWND ctrl) {
  if (dlg && ctrl) {
    ::SendMessageW(dlg, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(ctrl), TRUE);
  }
}

DialogImpl::DialogImpl() = default;

DialogImpl::~DialogImpl() = default;

HWND DialogImpl::Create(int dialog_id, HWND parent) {
  return ::CreateDialogParamW(CURRENT_MODULE(), MAKEINTRESOURCEW(dialog_id),
                              parent, &DialogProc,
                              reinterpret_cast<LPARAM>(this));
}

BOOL DialogImpl::DestroyWindow() {
  if (!IsWindow()) {
    return FALSE;
  }
  return ::DestroyWindow(hwnd_);
}

// static
INT_PTR CALLBACK DialogImpl::DialogProc(HWND hwnd,
                                        UINT msg,
                                        WPARAM wparam,
                                        LPARAM lparam) {
  DialogImpl* self = nullptr;
  if (msg == WM_INITDIALOG) {
    self = reinterpret_cast<DialogImpl*>(lparam);
    self->hwnd_ = hwnd;
    ::SetWindowLongPtrW(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<DialogImpl*>(::GetWindowLongPtrW(hwnd, DWLP_USER));
  }
  if (!self) {
    return FALSE;
  }
  LRESULT result = 0;
  const BOOL handled =
      self->ProcessWindowMessage(hwnd, msg, wparam, lparam, result, 0);
  if (msg == WM_NCDESTROY) {
    ::SetWindowLongPtrW(hwnd, DWLP_USER, 0);
    self->hwnd_ = nullptr;
    return handled ? TRUE : FALSE;
  }
  if (handled) {
    // A few `DLGPROC` messages are documented to return their result value
    // **directly** from the dialog procedure (the `DWLP_MSGRESULT` slot is
    // ignored). For these, the dialog manager interprets the procedure's
    // return value (e.g. as an `HBRUSH` for `WM_CTLCOLOR*`).
    //
    // Everything else uses `DWLP_MSGRESULT` and returns `TRUE` to indicate
    // "handled". `WM_INITDIALOG` is also a direct-return message, but it is
    // already special-cased to fall through to the `return result` path so
    // the handler can control initial focus.
    switch (msg) {
      case WM_INITDIALOG:
      case WM_CTLCOLORMSGBOX:
      case WM_CTLCOLOREDIT:
      case WM_CTLCOLORLISTBOX:
      case WM_CTLCOLORBTN:
      case WM_CTLCOLORDLG:
      case WM_CTLCOLORSCROLLBAR:
      case WM_CTLCOLORSTATIC:
      case WM_COMPAREITEM:
      case WM_VKEYTOITEM:
      case WM_CHARTOITEM:
      case WM_QUERYDRAGICON:
        return static_cast<INT_PTR>(result);
      default:
        ::SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, result);
        return TRUE;
    }
  }
  return FALSE;
}

SubclassedWindow::SubclassedWindow() = default;

SubclassedWindow::~SubclassedWindow() {
  // In the normal flow the subclass is uninstalled in `SubclassProc` when
  // `WM_NCDESTROY` arrives, so `hwnd_` is already null here. If a caller
  // ever destroys this C++ object while its HWND is still alive, leaving the
  // subclass installed would cause `SubclassProc` to dereference a dangling
  // `ref_data` pointer on the next message. Uninstall it defensively so the
  // window continues to function via its original `WNDPROC`.
  if (hwnd_ && ::IsWindow(hwnd_)) {
    ::RemoveWindowSubclass(hwnd_, &SubclassProc, kSubclassId);
  }
  hwnd_ = nullptr;
}

BOOL SubclassedWindow::SubclassWindow(HWND hwnd) {
  CHECK(!hwnd_);
  if (!hwnd || !::IsWindow(hwnd)) {
    return FALSE;
  }
  hwnd_ = hwnd;
  return ::SetWindowSubclass(hwnd, &SubclassProc, kSubclassId,
                             reinterpret_cast<DWORD_PTR>(this));
}

// static
LRESULT CALLBACK SubclassedWindow::SubclassProc(HWND hwnd,
                                                UINT msg,
                                                WPARAM wparam,
                                                LPARAM lparam,
                                                UINT_PTR id,
                                                DWORD_PTR ref_data) {
  auto* self = reinterpret_cast<SubclassedWindow*>(ref_data);
  if (!self) {
    return ::DefSubclassProc(hwnd, msg, wparam, lparam);
  }
  LRESULT result = 0;
  const BOOL handled =
      self->ProcessWindowMessage(hwnd, msg, wparam, lparam, result, 0);
  if (msg == WM_NCDESTROY) {
    ::RemoveWindowSubclass(hwnd, &SubclassProc, id);
    const LRESULT default_result =
        handled ? result : ::DefSubclassProc(hwnd, msg, wparam, lparam);
    self->hwnd_ = nullptr;
    return default_result;
  }
  if (handled) {
    return result;
  }
  return ::DefSubclassProc(hwnd, msg, wparam, lparam);
}

}  // namespace updater::ui
