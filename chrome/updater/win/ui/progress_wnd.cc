// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/progress_wnd.h"

#include "base/i18n/message_formatter.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/updater/updater_constants.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/ui/constants.h"
#include "chrome/updater/win/ui/ui_ctls.h"
#include "chrome/updater/win/ui/util.h"
#include "chrome/updater/win/util.h"

namespace updater {
namespace ui {

namespace {

// The current UI shows to the user only one completion type, even though
// there could be multiple applications in a bundle, where each application
// could have a different completion type. The following array lists the
// completion codes from low priority to high priority. The completion type
// with highest priority will be shown to the user.
constexpr CompletionCodes kCompletionCodesActionPriority[] = {
    CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY,
    CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND,
    CompletionCodes::COMPLETION_CODE_SUCCESS,
    CompletionCodes::COMPLETION_CODE_LAUNCH_COMMAND,
    CompletionCodes::COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY,
    CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY,
    CompletionCodes::COMPLETION_CODE_RESTART_BROWSER,
    CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS,
    CompletionCodes::COMPLETION_CODE_REBOOT_NOTICE_ONLY,
    CompletionCodes::COMPLETION_CODE_REBOOT,
    CompletionCodes::COMPLETION_CODE_ERROR,
    CompletionCodes::COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL,
};

// |kCompletionCodesActionPriority| must have all the values in enumeration
// CompletionCodes. The enumeration value starts from 1 so the array size
// should match the last value in the enumeration.
static_assert(
    base::size(kCompletionCodesActionPriority) ==
        static_cast<size_t>(
            CompletionCodes::COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL),
    "completion code is missing");

int GetPriority(CompletionCodes code) {
  for (size_t i = 0; i < base::size(kCompletionCodesActionPriority); ++i) {
    if (kCompletionCodesActionPriority[i] == code)
      return i;
  }

  NOTREACHED();
  return -1;
}

bool AreAllAppsCanceled(const std::vector<AppCompletionInfo>& apps_info) {
  for (const auto& app_info : apps_info) {
    if (app_info.is_canceled)
      return false;
  }
  return true;
}

}  // namespace

InstallStoppedWnd::InstallStoppedWnd(WTL::CMessageLoop* message_loop,
                                     HWND parent)
    : message_loop_(message_loop), parent_(parent) {
  DCHECK(message_loop);
  DCHECK(::IsWindow(parent));
}

InstallStoppedWnd::~InstallStoppedWnd() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (IsWindow())
    CloseWindow();
}

BOOL InstallStoppedWnd::PreTranslateMessage(MSG* msg) {
  return CWindow::IsDialogMessage(msg);
}

HRESULT InstallStoppedWnd::CloseWindow() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsWindow());
  ::EnableWindow(parent_, true);
  return DestroyWindow() ? S_OK : HRESULTFromLastError();
}

LRESULT InstallStoppedWnd::OnInitDialog(UINT, WPARAM, LPARAM, BOOL& handled) {
  // Simulates the modal behavior by disabling its parent window. The parent
  // window must be enabled before this window is destroyed.
  ::EnableWindow(parent_, false);

  message_loop_->AddMessageFilter(this);

  default_font_.CreatePointFont(90, kDialogFont);
  SendMessageToDescendants(
      WM_SETFONT, reinterpret_cast<WPARAM>(static_cast<HFONT>(default_font_)),
      0);

  CreateOwnerDrawTitleBar(m_hWnd, GetDlgItem(IDC_TITLE_BAR_SPACER), kBkColor);
  SetCustomDlgColors(kTextColor, kBkColor);

  EnableFlatButtons(m_hWnd);

  handled = true;
  return 1;
}

LRESULT InstallStoppedWnd::OnClickButton(WORD, WORD id, HWND, BOOL& handled) {
  DCHECK(id == IDOK || id == IDCANCEL);
  ::PostMessage(parent_, WM_INSTALL_STOPPED, id, 0);
  handled = true;
  return 0;
}

LRESULT InstallStoppedWnd::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
  message_loop_->RemoveMessageFilter(this);
  handled = true;
  return 0;
}

