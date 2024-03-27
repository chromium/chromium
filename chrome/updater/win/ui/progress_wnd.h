// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_PROGRESS_WND_H_
#define CHROME_UPDATER_WIN_UI_PROGRESS_WND_H_

#include <windows.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/app/app_install_progress.h"
#include "chrome/updater/win/ui/complete_wnd.h"
#include "chrome/updater/win/ui/owner_draw_controls.h"
#include "chrome/updater/win/ui/resources/resources.grh"
#include "url/gurl.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace updater::ui {

// Used to communicate between InstallStoppedWnd and ProgressWnd.
inline constexpr unsigned int WM_INSTALL_STOPPED = WM_APP;

class ProgressWndEvents : public CompleteWndEvents {
 public:
  // Restarts the running browsers.
  // If |restart_all_browsers| is true, all known browsers will be restarted.
  virtual bool DoRestartBrowser(bool restart_all_browsers,
                                const std::vector<GURL>& urls) = 0;

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

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<WTL::CMessageLoop> message_loop_ = nullptr;
  HWND parent_ = nullptr;

  WTL::CFont default_font_;
};

// Implements the UI progress window.
class ProgressWnd : public CompleteWnd, public AppInstallProgress {
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
  FRIEND_TEST_ALL_PREFIXES(ProgressWndTest, ClickedButton);
  FRIEND_TEST_ALL_PREFIXES(ProgressWndTest, OnInstallStopped);
  FRIEND_TEST_ALL_PREFIXES(ProgressWndTest, MaybeCloseWindow);
  FRIEND_TEST_ALL_PREFIXES(ProgressWndTest, GetBundleCompletionCode);
  FRIEND_TEST_ALL_PREFIXES(ProgressWndTest, DeterminePostInstallUrls);
  FRIEND_TEST_ALL_PREFIXES(ProgressWndTest, OnCheckingForUpdate);
  FRIEND_TEST_ALL_PREFIXES(ProgressWndTest, OnWaitingToDownload);
  FRIEND_TEST_ALL_PREFIXES(ProgressWndTest, OnWaitingRetryDownload);
  FRIEND_TEST_ALL_PREFIXES(ProgressWndTest, OnPause);
  FRIEND_TEST_ALL_PREFIXES(ProgressWndTest, OnComplete);
  FRIEND_TEST_ALL_PREFIXES(ProgressWndTest, LaunchCmdLine);

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

  static CompletionCodes GetBundleCompletionCode(
      const ObserverCompletionInfo& info);
  static std::wstring GetBundleCompletionErrorMessages(
      const ObserverCompletionInfo& info);

  // Overrides for AppInstallProgress.
  // These functions are called on the thread which owns this window.
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
                              const base::Time& next_retry_time) override;
  void OnWaitingToInstall(const std::string& app_id,
                          const std::u16string& app_name) override;
  void OnInstalling(const std::string& app_id,
                    const std::u16string& app_name,
                    const std::optional<base::TimeDelta> time_remaining,
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

  HRESULT ChangeControlState();
  HRESULT SetMarqueeMode(bool is_marquee);

  bool IsInstallStoppedWindowPresent();

  void HandleCancelRequest();

  // Returns true if the |InstallStoppedWnd| window is closed.
  bool CloseInstallStoppedWindow();

  void DeterminePostInstallUrls(const ObserverCompletionInfo& info);

  SEQUENCE_CHECKER(sequence_checker_);

  States cur_state_ = States::STATE_INIT;

  std::unique_ptr<InstallStoppedWnd> install_stopped_wnd_;

  raw_ptr<ProgressWndEvents> events_sink_ = nullptr;
  std::vector<GURL> post_install_urls_;
  bool is_canceled_ = false;

  struct ControlState {
    const int id;
    const ControlAttributes attr[static_cast<size_t>(States::STATE_END) + 1];
  };

  static const ControlState ctls_[];

  // The speed by which the progress bar moves in marquee mode.
  static constexpr int kMarqueeModeUpdatesMs = 75;
};

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_PROGRESS_WND_H_
