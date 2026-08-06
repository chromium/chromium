// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/webview2ui.h"

#include <windows.h>

#include <wrl/client.h>
#include <wrl/event.h>

#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/scoped_co_mem.h"
#include "third_party/webview2/include/WebView2.h"

namespace updater::ui {

WebView2UI::WebView2UI() = default;

void WebView2UI::ReportCreationResult(HRESULT result) {
  if (on_created_) {
    std::move(on_created_).Run(result);
  }
}

WebView2UI::~WebView2UI() {
  if (webview_) {
    if (web_message_token_.value != 0) {
      webview_->remove_WebMessageReceived(web_message_token_);
    }
    if (navigation_starting_token_.value != 0) {
      webview_->remove_NavigationStarting(navigation_starting_token_);
    }
    if (frame_navigation_starting_token_.value != 0) {
      webview_->remove_FrameNavigationStarting(
          frame_navigation_starting_token_);
    }
    if (new_window_requested_token_.value != 0) {
      webview_->remove_NewWindowRequested(new_window_requested_token_);
    }
    if (permission_requested_token_.value != 0) {
      webview_->remove_PermissionRequested(permission_requested_token_);
    }
    Microsoft::WRL::ComPtr<ICoreWebView2_4> webview_4;
    if (download_starting_token_.value != 0 &&
        SUCCEEDED(webview_.As(&webview_4))) {
      webview_4->remove_DownloadStarting(download_starting_token_);
    }
  }
  if (webview_controller_) {
    webview_controller_->Close();
  }
}

void WebView2UI::Create(HWND hwnd_parent,
                        const RECT& rect,
                        const base::FilePath& user_data_dir,
                        base::OnceCallback<void(HRESULT)> on_created) {
  CHECK(!user_data_dir.empty());
  CHECK(!on_created_);
  if (!hwnd_parent) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_created), E_INVALIDARG));
    return;
  }
  hwnd_parent_ = hwnd_parent;
  on_created_ = std::move(on_created);

  // Capture a WeakPtr to prevent a Use-After-Free crash if the user closes the
  // installer window (destroying this instance) before this asynchronous COM
  // callback completes.
  base::WeakPtr<WebView2UI> weak_this = weak_ptr_factory_.GetWeakPtr();
  HRESULT hr = ::CreateCoreWebView2EnvironmentWithOptions(
      nullptr, user_data_dir.value().c_str(), nullptr,
      Microsoft::WRL::Callback<
          ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [weak_this, rect](HRESULT result,
                            ICoreWebView2Environment* env) -> HRESULT {
            if (weak_this) {
              return weak_this->OnEnvironmentCreated(result, env, rect);
            }
            return S_OK;
          })
          .Get());

  if (FAILED(hr)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&WebView2UI::ReportCreationResult, weak_this, hr));
  }
}

HRESULT WebView2UI::OnEnvironmentCreated(HRESULT result,
                                         ICoreWebView2Environment* env,
                                         const RECT& rect) {
  if (FAILED(result)) {
    ReportCreationResult(result);
    return S_OK;
  }
  webview_env_ = env;

  // Capture a WeakPtr to prevent a Use-After-Free crash if the user closes the
  // installer window (destroying this instance) before this asynchronous COM
  // callback completes.
  base::WeakPtr<WebView2UI> weak_this = weak_ptr_factory_.GetWeakPtr();
  HRESULT hr = webview_env_->CreateCoreWebView2Controller(
      hwnd_parent_,
      Microsoft::WRL::Callback<
          ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
          [weak_this, rect](HRESULT result,
                            ICoreWebView2Controller* controller) -> HRESULT {
            if (weak_this) {
              return weak_this->OnControllerCreated(result, controller, rect);
            } else if (controller) {
              controller->Close();
            }
            return S_OK;
          })
          .Get());

  if (FAILED(hr)) {
    ReportCreationResult(hr);
  }
  return S_OK;
}

HRESULT WebView2UI::OnControllerCreated(HRESULT result,
                                        ICoreWebView2Controller* controller,
                                        const RECT& rect) {
  if (FAILED(result)) {
    ReportCreationResult(result);
    return S_OK;
  }

  webview_controller_ = controller;
  HRESULT hr = InitializeWebView();
  if (FAILED(hr)) {
    ReportCreationResult(hr);
    return S_OK;
  }

  Resize(rect);
  ReportCreationResult(S_OK);
  return S_OK;
}