ProgressWnd::ProgressWnd(WTL::CMessageLoop* message_loop, HWND parent)
    : CompleteWnd(IDD_PROGRESS,
                  ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS,
                  message_loop,
                  parent) {}

ProgressWnd::~ProgressWnd() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!IsWindow());
  cur_state_ = States::STATE_END;
}

void ProgressWnd::SetEventSink(ProgressWndEvents* events) {
  events_sink_ = events;
  CompleteWnd::SetEventSink(events_sink_);
}

LRESULT ProgressWnd::OnInitDialog(UINT message,
                                  WPARAM w_param,
                                  LPARAM l_param,
                                  BOOL& handled) {
  // TODO(sorin): remove this when https://crbug.com/1010653 is fixed.
  HideWindowChildren(*this);

  InitializeDialog();

  SetMarqueeMode(true);

  base::string16 state_text;
  ui::LoadString(IDS_INITIALIZING, &state_text);
  SetDlgItemText(IDC_INSTALLER_STATE_TEXT, state_text.c_str());
  ChangeControlState();

  handled = true;
  return 1;  // Let the system set the focus.
}

// If closing is disabled, then it does not close the window.
// If in a completion state, then the window is closed.
// Otherwise, the InstallStoppedWnd is displayed and the window is closed only
// if the user chooses cancel.
bool ProgressWnd::MaybeCloseWindow() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_close_enabled())
    return false;

  if (cur_state_ != States::STATE_COMPLETE_SUCCESS &&
      cur_state_ != States::STATE_COMPLETE_ERROR &&
      cur_state_ != States::STATE_COMPLETE_RESTART_BROWSER &&
      cur_state_ != States::STATE_COMPLETE_RESTART_ALL_BROWSERS &&
      cur_state_ != States::STATE_COMPLETE_REBOOT) {
    // The UI is not in final state: ask the user to proceed with closing it.
    // A modal dialog opens up and sends a message back to this window to
    // communicate the user decision.
    install_stopped_wnd_ =
        std::make_unique<InstallStoppedWnd>(message_loop(), *this);
    HWND hwnd = install_stopped_wnd_->Create(*this);
    if (hwnd) {
      base::string16 title;
      ui::LoadString(IDS_INSTALLATION_STOPPED_WINDOW_TITLE, &title);
      install_stopped_wnd_->SetWindowText(title.c_str());

      base::string16 button_text;
      ui::LoadString(IDS_RESUME_INSTALLATION, &button_text);
      install_stopped_wnd_->SetDlgItemText(IDOK, button_text.c_str());

      ui::LoadString(IDS_CANCEL_INSTALLATION, &button_text);
      install_stopped_wnd_->SetDlgItemText(IDCANCEL, button_text.c_str());

      base::string16 text;
      ui::LoadString(IDS_INSTALL_STOPPED, &text);
      install_stopped_wnd_->SetDlgItemText(IDC_INSTALL_STOPPED_TEXT,
                                           text.c_str());

      install_stopped_wnd_->CenterWindow(*this);
      install_stopped_wnd_->ShowWindow(SW_SHOWDEFAULT);
      return false;
    }
  }

  CloseWindow();
  return true;
}

LRESULT ProgressWnd::OnClickedButton(WORD notify_code,
                                     WORD id,
                                     HWND wnd_ctl,
                                     BOOL& handled) {
  DCHECK(id == IDC_BUTTON1 || id == IDC_BUTTON2 || id == IDC_CLOSE);
  DCHECK(events_sink_);

  switch (id) {
    case IDC_BUTTON1:
      switch (cur_state_) {
        case States::STATE_COMPLETE_RESTART_BROWSER:
          events_sink_->DoRestartBrowser(false, post_install_urls_);
          break;
        case States::STATE_COMPLETE_RESTART_ALL_BROWSERS:
          events_sink_->DoRestartBrowser(true, post_install_urls_);
          break;
        case States::STATE_COMPLETE_REBOOT:
          events_sink_->DoReboot();
          break;
        default:
          NOTREACHED();
      }
      break;
    case IDC_BUTTON2:
      switch (cur_state_) {
        case States::STATE_COMPLETE_RESTART_BROWSER:
        case States::STATE_COMPLETE_RESTART_ALL_BROWSERS:
        case States::STATE_COMPLETE_REBOOT:
          break;
        default:
          NOTREACHED();
      }
      break;
    case IDC_CLOSE:
      switch (cur_state_) {
        case States::STATE_COMPLETE_SUCCESS:
        case States::STATE_COMPLETE_ERROR:
          return CompleteWnd::OnClickedButton(notify_code, id, wnd_ctl,
                                              handled);
          break;
        default:
          NOTREACHED();
      }
      break;
    default:
      NOTREACHED();
  }

  handled = true;
  CloseWindow();

  return 0;
}

