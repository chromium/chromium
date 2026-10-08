// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/webview2_progress_wnd.h"

#include <windows.h>

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/scoped_co_mem.h"
#include "chrome/updater/get_updater_scope.h"
#include "chrome/updater/util/path_util.h"
#include "chrome/updater/win/ui/progress_wnd.h"
#include "chrome/updater/win/ui/ui_util.h"
#include "chrome/updater/win/ui/webview2ui.h"
#include "third_party/webview2/include/WebView2.h"
#include "ui/gfx/geometry/rect.h"

namespace updater::ui {

WebView2ProgressWnd::WebView2ProgressWnd() {
  set_window_style(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
                   WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
  set_initial_class_style(CS_HREDRAW | CS_VREDRAW);
  set_window_class_name(L"WebView2ProgressWnd");
}

WebView2ProgressWnd::~WebView2ProgressWnd() = default;

void WebView2ProgressWnd::SetEventSink(ProgressWndEvents* events) {
  events_ = events;
}

HRESULT WebView2ProgressWnd::Initialize() {
  // WebView2 requires the calling thread to be a single-threaded apartment
  // (STA), so a thread already initialized as MTA can't host it.
  if (!com_initializer_.Succeeded()) {
    LOG(ERROR) << "Thread apartment failed to initialize as STA.";
    return CO_E_NOTINITIALIZED;
  }

  // Fail early if the WebView2 runtime is not installed, instead of finding
  // out asynchronously after the window has been shown.
  base::win::ScopedCoMem<wchar_t> version;
  if (const HRESULT hr =
          ::GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
      FAILED(hr)) {
    LOG(ERROR) << "WebView2 runtime is not available: " << std::hex << hr;
    return hr;
  }
  if (!version.get() || !*version.get()) {
    LOG(ERROR) << "WebView2 runtime reported an empty version.";
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }
  VLOG(1) << "WebView2 runtime version: " << base::WideToUTF8(version.get());

  std::optional<base::FilePath> install_dir =
      GetInstallDirectory(GetUpdaterScope());
  if (!install_dir) {
    LOG(ERROR) << "Failed to resolve installation directory.";
    return E_FAIL;
  }
  user_data_dir_ = install_dir->Append(FILE_PATH_LITERAL("UserData"));
  return S_OK;
}

void WebView2ProgressWnd::Show() {
  CHECK(!user_data_dir_.empty()) << "Initialize() must succeed before Show()";
  CHECK(creation_failed_callback_)
      << "The creation failed callback must be set before Show()";

  RECT rc = {0, 0, 500, 400};
  ::AdjustWindowRect(&rc, window_style(), FALSE);

  // Center the window on the screen.
  const int screen_w = ::GetSystemMetrics(SM_CXSCREEN);
  const int screen_h = ::GetSystemMetrics(SM_CYSCREEN);
  const int width = rc.right - rc.left;
  const int height = rc.bottom - rc.top;

  const gfx::Rect bounds((screen_w - width) / 2, (screen_h - height) / 2, width,
                         height);

  // `Init` is fatal if the window can't be created, so `hwnd()` is valid.
  Init(nullptr, bounds);

  ::SetWindowTextW(hwnd(), L"Google Installer");

  // Show and paint the window.
  ::ShowWindow(hwnd(), SW_SHOW);
  ::UpdateWindow(hwnd());
}

void WebView2ProgressWnd::UpdateUI(const std::string& status_text,
                                   int progress_pos,
                                   bool is_marquee) {
  if (!is_webview_ready_ || !browser_) {
    return;
  }

  std::optional<std::string> json_status = base::WriteJson(status_text);
  if (!json_status) {
    LOG(ERROR) << "Failed to encode status text to JSON.";
    return;
  }

  browser_->ExecuteScript(base::UTF8ToWide(base::StrCat(
      {"updateUI(", *json_status, ", ", base::NumberToString(progress_pos),
       ", ", (is_marquee ? "true" : "false"), ")"})));
}

void WebView2ProgressWnd::OnCheckingForUpdate() {
  UpdateUI("Checking for updates...", 0, true);
}

void WebView2ProgressWnd::OnDownloading(
    const std::string& app_id,
    const std::u16string& app_name,
    const std::optional<base::TimeDelta> time_remaining,
    int pos) {
  UpdateUI(base::StrCat({"Downloading ", base::UTF16ToUTF8(app_name), "..."}),
           pos, pos <= 0);
}

void WebView2ProgressWnd::OnInstalling(
    const std::string& app_id,
    const std::u16string& app_name,
    const std::optional<base::TimeDelta> time_remaining,
    int pos) {
  UpdateUI(base::StrCat({"Installing ", base::UTF16ToUTF8(app_name), "..."}),
           pos, pos <= 0);
}

void WebView2ProgressWnd::OnComplete(
    const ObserverCompletionInfo& observer_info) {
  std::string message = base::UTF16ToUTF8(observer_info.completion_text);
  UpdateUI(message, 100, false);
}

void WebView2ProgressWnd::OnUpdateAvailable(const std::string&,
                                            const std::u16string&,
                                            const base::Version&) {}
void WebView2ProgressWnd::OnWaitingToDownload(const std::string&,
                                              const std::u16string&) {}
void WebView2ProgressWnd::OnWaitingRetryDownload(const std::string&,
                                                 const std::u16string&,
                                                 base::Time) {}
void WebView2ProgressWnd::OnWaitingToInstall(const std::string&,
                                             const std::u16string&) {}
void WebView2ProgressWnd::OnPause() {}

LRESULT WebView2ProgressWnd::OnCreate(UINT, WPARAM, LPARAM) {
  RECT client_rect = {0};
  ::GetClientRect(hwnd(), &client_rect);

  browser_ = std::make_unique<WebView2UI>();
  const HRESULT hr =
      browser_->Create(hwnd(), client_rect, user_data_dir_,
                       base::BindOnce(&WebView2ProgressWnd::OnWebViewCreated,
                                      msg_handler_weak_factory_.GetWeakPtr()));
  if (FAILED(hr)) {
    // The window is still being created, so the failure can't be handled
    // here. Returning -1 is not an option either: `gfx::WindowImpl::Init`
    // treats a failed window creation as fatal. Instead, handle the failure
    // once the UI message loop runs, through the same path as an asynchronous
    // WebView2 creation failure.
    ::PostMessage(hwnd(), WM_WEBVIEW2_CREATE_FAILED, static_cast<WPARAM>(hr),
                  0);
  }
  return 0;
}

LRESULT WebView2ProgressWnd::OnWebViewCreateFailed(UINT,
                                                   WPARAM wparam,
                                                   LPARAM) {
  OnWebViewCreationFailed(static_cast<HRESULT>(wparam));
  return 0;
}

void WebView2ProgressWnd::OnWebViewCreationFailed(HRESULT result) {
  LOG(ERROR) << "Failed to create WebView2: " << std::hex << result;
  CHECK(creation_failed_callback_);

  // Hand the UI over to the fallback. Once superseded, destroying this window
  // must not quit the UI message loop, which the fallback UI keeps using.
  superseded_ = true;

  // Run the callback while this window is still visible: the fallback window
  // can only take the foreground while this process owns the foreground
  // window.
  std::move(creation_failed_callback_).Run();

  // This may be running from within a WebView2 callback, so the window is
  // hidden now and closed asynchronously.
  ::ShowWindow(hwnd(), SW_HIDE);
  ::PostMessage(hwnd(), WM_CLOSE, 0, 0);
}

void WebView2ProgressWnd::OnWebViewCreated(HRESULT result) {
  if (FAILED(result)) {
    OnWebViewCreationFailed(result);
    return;
  }
  is_webview_ready_ = true;

  browser_->SetWebMessageHandler(
      base::BindRepeating(&WebView2ProgressWnd::OnWebMessageReceived,
                          msg_handler_weak_factory_.GetWeakPtr()));

  browser_->NavigateToString(
      std::wstring(LR"DDDD(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Chromium Updater</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background-color: #ffffff;
            color: #333333;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            height: 100vh;
            margin: 0;
            overflow: hidden; /* Prevent scrollbars during resize. */
            user-select: none;
            -webkit-user-select: none;
        }
        .container {
            width: 80%;
            max-width: 400px;
            text-align: center;
        }
        #status-text {
            margin-bottom: 20px;
            font-size: 16px;
            font-weight: 500;
        }
        .progress-bar-container {
            width: 100%;
            height: 8px;
            background-color: #e0e0e0;
            border-radius: 4px;
            overflow: hidden;
            margin-bottom: 20px;
        }
        .progress-bar-fill {
            height: 100%;
            width: 0%;
            background-color: #1a73e8; /* Blue. */
            transition: width 0.3s ease;
        }
        button {
            padding: 8px 16px;
            background-color: #ffffff;
            border: 1px solid #dadce0;
            border-radius: 4px;
            cursor: pointer;
            font-size: 14px;
            color: #1a73e8;
            font-weight: 500;
        }
        button:hover {
            background-color: #f8f9fa;
        }
        .marquee { animation: marquee 2s infinite linear;
          width: 30% !important; }
        @keyframes marquee { from { margin-left: -30%; } to
          { margin-left: 100%; } }
    </style>
</head>
<body>

    <div class="container">
        <div id="status-text">Initializing...</div>

        <div class="progress-bar-container">
            <div id="progress-bar" class="progress-bar-fill"></div>
        </div>

        <button id="cancel-btn">Cancel</button>
    </div>

    <script>
        // Let the C++ backend know the DOM is ready and functions are exposed.
        window.addEventListener('DOMContentLoaded', () => {
            if (window.chrome && window.chrome.webview) {
                window.chrome.webview.postMessage("ui_ready");
            } else {
                console.warn("WebView2 API not found!");
            }
        });

        // Handle the cancel button click.
        document.getElementById('cancel-btn').addEventListener('click', () => {
            if (window.chrome && window.chrome.webview) {
                window.chrome.webview.postMessage("cancel_installation");

                document.getElementById('status-text').innerText =
                    "Cancelling...";
                document.getElementById('cancel-btn').disabled = true;
            }
        });

        // C++ `ExecuteScript` can call the below functions.
        window.updateUI = (status, pos, isMarquee) => {
            document.getElementById('status-text').innerText = status;
            const bar = document.getElementById('progress-bar');
            bar.style.width = pos + '%';
            if (isMarquee) bar.classList.add('marquee');
            else bar.classList.remove('marquee');
            if (pos >= 100)
                document.getElementById('cancel-btn').style.display = 'none';
        };
    </script>
</body>
</html>
)DDDD"));
}

void WebView2ProgressWnd::OnWebMessageReceived(const std::wstring& message) {
  // Handle messages from javascript.
  if (message == L"cancel_installation") {
    VLOG(2) << "User clicked cancel in the HTML UI.";
    if (events_) {
      events_->DoCancel();
    }
    ::PostMessage(hwnd(), WM_CLOSE, 0, 0);
  } else if (message == L"ui_ready") {
    VLOG(2) << "HTML UI DOM is loaded and ready.";
    UpdateUI("Initializing...", 0, true);
  } else {
    LOG(FATAL) << "Unrecognized web message: " << message;
  }
}

LRESULT WebView2ProgressWnd::OnSize(UINT, WPARAM, LPARAM) {
  // Resize WebView2.
  if (browser_) {
    RECT client_rect = {0};
    ::GetClientRect(hwnd(), &client_rect);
    browser_->Resize(client_rect);
  }
  SetMsgHandled(FALSE);  // Let other handlers process `WM_SIZE` if needed.
  return 0;
}

LRESULT WebView2ProgressWnd::OnDpiChanged(UINT, WPARAM, LPARAM lparam) {
  ApplySuggestedWindowRect(hwnd(), lparam);
  return 0;
}

LRESULT WebView2ProgressWnd::OnDestroy(UINT, WPARAM, LPARAM) {
  // Explicitly destroy the browser to release COM references and close the
  // underlying WebView2 processes.
  browser_.reset();

  // Closing the window ends the UI, unless another window has taken over.
  if (!superseded_) {
    ::PostQuitMessage(0);
  }

  SetMsgHandled(FALSE);
  return 0;
}

}  // namespace updater::ui