HRESULT WebView2UI::InitializeWebView() {
  HRESULT hr = webview_controller_->get_CoreWebView2(&webview_);
  if (FAILED(hr) || !webview_) {
    return FAILED(hr) ? hr : E_FAIL;
  }

  Microsoft::WRL::ComPtr<ICoreWebView2Settings> settings;
  hr = webview_->get_Settings(&settings);
  if (FAILED(hr) || !settings) {
    return FAILED(hr) ? hr : E_FAIL;
  }

  hr = settings->put_AreDefaultContextMenusEnabled(FALSE);
  if (FAILED(hr)) {
    return hr;
  }
  hr = settings->put_AreDevToolsEnabled(FALSE);
  if (FAILED(hr)) {
    return hr;
  }
  hr = settings->put_IsZoomControlEnabled(FALSE);
  if (FAILED(hr)) {
    return hr;
  }
  hr = settings->put_IsStatusBarEnabled(FALSE);
  if (FAILED(hr)) {
    return hr;
  }
  hr = settings->put_AreDefaultScriptDialogsEnabled(FALSE);
  if (FAILED(hr)) {
    return hr;
  }
  hr = settings->put_AreHostObjectsAllowed(FALSE);
  if (FAILED(hr)) {
    return hr;
  }
  hr = settings->put_IsBuiltInErrorPageEnabled(FALSE);
  if (FAILED(hr)) {
    return hr;
  }

  Microsoft::WRL::ComPtr<ICoreWebView2Settings3> settings3;
  if (SUCCEEDED(settings.As(&settings3))) {
    hr = settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
    if (FAILED(hr)) {
      return hr;
    }
  }

  Microsoft::WRL::ComPtr<ICoreWebView2Settings4> settings4;
  if (SUCCEEDED(settings.As(&settings4))) {
    hr = settings4->put_IsPasswordAutosaveEnabled(FALSE);
    if (FAILED(hr)) {
      return hr;
    }
    hr = settings4->put_IsGeneralAutofillEnabled(FALSE);
    if (FAILED(hr)) {
      return hr;
    }
  }

  Microsoft::WRL::ComPtr<ICoreWebView2Settings5> settings5;
  if (SUCCEEDED(settings.As(&settings5))) {
    hr = settings5->put_IsPinchZoomEnabled(FALSE);
    if (FAILED(hr)) {
      return hr;
    }
  }

  Microsoft::WRL::ComPtr<ICoreWebView2Settings6> settings6;
  if (SUCCEEDED(settings.As(&settings6))) {
    hr = settings6->put_IsSwipeNavigationEnabled(FALSE);
    if (FAILED(hr)) {
      return hr;
    }
  }

  // Restrict navigations.
  hr = webview_->add_NavigationStarting(
      Microsoft::WRL::Callback<ICoreWebView2NavigationStartingEventHandler>(
          this, &WebView2UI::OnNavigationStarting)
          .Get(),
      &navigation_starting_token_);
  if (FAILED(hr)) {
    return hr;
  }

  // Restrict iframe navigations.
  hr = webview_->add_FrameNavigationStarting(
      Microsoft::WRL::Callback<ICoreWebView2NavigationStartingEventHandler>(
          this, &WebView2UI::OnNavigationStarting)
          .Get(),
      &frame_navigation_starting_token_);
  if (FAILED(hr)) {
    return hr;
  }

  // Block new window creation.
  hr = webview_->add_NewWindowRequested(
      Microsoft::WRL::Callback<ICoreWebView2NewWindowRequestedEventHandler>(
          this, &WebView2UI::OnNewWindowRequested)
          .Get(),
      &new_window_requested_token_);
  if (FAILED(hr)) {
    return hr;
  }

  // Block file downloads.
  Microsoft::WRL::ComPtr<ICoreWebView2_4> webview_4;
  if (SUCCEEDED(webview_.As(&webview_4))) {
    hr = webview_4->add_DownloadStarting(
        Microsoft::WRL::Callback<ICoreWebView2DownloadStartingEventHandler>(
            this, &WebView2UI::OnDownloadStarting)
            .Get(),
        &download_starting_token_);
    if (FAILED(hr)) {
      return hr;
    }
  }

  // Block permission requests (camera, location, notifications).
  hr = webview_->add_PermissionRequested(
      Microsoft::WRL::Callback<ICoreWebView2PermissionRequestedEventHandler>(
          this, &WebView2UI::OnPermissionRequested)
          .Get(),
      &permission_requested_token_);
  if (FAILED(hr)) {
    return hr;
  }

  // Expose WebMessage handler.
  hr = webview_->add_WebMessageReceived(
      Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
          this, &WebView2UI::OnWebMessageReceived)
          .Get(),
      &web_message_token_);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

