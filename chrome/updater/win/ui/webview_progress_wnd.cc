// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/webview_progress_wnd.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace updater::ui {

WebviewProgressWnd::WebviewProgressWnd() = default;

void WebviewProgressWnd::SetEventSink(ProgressWndEvents* events) {
  events_sink_ = events;
}

WebviewProgressWnd::~WebviewProgressWnd() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

HRESULT WebviewProgressWnd::Initialize() {
  wv_.Initialize(nullptr);
  m_hWnd = wv_.hwnd();
  return S_OK;
}

void WebviewProgressWnd::Show() {
  // TODO(crbug.com/409590312): bind C++ handlers to allow UI interaction.
  // TODO(crbug.com/409590312): localize HTML.

  wv_.SetTitle(base::UTF16ToUTF8(bundle_name_));
  wv_.SetSize(480, 240, 0);
  wv_.Navigate("data:text/html," + std::string(R"DDDD(
<!DOCTYPE html>
<html>
<head>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
          padding: 20px; background: #f3f3f3; }
        .container { background: white; padding: 20px; border-radius: 8px;
          box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .progress-container { width: 100%; background: #eee; height: 20px;
          border-radius: 10px; overflow: hidden; margin: 20px 0; }
        #progress-bar { width: 0%; height: 100%; background: #0078d4;
          transition: width 0.3s; }
        .marquee { animation: marquee 2s infinite linear;
          width: 30% !important; }
        @keyframes marquee { from { margin-left: -30%; } to
          { margin-left: 100%; } }
        .footer { display: flex; justify-content: flex-end; }
        button { padding: 8px 16px; border: 1px solid #ccc; background: white;
          cursor: pointer; }
        button:hover { background: #f0f0f0; }
    </style>
</head>
<body>
    <div class="container">
        <h2 id="title">Installer</h2>
        <p id="status">Initializing...</p>
        <div class="progress-container">
            <div id="progress-bar"></div>
        </div>
        <div class="footer">
            <button id="cancel-btn" onclick="window.handleCancel()">Cancel
            </button>
        </div>
    </div>
    <script>
        window.updateUI = (status, pos, isMarquee) => {
            document.getElementById('status').innerText = status;
            const bar = document.getElementById('progress-bar');
            bar.style.width = pos + '%';
            if (isMarquee) bar.classList.add('marquee');
            else bar.classList.remove('marquee');
        };
    </script>
</body>
</html>
)DDDD"));
  wv_.Show();
}

void WebviewProgressWnd::UpdateUI(const std::string& status_text,
                                  int progress_pos,
                                  bool is_marquee) {
  wv_.Eval("updateUI('" + status_text + "', " +
           base::NumberToString(progress_pos) + ", " +
           (is_marquee ? "true" : "false") + ")");
}

void WebviewProgressWnd::HandleCancelRequest() {
  if (is_canceled_) {
    return;
  }
  is_canceled_ = true;
  UpdateUI("Canceling...", 0, true);
  if (events_sink_) {
    events_sink_->DoCancel();
  }
}

void WebviewProgressWnd::OnCheckingForUpdate() {
  UpdateUI("Checking for updates...", 0, true);
}

void WebviewProgressWnd::OnDownloading(
    const std::string& app_id,
    const std::u16string& app_name,
    const std::optional<base::TimeDelta> time_remaining,
    int pos) {
  UpdateUI("Downloading " + base::UTF16ToUTF8(app_name) + "...", pos, pos <= 0);
}

void WebviewProgressWnd::OnInstalling(
    const std::string& app_id,
    const std::u16string& app_name,
    const std::optional<base::TimeDelta> time_remaining,
    int pos) {
  UpdateUI("Installing...", pos, pos <= 0);
}

void WebviewProgressWnd::OnComplete(
    const ObserverCompletionInfo& observer_info) {
  std::string message = base::UTF16ToUTF8(observer_info.completion_text);
  UpdateUI(message, 100, false);
  wv_.Eval("document.getElementById('cancel-btn').innerText = 'Close';");
}

void WebviewProgressWnd::OnUpdateAvailable(const std::string&,
                                           const std::u16string&,
                                           const base::Version&) {}
void WebviewProgressWnd::OnWaitingToDownload(const std::string&,
                                             const std::u16string&) {}
void WebviewProgressWnd::OnWaitingRetryDownload(const std::string&,
                                                const std::u16string&,
                                                base::Time) {}
void WebviewProgressWnd::OnWaitingToInstall(const std::string&,
                                            const std::u16string&) {}
void WebviewProgressWnd::OnPause() {}

}  // namespace updater::ui
