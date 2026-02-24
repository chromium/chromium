// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/webbrowser.h"

#include <wrl/client.h>

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"

namespace updater::ui {

WebBrowser::WebBrowser() = default;

int WebBrowser::Initialize(HWND parent) {
  ::OleInitialize(nullptr);

  WNDCLASSEXW wc = {sizeof(WNDCLASSEXW)};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = ::GetModuleHandle(nullptr);
  wc.lpszClassName = L"WebBrowser";
  ::RegisterClassExW(&wc);

  hwnd_ = ::CreateWindowExW(0, L"WebBrowser", L"", WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, parent,
                            nullptr, wc.hInstance, this);
  return hwnd_ && CreateBrowser() ? 0 : ::GetLastError();
}

void WebBrowser::Show() {
  ::ShowWindow(hwnd_, SW_SHOW);
  ::UpdateWindow(hwnd_);
}

WebBrowser::~WebBrowser() {
  if (web_browser_) {
    web_browser_->Quit();
  }
  if (hwnd_) {
    ::DestroyWindow(hwnd_);
  }
  ::OleUninitialize();
}

bool WebBrowser::CreateBrowser() {
  Microsoft::WRL::ComPtr<IOleObject> ole_obj;
  HRESULT hr = ::CoCreateInstance(CLSID_WebBrowser, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&ole_obj));
  if (FAILED(hr)) {
    return false;
  }

  // TODO(crbug.com/409590312): implement a real client site.
  ole_obj->SetClientSite(nullptr);
  hr = ole_obj.As(&web_browser_);
  if (FAILED(hr)) {
    return false;
  }

  RECT rect{};
  ::GetClientRect(hwnd_, &rect);
  hr = ole_obj->DoVerb(OLEIVERB_INPLACEACTIVATE, nullptr, nullptr, 0, hwnd_,
                       &rect);
  return SUCCEEDED(hr);
}

void WebBrowser::Navigate(const std::string& url) {
  if (!web_browser_) {
    return;
  }
  base::win::ScopedVariant empty;
  web_browser_->Navigate(base::win::ScopedBstr(base::UTF8ToWide(url)).Get(),
                         empty.AsInput(), empty.AsInput(), empty.AsInput(),
                         empty.AsInput());
}

void WebBrowser::Eval(const std::string& js) {
  if (!web_browser_) {
    return;
  }

  Microsoft::WRL::ComPtr<IDispatch> doc;
  if (FAILED(web_browser_->get_Document(&doc)) || !doc) {
    return;
  }

  Microsoft::WRL::ComPtr<IHTMLDocument2> html_doc;
  if (FAILED(doc.As(&html_doc))) {
    return;
  }

  Microsoft::WRL::ComPtr<IHTMLWindow2> window;
  if (FAILED(html_doc->get_parentWindow(&window))) {
    return;
  }

  // TODO(crbug.com/409590312): use the result from `execScript`.
  base::win::ScopedVariant unused_result;
  window->execScript(base::win::ScopedBstr(base::UTF8ToWide(js)).Get(),
                     base::win::ScopedBstr(L"JavaScript").Get(),
                     unused_result.Receive());
}

void WebBrowser::SetTitle(const std::string& title) {
  ::SetWindowTextW(hwnd_, base::UTF8ToWide(title).c_str());
}

void WebBrowser::SetSize(int width, int height, int hints) {
  ::SetWindowPos(hwnd_, nullptr, 0, 0, width, height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK WebBrowser::WndProc(HWND hwnd,
                                     UINT msg,
                                     WPARAM wp,
                                     LPARAM lp) {
  WebBrowser* web_browser = nullptr;
  if (msg == WM_NCCREATE) {
    CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lp);
    web_browser = reinterpret_cast<WebBrowser*>(cs->lpCreateParams);
    ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                        reinterpret_cast<LONG_PTR>(web_browser));
  } else {
    // TODO(crbug.com/409590312): use `web_browser` for specific messages.
    web_browser =
        reinterpret_cast<WebBrowser*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  return ::DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace updater::ui