HRESULT WebView2UI::OnWebMessageReceived(
    ICoreWebView2* sender,
    ICoreWebView2WebMessageReceivedEventArgs* args) {
  if (web_message_handler_) {
    base::win::ScopedCoMem<WCHAR> message;
    if (SUCCEEDED(args->TryGetWebMessageAsString(&message)) && message) {
      web_message_handler_.Run(message.get());
    }
  }
  return S_OK;
}

HRESULT WebView2UI::OnNavigationStarting(
    ICoreWebView2* sender,
    ICoreWebView2NavigationStartingEventArgs* args) {
  base::win::ScopedCoMem<WCHAR> uri;
  CHECK(SUCCEEDED(args->get_Uri(&uri)) && uri)
      << "Failed to get navigation URI.";

  std::wstring_view uri_str(uri.get());
  VLOG(1) << "WebView2 NavigationStarting: " << uri_str;

  const bool allow =
      base::EqualsCaseInsensitiveASCII(uri_str, L"about:blank") ||
      base::StartsWith(uri_str, L"data:text/html",
                       base::CompareCase::INSENSITIVE_ASCII);

  // The UI is built from a static HTML page in memory and does not navigate
  // to external URLs. Any navigation other than the allowed blank or
  // data:text/html strings indicates unexpected page manipulation or
  // compromise. Fail-closed by crashing to prevent further exploitation.
  CHECK(allow) << "WebView2 blocked unauthorized navigation to: " << uri_str;
  return S_OK;
}

HRESULT WebView2UI::OnNewWindowRequested(
    ICoreWebView2* sender,
    ICoreWebView2NewWindowRequestedEventArgs* args) {
  // The UI is self-contained and does not use any browser window features.
  // Any attempt to request a new window is unexpected and indicates potential
  // compromise. Fail-closed by crashing immediately.
  CHECK(false) << "WebView2 blocked unauthorized new window request.";
  return S_OK;
}

HRESULT WebView2UI::OnDownloadStarting(
    ICoreWebView2* sender,
    ICoreWebView2DownloadStartingEventArgs* args) {
  // The UI is self-contained and does not support file downloads.
  // Any attempt to download a file is unexpected and indicates potential
  // compromise. Fail-closed by crashing immediately.
  CHECK(false) << "WebView2 blocked unauthorized download request.";
  return S_OK;
}

HRESULT WebView2UI::OnPermissionRequested(
    ICoreWebView2* sender,
    ICoreWebView2PermissionRequestedEventArgs* args) {
  // The UI is self-contained and does not request any device permissions.
  // Any attempt to request permissions is unexpected and indicates potential
  // compromise. Fail-closed by crashing immediately.
  CHECK(false) << "WebView2 blocked unauthorized permission request.";
  return S_OK;
}

HRESULT WebView2UI::ExecuteScript(
    const std::wstring& script,
    base::OnceCallback<void(const std::wstring&)> on_complete) {
  if (!webview_) {
    return E_POINTER;
  }

  auto split_callback = base::SplitOnceCallback(std::move(on_complete));
  HRESULT hr = webview_->ExecuteScript(
      script.c_str(),
      Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
          [on_complete = std::move(split_callback.first)](
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

  if (FAILED(hr)) {
    if (split_callback.second) {
      std::move(split_callback.second).Run(L"");
    }
  }
  return hr;
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
