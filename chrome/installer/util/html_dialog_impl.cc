// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <mshtmhst.h>
#include <urlmon.h>

#include "base/strings/string_util.h"
#include "base/win/scoped_variant.h"
#include "chrome/installer/util/html_dialog.h"

namespace installer {

// Windows implementation of the HTML dialog class. The main danger with
// using the IE embedded control as a child window of a custom window is that
// it still contains too much browser functionality, allowing the user to do
// things that are not expected of a plain dialog. ShowHTMLDialog API solves
// that problem but gives us a not very customizable frame. We solve that
// using hooks to end up with a robust dialog at the expense of having to do
// the buttons in html itself, like so:
//
// <form onsubmit="submit_it(this); return false;">
//  <input name="accept" type="checkbox" /> My cool option
//  <input name="submit" type="submit" value="[accept]" />
// </form>
//
// function submit_it(f) {
//  if (f.accept.checked) {
//    window.returnValue = 1;  <-- this matches HTML_DLG_ACCEPT
//  } else {
//    window.returnValue = 2;  <-- this matches HTML_DLG_DECLINE
//  }
//  window.close();
// }
//
// Note that on the submit handler you need to set window.returnValue to one of
// the values of DialogResult and call window.close().

class HTMLDialogWin : public HTMLDialog {
 public:
  HTMLDialogWin(const std::wstring& url, const std::wstring& param)
      : url_(url), param_(param) {
    if (!mshtml_)
      mshtml_ = LoadLibrary(L"MSHTML.DLL");
  }

  DialogResult ShowModal(void* parent_window,
                         CustomizationCallback* callback) override {
    int result = HTML_DLG_DECLINE;
    if (!InternalDoDialog(callback, &result))
      return HTML_DLG_ERROR;
    return static_cast<DialogResult>(result);
  }

  std::wstring GetExtraResult() override { return extra_result_; }

 private:
  bool InternalDoDialog(CustomizationCallback* callback, int* result);
  static LRESULT CALLBACK MsgFilter(int code, WPARAM wParam, LPARAM lParam);

  std::wstring url_;
  std::wstring param_;
  static HHOOK hook_;
  static HINSTANCE mshtml_;
  static CustomizationCallback* callback_;
  std::wstring extra_result_;
};

HTMLDialog* CreateNativeHTMLDialog(const std::wstring& url,
                                   const std::wstring& param) {
  return new HTMLDialogWin(url, param);
}

HHOOK HTMLDialogWin::hook_ = nullptr;
HINSTANCE HTMLDialogWin::mshtml_ = nullptr;
HTMLDialogWin::CustomizationCallback* HTMLDialogWin::callback_ = nullptr;

// This hook function gets called for messages bound to the windows that
// ShowHTMLDialog creates. We tell apart the top window because it has the
// system menu style.
LRESULT HTMLDialogWin::MsgFilter(int code, WPARAM wParam, LPARAM lParam) {
  static bool tweak_window = true;
  if (lParam && tweak_window) {
    HWND target_window = reinterpret_cast<MSG*>(lParam)->hwnd;
    if (target_window) {
      LONG_PTR style = ::GetWindowLongPtrW(target_window, GWL_STYLE);
      if (style & WS_SYSMENU) {
        tweak_window = false;
        callback_->OnBeforeDisplay(target_window);
      }
    }
  }
  // Always call the next hook in the chain.
  return ::CallNextHookEx(hook_, code, wParam, lParam);
}

bool HTMLDialogWin::InternalDoDialog(CustomizationCallback* callback,
                                     int* result) {
  if (!mshtml_)
    return false;
  SHOWHTMLDIALOGFN* show_html_dialog = reinterpret_cast<SHOWHTMLDIALOGFN*>(
      GetProcAddress(mshtml_, "ShowHTMLDialog"));
  if (!show_html_dialog)
    return false;

  IMoniker* url_moniker = nullptr;
  ::CreateURLMonikerEx(nullptr, url_.c_str(), &url_moniker, URL_MK_UNIFORM);
  if (!url_moniker)
    return false;

  wchar_t* extra_args = nullptr;
  if (callback) {
    callback->OnBeforeCreation(&extra_args);
    // Sets a windows hook for this thread only.
    hook_ = ::SetWindowsHookEx(WH_GETMESSAGE, MsgFilter, nullptr,
                               GetCurrentThreadId());
    if (hook_)
      callback_ = callback;
  }

  // Pass our parameter to the dialog in the dialogArguments property of
  // the window object.
  base::win::ScopedVariant dialog_args(param_.c_str());

  VARIANT v_result;
  ::VariantInit(&v_result);

  // Creates the window with the embedded IE control in a modal loop.
  HRESULT hr = show_html_dialog(nullptr, url_moniker, dialog_args.AsInput(),
                                extra_args, &v_result);
  url_moniker->Release();

  if (v_result.vt == VT_I4) {
    *result = v_result.intVal;
  } else if (v_result.vt == VT_BSTR) {
    *result = HTML_DLG_EXTRA;
    extra_result_.assign(v_result.bstrVal, SysStringLen(v_result.bstrVal));
  }

  ::VariantClear(&v_result);

  if (hook_) {
    ::UnhookWindowsHookEx(hook_);
    callback_ = nullptr;
    hook_ = nullptr;
  }
  return SUCCEEDED(hr);
}

// EulaHTMLDialog implementation ---------------------------------------------

void EulaHTMLDialog::Customizer::OnBeforeCreation(wchar_t** extra) {}

// The customization of the window consists in removing the close button and
// replacing the existing 'e' icon with the standard informational icon.
void EulaHTMLDialog::Customizer::OnBeforeDisplay(void* window) {
  if (!window)
    return;
  HWND top_window = static_cast<HWND>(window);
  LONG_PTR style = ::GetWindowLongPtrW(top_window, GWL_STYLE);
  ::SetWindowLongPtrW(top_window, GWL_STYLE, style & ~WS_SYSMENU);
  HICON ico = ::LoadIcon(nullptr, IDI_INFORMATION);
  ::SendMessageW(top_window, WM_SETICON, ICON_SMALL,
                 reinterpret_cast<LPARAM>(ico));
}

EulaHTMLDialog::EulaHTMLDialog(const std::wstring& file,
                               const std::wstring& param) {
  dialog_ = CreateNativeHTMLDialog(file, param);
}

EulaHTMLDialog::~EulaHTMLDialog() {
  delete dialog_;
}

EulaHTMLDialog::Outcome EulaHTMLDialog::ShowModal() {
  Customizer customizer;
  HTMLDialog::DialogResult dr = dialog_->ShowModal(nullptr, &customizer);
  if (HTMLDialog::HTML_DLG_ACCEPT == dr)
    return EulaHTMLDialog::ACCEPTED;
  else if (HTMLDialog::HTML_DLG_EXTRA == dr)
    return EulaHTMLDialog::ACCEPTED_OPT_IN;
  else
    return EulaHTMLDialog::REJECTED;
}

}  // namespace installer