LRESULT ProgressWnd::OnInstallStopped(UINT msg,
                                      WPARAM wparam,
                                      LPARAM,
                                      BOOL& handled) {
  install_stopped_wnd_.reset();

  DCHECK(msg == WM_INSTALL_STOPPED);
  DCHECK(wparam == IDOK || wparam == IDCANCEL);
  switch (wparam) {
    case IDOK:
      break;
    case IDCANCEL:
      HandleCancelRequest();
      break;
    default:
      NOTREACHED();
      break;
  }

  handled = true;
  return 0;
}

void ProgressWnd::HandleCancelRequest() {
  base::string16 s;
  ui::LoadString(IDS_CANCELING, &s);
  SetDlgItemText(IDC_INSTALLER_STATE_TEXT, s.c_str());

  if (is_canceled_)
    return;
  is_canceled_ = true;
  if (events_sink_)
    events_sink_->DoCancel();
}

void ProgressWnd::OnCheckingForUpdate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsWindow())
    return;

  cur_state_ = States::STATE_CHECKING_FOR_UPDATE;

  base::string16 s;
  ui::LoadString(IDS_WAITING_TO_CONNECT, &s);
  SetDlgItemText(IDC_INSTALLER_STATE_TEXT, s.c_str());

  ChangeControlState();
}

void ProgressWnd::OnUpdateAvailable(const base::string16& app_id,
                                    const base::string16& app_name,
                                    const base::string16& version_string) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (base::EqualsCaseInsensitiveASCII(app_id,
                                       base::ASCIIToUTF16(kChromeAppId))) {
    HBITMAP app_bitmap = reinterpret_cast<HBITMAP>(
        ::LoadImage(GetCurrentModuleHandle(), MAKEINTRESOURCE(IDB_CHROME),
                    IMAGE_BITMAP, 0, 0, LR_SHARED));
    DCHECK(app_bitmap);
    SendDlgItemMessage(IDC_APP_BITMAP, STM_SETIMAGE, IMAGE_BITMAP,
                       reinterpret_cast<LPARAM>(app_bitmap));
  }

  if (!IsWindow())
    return;
}

void ProgressWnd::OnWaitingToDownload(const base::string16& app_id,
                                      const base::string16& app_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsWindow())
    return;

  cur_state_ = States::STATE_WAITING_TO_DOWNLOAD;

  base::string16 s;
  ui::LoadString(IDS_WAITING_TO_DOWNLOAD, &s);
  SetDlgItemText(IDC_INSTALLER_STATE_TEXT, s.c_str());

  ChangeControlState();
}

