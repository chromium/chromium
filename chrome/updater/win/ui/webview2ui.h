// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_WEBVIEW2UI_H_
#define CHROME_UPDATER_WIN_UI_WEBVIEW2UI_H_

#include <windows.h>

#include <wrl/client.h>

#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "third_party/webview2/include/WebView2.h"

namespace updater::ui {

class WebView2UI {
 public:
  WebView2UI();
  ~WebView2UI();

  // Asynchronous creation.
  void Create(HWND hwnd_parent,
              const RECT& rect,
              const base::FilePath& user_data_dir,
              base::OnceCallback<void(HRESULT)> on_created);

  // Navigates to the specified URL.
  HRESULT Navigate(const std::wstring& url);

  // Navigates to the specified HTML content string. This is more reliable
  // than using data:text/html URLs for complex HTML content.
  HRESULT NavigateToString(const std::wstring& html_content);

  // Resize the WebView. Typically called from the parent window `WM_SIZE`
  // handler.
  void Resize(const RECT& rect);

  // Executes javascript asynchronously. The result if any is passed to the
  // callback as a JSON string.
  HRESULT ExecuteScript(const std::wstring& script,
                        base::OnceCallback<void(const std::wstring&)>
                            on_complete = base::DoNothing());

  // Sets the handler for messages from javascript via
  // `window.chrome.webview.postMessage()`.
  void SetWebMessageHandler(
      base::RepeatingCallback<void(const std::wstring&)> handler);

  ICoreWebView2* GetWebView() const { return webview_.Get(); }

 private:
  HRESULT OnEnvironmentCreated(HRESULT result,
                               ICoreWebView2Environment* env,
                               const RECT& rect);
  HRESULT OnControllerCreated(HRESULT result,
                              ICoreWebView2Controller* controller,
                              const RECT& rect);

  HRESULT InitializeWebView();

  void ReportCreationResult(HRESULT result);

  HRESULT OnWebMessageReceived(ICoreWebView2* sender,
                               ICoreWebView2WebMessageReceivedEventArgs* args);
  HRESULT OnNavigationStarting(ICoreWebView2* sender,
                               ICoreWebView2NavigationStartingEventArgs* args);
  HRESULT OnNewWindowRequested(ICoreWebView2* sender,
                               ICoreWebView2NewWindowRequestedEventArgs* args);
  HRESULT OnDownloadStarting(ICoreWebView2* sender,
                             ICoreWebView2DownloadStartingEventArgs* args);
  HRESULT OnPermissionRequested(
      ICoreWebView2* sender,
      ICoreWebView2PermissionRequestedEventArgs* args);

  HWND hwnd_parent_ = nullptr;

  Microsoft::WRL::ComPtr<ICoreWebView2Environment> webview_env_;
  Microsoft::WRL::ComPtr<ICoreWebView2Controller> webview_controller_;
  Microsoft::WRL::ComPtr<ICoreWebView2> webview_;

  base::OnceCallback<void(HRESULT)> on_created_;

  base::RepeatingCallback<void(const std::wstring&)> web_message_handler_;
  EventRegistrationToken web_message_token_ = {};
  EventRegistrationToken navigation_starting_token_ = {};
  EventRegistrationToken frame_navigation_starting_token_ = {};
  EventRegistrationToken new_window_requested_token_ = {};
  EventRegistrationToken download_starting_token_ = {};
  EventRegistrationToken permission_requested_token_ = {};

  base::WeakPtrFactory<WebView2UI> weak_ptr_factory_{this};

  WebView2UI(const WebView2UI&) = delete;
  WebView2UI& operator=(const WebView2UI&) = delete;
};

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_WEBVIEW2UI_H_
