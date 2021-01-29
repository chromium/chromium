// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_PROGRESS_WND_H_
#define CHROME_UPDATER_WIN_UI_PROGRESS_WND_H_

#include <windows.h>

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "base/win/atl.h"
#include "chrome/updater/win/install_progress_observer.h"
#include "chrome/updater/win/ui/complete_wnd.h"
#include "chrome/updater/win/ui/owner_draw_controls.h"
#include "chrome/updater/win/ui/resources/resources.grh"

namespace updater {
namespace ui {

// Used to communicate between InstallStoppedWnd and ProgressWnd.
constexpr unsigned int WM_INSTALL_STOPPED = WM_APP;

class ProgressWndEvents : public CompleteWndEvents {
 public:
  // Restarts the running browsers.
  // If |restart_all_browsers| is true, all known browsers will be restarted.
  virtual bool DoRestartBrowser(bool restart_all_browsers,
                                const std::vector<base::string16>& urls) = 0;

  // Initiates a reboot and returns whether it was initiated successfully.
  virtual bool DoReboot() = 0;

  // Indicates that current operation is cancelled.
  virtual void DoCancel() = 0;
};

// Implements the "Installation Stopped" window. |InstallStoppedWnd| is
// modal relative to its parent. When the window is closed it sends
// a user message to its parent to notify which button the user has clicked on.
class InstallStoppedWnd : public CAxDialogImpl<InstallStoppedWnd>,
                          public OwnerDrawTitleBar,
                          public CustomDlgColors,
                          public WTL::CMessageFilter {
  using Base = CAxDialogImpl<InstallStoppedWnd>;

 public:
  static constexpr int IDD = IDD_INSTALL_STOPPED;

  InstallStoppedWnd(WTL::CMessageLoop* message_loop, HWND parent);
  InstallStoppedWnd(const InstallStoppedWnd&) = delete;
  InstallStoppedWnd& operator=(const InstallStoppedWnd&) = delete;
  ~InstallStoppedWnd() override;

  // Closes the window, handling transition back to the parent window.
  HRESULT CloseWindow();

  // Overrides for WTL::CMessageFilter.
  BOOL PreTranslateMessage(MSG* msg) override;

  BEGIN_MSG_MAP(InstallStoppedWnd)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    COMMAND_ID_HANDLER(IDOK, OnClickButton)
    COMMAND_ID_HANDLER(IDCANCEL, OnClickButton)
    CHAIN_MSG_MAP(Base)
    CHAIN_MSG_MAP(OwnerDrawTitleBar)
    CHAIN_MSG_MAP(CustomDlgColors)
  END_MSG_MAP()

 private:
  LRESULT OnInitDialog(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  LRESULT OnClickButton(WORD notify_code,
                        WORD id,
                        HWND wnd_ctl,
                        BOOL& handled);  // NOLINT
  LRESULT OnDestroy(UINT msg,
                    WPARAM wparam,
                    LPARAM lparam,
                    BOOL& handled);  // NOLINT

  THREAD_CHECKER(thread_checker_);

  WTL::CMessageLoop* message_loop_ = nullptr;
  HWND parent_ = nullptr;

  WTL::CFont default_font_;
};

// Implements the UI progress window.
class ProgressWnd : public CompleteWnd, public InstallProgressObserver {
 public:
  ProgressWnd(WTL::CMessageLoop* message_loop, HWND parent);
  ProgressWnd(const ProgressWnd&) = delete;
  ProgressWnd& operator=(const ProgressWnd&) = delete;
  ~ProgressWnd() override;

  void SetEventSink(ProgressWndEvents* ev);

  BEGIN_MSG_MAP(ProgressWnd)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    MESSAGE_HANDLER(WM_INSTALL_STOPPED, OnInstallStopped)
    COMMAND_HANDLER(IDC_BUTTON1, BN_CLICKED, OnClickedButton)
    COMMAND_HANDLER(IDC_BUTTON2, BN_CLICKED, OnClickedButton)
    COMMAND_HANDLER(IDC_CLOSE, BN_CLICKED, OnClickedButton)
    CHAIN_MSG_MAP(CompleteWnd)
  END_MSG_MAP()

 private:
  // Overrides for InstallProgressObserver.
  // These functions are called on the thread which owns this window.
  void OnCheckingForUpdate() override;
  void OnUpdateAvailable(const base::string16& app_id,
                         const base::string16& app_name,
                         const base::string16& version_string) override;
  void OnWaitingToDownload(const base::string16& app_id,
                           const base::string16& app_name) override;
  void OnDownloading(const base::string16& app_id,
                     const base::string16& app_name,
                     int time_remaining_ms,
                     int pos) override;
  void OnWaitingRetryDownload(const base::string16& app_id,
                              const base::string16& app_name,
                              const base::Time& next_retry_time) override;
  void OnWaitingToInstall(const base::string16& app_id,
                          const base::string16& app_name,
                          bool* can_start_install) override;
  void OnInstalling(const base::string16& app_id,
                    const base::string16& app_name,
                    int time_remaining_ms,
                    int pos) override;
  void OnPause() override;
  void OnComplete(const ObserverCompletionInfo& observer_info) override;

  LRESULT OnInitDialog(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  LRESULT OnInstallStopped(UINT msg,
                           WPARAM wparam,
                           LPARAM lparam,
                           BOOL& handled);  // NOLINT
  LRESULT OnClickedButton(WORD notify_code,
                          WORD id,
                          HWND wnd_ctl,
                          BOOL& handled);  // NOLINT

  // Returns true if this window is closed.
  bool MaybeCloseWindow() override;

  HRESULT LaunchCmdLine(const AppCompletionInfo& app_info);
  bool LaunchCmdLines(const ObserverCompletionInfo& info);
  HRESULT ChangeControlState();
  HRESULT SetMarqueeMode(bool is_marquee);

  bool IsInstallStoppedWindowPresent();

  void HandleCancelRequest();

  // Returns true if the |InstallStoppedWnd| window is closed.
  bool CloseInstallStoppedWindow();

  void DeterminePostInstallUrls(const ObserverCompletionInfo& info);
  CompletionCodes GetBundleOverallCompletionCode(
      const ObserverCompletionInfo& info) const;

  enum class States {
    STATE_INIT = 0,
    STATE_CHECKING_FOR_UPDATE,
    STATE_WAITING_TO_DOWNLOAD,
    STATE_DOWNLOADING,
    STATE_WAITING_TO_INSTALL,
    STATE_INSTALLING,
    STATE_PAUSED,
    STATE_COMPLETE_SUCCESS,
    STATE_COMPLETE_ERROR,
    STATE_COMPLETE_RESTART_BROWSER,
    STATE_COMPLETE_RESTART_ALL_BROWSERS,
    STATE_COMPLETE_REBOOT,
    STATE_END,
  };

  THREAD_CHECKER(thread_checker_);

  States cur_state_ = States::STATE_INIT;

  std::unique_ptr<InstallStoppedWnd> install_stopped_wnd_;

  ProgressWndEvents* events_sink_ = nullptr;
  std::vector<base::string16> post_install_urls_;
  bool is_canceled_ = false;

  struct ControlState {
    const int id;
    const ControlAttributes attr[static_cast<size_t>(States::STATE_END) + 1];
  };

  static const ControlState ctls_[];

  // The speed by which the progress bar moves in marquee mode.
  static constexpr int kMarqueeModeUpdatesMs = 75;
};

}  // namespace ui
}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UI_PROGRESS_WND_H_
