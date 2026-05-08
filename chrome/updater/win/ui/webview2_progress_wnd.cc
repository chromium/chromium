// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/webview2_progress_wnd.h"

#include <windows.h>

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/version.h"
#include "base/win/atl.h"
#include "chrome/updater/win/ui/progress_wnd.h"
#include "chrome/updater/win/ui/webview2ui.h"

namespace updater::ui {

WebView2ProgressWnd::WebView2ProgressWnd() = default;

WebView2ProgressWnd::~WebView2ProgressWnd() = default;

void WebView2ProgressWnd::SetEventSink(ProgressWndEvents* events) {
  events_ = events;
}

void WebView2ProgressWnd::Initialize() {}

void WebView2ProgressWnd::Show() {
  RECT rc = {0, 0, 500, 400};
  constexpr DWORD window_styles = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                                  WS_MINIMIZEBOX | WS_CLIPCHILDREN |
                                  WS_CLIPSIBLINGS;

  // Adjust to add space for the title bar.
  ::AdjustWindowRect(&rc, window_styles, FALSE);

  // Center the window on the screen.
  const int screen_w = ::GetSystemMetrics(SM_CXSCREEN);
  const int screen_h = ::GetSystemMetrics(SM_CYSCREEN);
  const int width = rc.right - rc.left;
  const int height = rc.bottom - rc.top;

  RECT center_rc = {(screen_w - width) / 2, (screen_h - height) / 2,
                    ((screen_w - width) / 2) + width,
                    ((screen_h - height) / 2) + height};

  const HWND hwnd =
      Create(nullptr, center_rc, L"Google Installer", window_styles);

  if (!hwnd) {
    // TODO(crbug.com/409590312): Handle UI creation error.
    return;
  }

  // Show and paint the window.
  ShowWindow(SW_SHOW);
  UpdateWindow();
}

void WebView2ProgressWnd::UpdateUI(const std::string& status_text,
                                   int progress_pos,
                                   bool is_marquee) {
  if (!is_webview_ready_ || !browser_) {
    return;
  }

  browser_->ExecuteScript(base::UTF8ToWide(base::StrCat(
      {"updateUI('", status_text, "', ", base::NumberToString(progress_pos),
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

LRESULT WebView2ProgressWnd::OnCreate(UINT msg,
                                      WPARAM wparam,
                                      LPARAM lparam,
                                      BOOL& handled) {
  // WebView2 requires COM to be initialized on the calling thread.
  // Initialize COM as single-threaded apartment (STA).
  if (!com_initializer_.Succeeded()) {
    LOG(ERROR) << "Thread apartment failed to initialize as STA. "
               << "WebView2 may fail to display.";
    handled = TRUE;
    return -1;
  }

  RECT client_rect = {0};
  ::GetClientRect(m_hWnd, &client_rect);

  browser_ = std::make_unique<WebView2UI>();
  const HRESULT hr =
      browser_->Create(m_hWnd, client_rect,
                       base::BindOnce(&WebView2ProgressWnd::OnWebViewCreated,
                                      weak_ptr_factory_.GetWeakPtr()));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create WebView2: " << std::hex << hr;
    // TODO(crbug.com/409590312): Handle UI creation error.
  }

  handled = TRUE;
  return 0;
}

void WebView2ProgressWnd::OnWebViewCreated(bool success) {
  if (!success) {
    LOG(ERROR) << "WebView2 creation callback reported failure.";
    return;
  }
  is_webview_ready_ = true;

  browser_->SetWebMessageHandler(
      base::BindRepeating(&WebView2ProgressWnd::OnWebMessageReceived,
                          weak_ptr_factory_.GetWeakPtr()));

  browser_->NavigateToString(std::wstring(LR"DDDD(
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
    ::PostMessage(m_hWnd, WM_CLOSE, 0, 0);
  } else if (message == L"ui_ready") {
    VLOG(2) << "HTML UI DOM is loaded and ready.";
    UpdateUI("Initializing...", 0, true);
  }
}

LRESULT WebView2ProgressWnd::OnSize(UINT msg,
                                    WPARAM wparam,
                                    LPARAM lparam,
                                    BOOL& handled) {
  // Resize WebView2.
  if (browser_) {
    RECT client_rect = {0};
    ::GetClientRect(m_hWnd, &client_rect);
    browser_->Resize(client_rect);
  }
  handled = FALSE;  // Let other handlers process `WM_SIZE` if needed.
  return 0;
}

LRESULT WebView2ProgressWnd::OnDpiChanged(UINT msg,
                                          WPARAM wparam,
                                          LPARAM lparam,
                                          BOOL& handled) {
  // `lparam` is a pointer to a RECT containing the suggested new window bounds.
  RECT* const suggested_rect = reinterpret_cast<RECT*>(lparam);

  // Apply the suggested bounds.
  ::SetWindowPos(m_hWnd, nullptr, suggested_rect->left, suggested_rect->top,
                 suggested_rect->right - suggested_rect->left,
                 suggested_rect->bottom - suggested_rect->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);

  handled = TRUE;
  return 0;
}

LRESULT WebView2ProgressWnd::OnDestroy(UINT msg,
                                       WPARAM wparam,
                                       LPARAM lparam,
                                       BOOL& handled) {
  // Explicitly destroy the browser to release COM references and close the
  // underlying WebView2 processes.
  browser_.reset();

  ::PostQuitMessage(0);

  handled = FALSE;
  return 0;
}

}  // namespace updater::ui
