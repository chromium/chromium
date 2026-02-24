// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_WEBVIEW_PROGRESS_WND_H_
#define CHROME_UPDATER_WIN_UI_WEBVIEW_PROGRESS_WND_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/updater/app/app_install_progress.h"
#include "chrome/updater/win/ui/progress_wnd.h"
#include "chrome/updater/win/ui/webbrowser.h"

namespace updater::ui {

// Represents a HTML/JS/CSS based progress window.
class WebviewProgressWnd : public AppInstallProgress {
 public:
  WebviewProgressWnd();
  WebviewProgressWnd(const WebviewProgressWnd&) = delete;
  WebviewProgressWnd& operator=(const WebviewProgressWnd&) = delete;
  ~WebviewProgressWnd() override;

  void SetEventSink(ProgressWndEvents* ev);
  HRESULT Initialize();
  void Show();
  void set_bundle_name(const std::u16string& bundle_name) {
    bundle_name_ = bundle_name;
  }

  // Overrides for AppInstallProgress.
  void OnCheckingForUpdate() override;
  void OnUpdateAvailable(const std::string& app_id,
                         const std::u16string& app_name,
                         const base::Version& version) override;
  void OnWaitingToDownload(const std::string& app_id,
                           const std::u16string& app_name) override;
  void OnDownloading(const std::string& app_id,
                     const std::u16string& app_name,
                     const std::optional<base::TimeDelta> time_remaining,
                     int pos) override;
  void OnWaitingRetryDownload(const std::string& app_id,
                              const std::u16string& app_name,
                              base::Time next_retry_time) override;
  void OnWaitingToInstall(const std::string& app_id,
                          const std::u16string& app_name) override;
  void OnInstalling(const std::string& app_id,
                    const std::u16string& app_name,
                    const std::optional<base::TimeDelta> time_remaining,
                    int pos) override;
  void OnPause() override;
  void OnComplete(const ObserverCompletionInfo& observer_info) override;

  // Mirrors the corresponding member in `ProgressWnd`.
  HWND m_hWnd;

 private:
  // Helper to execute JS in the webview.
  void UpdateUI(const std::string& status_text,
                int progress_pos,
                bool is_marquee);

  // Callback bound to the webview.
  void HandleCancelRequest();

  std::u16string bundle_name_;
  raw_ptr<ProgressWndEvents> events_sink_;

  // TODO(crbug.com/409590312): use `WebView2` instead of the WebBrowser
  // control.
  WebBrowser wv_;
  bool is_canceled_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_WEBVIEW_PROGRESS_WND_H_