// May be called repeatedly during download.
void ProgressWnd::OnDownloading(const base::string16& app_id,
                                const base::string16& app_name,
                                int time_remaining_ms,
                                int pos) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsWindow())
    return;

  DCHECK(0 <= pos && pos <= 100);

  cur_state_ = States::STATE_DOWNLOADING;

  base::string16 s;

  // TODO(sorin): should use base::TimeDelta, https://crbug.com/1016921
  int time_remaining_sec = CeilingDivide(time_remaining_ms, kMsPerSec);
  if (time_remaining_ms < 0) {
    ui::LoadString(IDS_DOWNLOADING, &s);
  } else if (time_remaining_ms == 0) {
    ui::LoadString(IDS_DOWNLOADING_COMPLETED, &s);
  } else if (time_remaining_sec < kSecPerMin) {
    // Less than one minute remaining.
    ui::LoadString(IDS_DOWNLOADING_SHORT, &s);
    s = base::i18n::MessageFormatter::FormatWithNumberedArgs(
        s, time_remaining_sec);
  } else if (time_remaining_sec < kSecondsPerHour) {
    // Less than one hour remaining.
    int time_remaining_minute = CeilingDivide(time_remaining_sec, kSecPerMin);
    ui::LoadString(IDS_DOWNLOADING_LONG, &s);
    s = base::i18n::MessageFormatter::FormatWithNumberedArgs(
        s, time_remaining_minute);
  } else {
    int time_remaining_hour =
        CeilingDivide(time_remaining_sec, kSecondsPerHour);
    ui::LoadString(IDS_DOWNLOADING_VERY_LONG, &s);
    s = base::i18n::MessageFormatter::FormatWithNumberedArgs(
        s, time_remaining_hour);
  }

  // Reduces flicker by only updating the control if the text has changed.
  base::string16 current_text;
  ui::GetDlgItemText(*this, IDC_INSTALLER_STATE_TEXT, &current_text);
  if (s != current_text)
    SetDlgItemText(IDC_INSTALLER_STATE_TEXT, s.c_str());

  SetMarqueeMode(pos == 0);
  if (pos > 0)
    SendDlgItemMessage(IDC_PROGRESS, PBM_SETPOS, pos, 0);

  ChangeControlState();
}

void ProgressWnd::OnWaitingRetryDownload(const base::string16& app_id,
                                         const base::string16& app_name,
                                         const base::Time& next_retry_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsWindow())
    return;

  // Display the next retry time interval if |next_retry_time| is in the future.
  const auto retry_time_in_sec =
      (next_retry_time - base::Time::NowFromSystemTime()).InSeconds();
  if (retry_time_in_sec > 0) {
    base::string16 s;
    ui::LoadString(IDS_DOWNLOAD_RETRY, &s);
    s = base::i18n::MessageFormatter::FormatWithNumberedArgs(
        s, 1 + retry_time_in_sec);
    SetDlgItemText(IDC_INSTALLER_STATE_TEXT, s.c_str());
    ChangeControlState();
  }
}

void ProgressWnd::OnWaitingToInstall(const base::string16& app_id,
                                     const base::string16& app_name,
                                     bool* can_start_install) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(can_start_install);
  if (!IsWindow())
    return;

  if (States::STATE_WAITING_TO_INSTALL != cur_state_) {
    cur_state_ = States::STATE_WAITING_TO_INSTALL;
    base::string16 s;
    ui::LoadString(IDS_WAITING_TO_INSTALL, &s);
    SetDlgItemText(IDC_INSTALLER_STATE_TEXT, s.c_str());
    ChangeControlState();
  }

  *can_start_install = !IsInstallStoppedWindowPresent();
}

// May be called repeatedly during install.
void ProgressWnd::OnInstalling(const base::string16& app_id,
                               const base::string16& app_name,
                               int time_remaining_ms,
                               int pos) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsWindow())
    return;

  if (States::STATE_INSTALLING != cur_state_) {
    cur_state_ = States::STATE_INSTALLING;
    base::string16 s;
    ui::LoadString(IDS_INSTALLING, &s);
    SetDlgItemText(IDC_INSTALLER_STATE_TEXT, s.c_str());
    ChangeControlState();
  }

  SetMarqueeMode(pos <= 0);
  if (pos > 0)
    SendDlgItemMessage(IDC_PROGRESS, PBM_SETPOS, pos, 0);
}

void ProgressWnd::OnPause() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsWindow())
    return;

  cur_state_ = States::STATE_PAUSED;
  ChangeControlState();
}

void ProgressWnd::DeterminePostInstallUrls(const ObserverCompletionInfo& info) {
  DCHECK(post_install_urls_.empty());
  post_install_urls_.clear();

  for (size_t i = 0; i < info.apps_info.size(); ++i) {
    const AppCompletionInfo& app_info = info.apps_info[i];
    if (!app_info.post_install_url.empty() &&
        (app_info.completion_code ==
             CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS ||
         app_info.completion_code ==
             CompletionCodes::COMPLETION_CODE_RESTART_BROWSER)) {
      post_install_urls_.push_back(app_info.post_install_url);
    }
  }
  DCHECK(!post_install_urls_.empty());
}

