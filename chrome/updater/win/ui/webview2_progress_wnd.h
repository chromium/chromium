// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_WEBVIEW2_PROGRESS_WND_H_
#define CHROME_UPDATER_WIN_UI_WEBVIEW2_PROGRESS_WND_H_

#include <windows.h>

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
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

  // Checks that the WebView2 UI can be used on this thread: the thread must be
  // a COM single-threaded apartment, and the WebView2 runtime must be
  // installed. Returns a failure `HRESULT` otherwise, in which case the window
  // must not be shown, and the caller should use a different UI instead.
  HRESULT Initialize();

  // Creates and shows the window, and starts creating the WebView2 inside it.
  // `Initialize()` must have succeeded, and the creation failed callback must
  // have been set.
  void Show();

  // Sets the callback which is run on the UI thread if the WebView2 fails to
  // be created after the window has been shown, so that a different UI can
  // take over. The callback runs while this window is still visible, so the
  // new UI can take the foreground; afterwards, this window hides itself and
  // posts a request to close itself, and it no longer quits the UI message
  // loop when it is destroyed. The callback may run from within a WebView2
  // callback, so it must not destroy this object synchronously.
  void set_creation_failed_callback(base::OnceClosure callback) {
    creation_failed_callback_ = std::move(callback);
  }

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
    CR_MESSAGE_HANDLER_EX(WM_WEBVIEW2_CREATE_FAILED, OnWebViewCreateFailed)
  CR_END_MSG_MAP()

 private:
  // Window message handlers.
  LRESULT OnCreate(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnSize(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnDpiChanged(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnDestroy(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnWebViewCreateFailed(UINT msg, WPARAM wparam, LPARAM lparam);

  // WebView2 asynchronous completion callback.
  void OnWebViewCreated(HRESULT result);

  // Handles a WebView2 creation failure by handing the UI over to
  // `creation_failed_callback_` and closing this window.
  void OnWebViewCreationFailed(HRESULT result);

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

  // The WebView2 user data directory. Set by `Initialize()`.
  base::FilePath user_data_dir_;

  base::OnceClosure creation_failed_callback_;

  // True once `creation_failed_callback_` has been run, and a different UI
  // has taken over. Destroying the window must not quit the UI message loop
  // from then on.
  bool superseded_ = false;

  CR_MSG_MAP_CLASS_DECLARATIONS(WebView2ProgressWnd)
};

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_WEBVIEW2_PROGRESS_WND_H_
