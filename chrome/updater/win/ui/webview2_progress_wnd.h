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
#include "base/win/atl.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/updater/win/ui/progress_wnd.h"
#include "chrome/updater/win/ui/webview2ui.h"

namespace base {
class Version;
}

namespace updater::ui {

class WebView2ProgressWnd : public CWindowImpl<WebView2ProgressWnd>,
                            public AppInstallProgress {
 public:
  // This macro declares a static local variable that would get duplicated in
  // component builds. However, the updater is only meaningful in non-component
  // builds (docs/updater/dev_manual.md), so silence clang's warning.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunique-object-duplication"
  DECLARE_WND_CLASS_EX(L"WebView2ProgressWnd",
                       CS_HREDRAW | CS_VREDRAW,
                       COLOR_WINDOW)
#pragma clang diagnostic pop

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

  BEGIN_MSG_MAP(WebView2ProgressWnd)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MESSAGE_HANDLER(WM_SIZE, OnSize)
    MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
  END_MSG_MAP()

 private:
  // Window message handlers.
  LRESULT OnCreate(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnSize(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnDpiChanged(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnDestroy(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);

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

  base::WeakPtrFactory<WebView2ProgressWnd> weak_ptr_factory_{this};
};

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_WEBVIEW2_PROGRESS_WND_H_
