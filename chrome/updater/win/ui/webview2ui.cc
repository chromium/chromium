// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/webview2ui.h"

#include <windows.h>

#include <wrl/client.h>
#include <wrl/event.h>

#include <functional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "third_party/webview2/include/WebView2.h"

namespace updater::ui {

WebView2UI::WebView2UI() = default;

WebView2UI::~WebView2UI() {
  if (webview_controller_) {
    webview_controller_->Close();
  }
}

HRESULT WebView2UI::Create(HWND hwnd_parent,
                           const RECT& rect,
                           base::OnceCallback<void(bool)> on_created) {
  if (!hwnd_parent) {
    return E_INVALIDARG;
  }
  hwnd_parent_ = hwnd_parent;

  return ::CreateCoreWebView2EnvironmentWithOptions(
      nullptr, nullptr, nullptr,
      Microsoft::WRL::Callback<
          ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [this, rect, on_created = std::move(on_created)](
              HRESULT result,
              ICoreWebView2Environment* env) mutable -> HRESULT {
            if (FAILED(result)) {
              std::move(on_created).Run(false);
              return result;
            }
            webview_env_ = env;

            webview_env_->CreateCoreWebView2Controller(
                hwnd_parent_,
                Microsoft::WRL::Callback<
                    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [this, rect, on_created = std::move(on_created)](
                        HRESULT result,
                        ICoreWebView2Controller* controller) mutable
                        -> HRESULT {
                      if (FAILED(result)) {
                        std::move(on_created).Run(false);
                        return result;
                      }

                      webview_controller_ = controller;
                      webview_controller_->get_CoreWebView2(&webview_);

                      webview_->add_WebMessageReceived(
                          Microsoft::WRL::Callback<
                              ICoreWebView2WebMessageReceivedEventHandler>(
                              [this](ICoreWebView2* sender,
                                     ICoreWebView2WebMessageReceivedEventArgs*
                                         args) -> HRESULT {
                                if (web_message_handler_) {
                                  LPWSTR message = nullptr;
                                  if (SUCCEEDED(args->TryGetWebMessageAsString(
                                          &message)) &&
                                      message) {
                                    web_message_handler_.Run(message);
                                    ::CoTaskMemFree(message);
                                  }
                                }
                                return S_OK;
                              })
                              .Get(),
                          &web_message_token_);

                      Resize(rect);

                      std::move(on_created).Run(true);
                      return S_OK;
                    })
                    .Get());
            return S_OK;
          })
          .Get());
}

HRESULT WebView2UI::ExecuteScript(
    const std::wstring& script,
    base::OnceCallback<void(const std::wstring&)> on_complete) {
  if (!webview_) {
    return E_POINTER;
  }

  return webview_->ExecuteScript(
      script.c_str(),
      Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
          [on_complete = std::move(on_complete)](
              HRESULT error_code,
              LPCWSTR result_object_as_json) mutable -> HRESULT {
            if (on_complete) {
              // WebView2 returns the result as a JSON string (e.g., "42",
              // "true", or "{\"foo\":\"bar\"}")
              std::move(on_complete)
                  .Run(result_object_as_json ? result_object_as_json : L"");
            }
            return S_OK;
          })
          .Get());
}

void WebView2UI::SetWebMessageHandler(
    base::RepeatingCallback<void(const std::wstring&)> handler) {
  web_message_handler_ = std::move(handler);
}

HRESULT WebView2UI::Navigate(const std::wstring& url) {
  if (!webview_) {
    return E_POINTER;
  }
  return webview_->Navigate(url.c_str());
}

HRESULT WebView2UI::NavigateToString(const std::wstring& html_content) {
  if (!webview_) {
    return E_POINTER;
  }
  return webview_->NavigateToString(html_content.c_str());
}

void WebView2UI::Resize(const RECT& rect) {
  if (webview_controller_) {
    webview_controller_->put_Bounds(rect);
  }
}

}  // namespace updater::ui