CompletionCodes ProgressWnd::GetBundleOverallCompletionCode(
    const ObserverCompletionInfo& info) const {
  if (info.completion_code == CompletionCodes::COMPLETION_CODE_ERROR ||
      info.completion_code ==
          CompletionCodes::COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL) {
    return info.completion_code;
  }

  DCHECK(info.completion_code == CompletionCodes::COMPLETION_CODE_SUCCESS);

  CompletionCodes overall_completion_code = kCompletionCodesActionPriority[0];
  for (size_t i = 0; i < info.apps_info.size(); ++i) {
    if (GetPriority(overall_completion_code) <
        GetPriority(info.apps_info[i].completion_code)) {
      overall_completion_code = info.apps_info[i].completion_code;
    }
  }

  return overall_completion_code;
}

void ProgressWnd::OnComplete(const ObserverCompletionInfo& observer_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!CompleteWnd::OnComplete())
    return;

  CloseInstallStoppedWindow();

  bool launch_commands_succeeded = LaunchCmdLines(observer_info);

  using MessageFormatter = base::i18n::MessageFormatter;

  base::string16 s;
  CompletionCodes overall_completion_code =
      GetBundleOverallCompletionCode(observer_info);
  switch (overall_completion_code) {
    case CompletionCodes::COMPLETION_CODE_SUCCESS:
    case CompletionCodes::COMPLETION_CODE_LAUNCH_COMMAND:
    case CompletionCodes::COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      CompleteWnd::DisplayCompletionDialog(true, observer_info.completion_text,
                                           observer_info.help_url);
      break;
    case CompletionCodes::COMPLETION_CODE_ERROR:
      if (AreAllAppsCanceled(observer_info.apps_info)) {
        CloseWindow();
        return;
      }
      cur_state_ = States::STATE_COMPLETE_ERROR;
      CompleteWnd::DisplayCompletionDialog(false, observer_info.completion_text,
                                           observer_info.help_url);
      break;
    case CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS:
      cur_state_ = States::STATE_COMPLETE_RESTART_ALL_BROWSERS;
      ui::LoadString(IDS_RESTART_NOW, &s);
      SetDlgItemText(IDC_BUTTON1, s.c_str());
      ui::LoadString(IDS_RESTART_LATER, &s);
      SetDlgItemText(IDC_BUTTON2, s.c_str());
      ui::LoadString(IDS_TEXT_RESTART_ALL_BROWSERS, &s);
      SetDlgItemText(
          IDC_COMPLETE_TEXT,
          MessageFormatter::FormatWithNumberedArgs(s, bundle_name()).c_str());
      DeterminePostInstallUrls(observer_info);
      break;
    case CompletionCodes::COMPLETION_CODE_RESTART_BROWSER:
      cur_state_ = States::STATE_COMPLETE_RESTART_BROWSER;
      ui::LoadString(IDS_RESTART_NOW, &s);
      SetDlgItemText(IDC_BUTTON1, s.c_str());
      ui::LoadString(IDS_RESTART_LATER, &s);
      SetDlgItemText(IDC_BUTTON2, s.c_str());
      ui::LoadString(IDS_TEXT_RESTART_BROWSER, &s);
      SetDlgItemText(
          IDC_COMPLETE_TEXT,
          MessageFormatter::FormatWithNumberedArgs(s, bundle_name()).c_str());
      SetDlgItemText(IDC_COMPLETE_TEXT, s.c_str());
      DeterminePostInstallUrls(observer_info);
      break;
    case CompletionCodes::COMPLETION_CODE_REBOOT:
      cur_state_ = States::STATE_COMPLETE_REBOOT;
      ui::LoadString(IDS_RESTART_NOW, &s);
      SetDlgItemText(IDC_BUTTON1, s.c_str());
      ui::LoadString(IDS_RESTART_LATER, &s);
      SetDlgItemText(IDC_BUTTON2, s.c_str());
      ui::LoadString(IDS_TEXT_RESTART_COMPUTER, &s);
      SetDlgItemText(
          IDC_COMPLETE_TEXT,
          MessageFormatter::FormatWithNumberedArgs(s, bundle_name()).c_str());
      SetDlgItemText(IDC_COMPLETE_TEXT, s.c_str());
      break;
    case CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      ui::LoadString(IDS_TEXT_RESTART_ALL_BROWSERS, &s);
      CompleteWnd::DisplayCompletionDialog(
          true, MessageFormatter::FormatWithNumberedArgs(s, bundle_name()),
          observer_info.help_url);
      break;
    case CompletionCodes::COMPLETION_CODE_REBOOT_NOTICE_ONLY:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      ui::LoadString(IDS_TEXT_RESTART_COMPUTER, &s);
      CompleteWnd::DisplayCompletionDialog(
          true, MessageFormatter::FormatWithNumberedArgs(s, bundle_name()),
          observer_info.help_url);
      break;
    case CompletionCodes::COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      ui::LoadString(IDS_TEXT_RESTART_BROWSER, &s);
      CompleteWnd::DisplayCompletionDialog(
          true, MessageFormatter::FormatWithNumberedArgs(s, bundle_name()),
          observer_info.help_url);
      break;
    case CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      if (launch_commands_succeeded) {
        CloseWindow();
        return;
      }
      CompleteWnd::DisplayCompletionDialog(true, observer_info.completion_text,
                                           observer_info.help_url);
      break;
    case CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      CloseWindow();
      return;
    default:
      NOTREACHED();
      break;
  }

  ChangeControlState();
}

