// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_WEBVIEW2_PROGRESS_WND_H_
#define CHROME_UPDATER_WIN_UI_WEBVIEW2_PROGRESS_WND_H_

#include <windows.h>

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/updater/win/ui/progress_wnd.h"
#include "chrome/updater/win/ui/webview2ui.h"
#include "chrome/updater/win/ui/window_impl.h"
#include "ui/gfx/win/msg_util.h"
#include "ui/gfx/win/window_impl.h"

namespace base {
class Version;
}

namespace updater::ui {

class WebView2ProgressWnd : public gfx::WindowImpl, public AppInstallProgress {
 public:
  WebView2ProgressWnd();
  ~WebView2ProgressWnd() override;

  void SetEventSink(ProgressWndEvents* events);
  void Initialize();
  void Show();
  void set_bundle_name(const std::u16string& bundle_name) {
    bundle_name_ = bundle_name;
  }

  // Overrides for `AppInstallProgress`.
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

  CR_BEGIN_MSG_MAP_EX(WebView2ProgressWnd)
    CR_MESSAGE_HANDLER_EX(WM_CREATE, OnCreate)
    CR_MESSAGE_HANDLER_EX(WM_SIZE, OnSize)
    CR_MESSAGE_HANDLER_EX(WM_DPICHANGED, OnDpiChanged)
    CR_MESSAGE_HANDLER_EX(WM_DESTROY, OnDestroy)
  CR_END_MSG_MAP()

 private:
  // Window message handlers.
  LRESULT OnCreate(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnSize(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnDpiChanged(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnDestroy(UINT msg, WPARAM wparam, LPARAM lparam);

  // WebView2 asynchronous completion callback.
  void OnWebViewCreated(bool success);

  // javascript event handler.
  void OnWebMessageReceived(const std::wstring& message);

  // Updates the UI by calling a javascript function.
  void UpdateUI(const std::string& status_text,
                int progress_pos,
                bool is_marquee);

  std::unique_ptr<WebView2UI> browser_;
  std::u16string bundle_name_;
  raw_ptr<ProgressWndEvents> events_ = nullptr;

  bool is_webview_ready_ = false;
  base::win::ScopedCOMInitializer com_initializer_;

  CR_MSG_MAP_CLASS_DECLARATIONS(WebView2ProgressWnd)
};

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_WEBVIEW2_PROGRESS_WND_H_
