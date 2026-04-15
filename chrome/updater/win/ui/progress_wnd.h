// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_PROGRESS_WND_H_
#define CHROME_UPDATER_WIN_UI_PROGRESS_WND_H_

#include <windows.h>

// clang-format off
// This needs to be included before ATL headers.
#include "base/win/atl.h"
// clang-format on

#include <atlapp.h>
#include <atlcrack.h>
#include <atlgdi.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
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
class Version;
}  // namespace base

namespace updater::ui {

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
    MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
    MSG_WM_CTLCOLORSTATIC(OnCtlColorStatic)
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
                              base::Time next_retry_time) override;
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
  LRESULT OnClickedButton(WORD notify_code,
                          WORD id,
                          HWND wnd_ctl,
                          BOOL& handled);  // NOLINT
  LRESULT OnEraseBkgnd(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  HBRUSH OnCtlColorStatic(WTL::CDCHandle dc, WTL::CStatic wndStatic);

  void SetControlText(int id, const std::wstring& text);

  // Returns true if this window is closed.
  bool MaybeCloseWindow() override;

  HRESULT ChangeControlState();
  HRESULT SetMarqueeMode(bool is_marquee);

  void HandleCancelRequest();

  void DeterminePostInstallUrls(const ObserverCompletionInfo& info);

  SEQUENCE_CHECKER(sequence_checker_);

  States cur_state_ = States::STATE_INIT;

  raw_ptr<ProgressWndEvents> events_sink_ = nullptr;
  std::vector<GURL> post_install_urls_;
  bool is_canceled_ = false;

  struct ControlState {
   private:
    static constexpr size_t kNumControlAttributes =
        1 + std::to_underlying(States::STATE_END);

   public:
    const int id;
    const std::array<ControlAttributes, kNumControlAttributes> attr;
  };

  static const ControlState ctls_[];

  // The speed by which the progress bar moves in marquee mode.
  static constexpr int kMarqueeModeUpdatesMs = 75;
};

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_PROGRESS_WND_H_