HRESULT ProgressWnd::LaunchCmdLine(const AppCompletionInfo& app_info) {
  if (app_info.post_install_launch_command_line.empty())
    return S_OK;

  if (app_info.completion_code !=
          CompletionCodes::COMPLETION_CODE_LAUNCH_COMMAND &&
      app_info.completion_code !=
          CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND) {
    return S_OK;
  }

  DCHECK(SUCCEEDED(app_info.error_code));
  DCHECK(!app_info.is_noupdate);

  auto process =
      base::LaunchProcess(app_info.post_install_launch_command_line, {});
  return process.IsValid() ? S_OK : HRESULTFromLastError();
}

bool ProgressWnd::LaunchCmdLines(const ObserverCompletionInfo& info) {
  bool result = true;

  for (size_t i = 0; i < info.apps_info.size(); ++i) {
    const AppCompletionInfo& app_info = info.apps_info[i];
    if (FAILED(app_info.error_code)) {
      continue;
    }
    result &= SUCCEEDED(LaunchCmdLine(app_info));
  }

  return result;
}

HRESULT ProgressWnd::ChangeControlState() {
  for (const auto& ctl : ctls_)
    SetControlAttributes(ctl.id, ctl.attr[static_cast<size_t>(cur_state_)]);
  return S_OK;
}

HRESULT ProgressWnd::SetMarqueeMode(bool is_marquee) {
  CWindow progress_bar = GetDlgItem(IDC_PROGRESS);
  LONG_PTR style = progress_bar.GetWindowLongPtr(GWL_STYLE);
  if (is_marquee)
    style |= PBS_MARQUEE;
  else
    style &= ~PBS_MARQUEE;
  progress_bar.SetWindowLongPtr(GWL_STYLE, style);
  progress_bar.SendMessage(PBM_SETMARQUEE, !!is_marquee, 0);

  return S_OK;
}

bool ProgressWnd::IsInstallStoppedWindowPresent() {
  return install_stopped_wnd_.get() && install_stopped_wnd_->IsWindow();
}

bool ProgressWnd::CloseInstallStoppedWindow() {
  if (IsInstallStoppedWindowPresent()) {
    install_stopped_wnd_->CloseWindow();
    install_stopped_wnd_.reset();
    return true;
  }
  return false;
}

}  // namespace ui
}  // namespace updater
