// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/updater/win/ui/progress_wnd.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/process/launch.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/win/scoped_localalloc.h"
#include "chrome/updater/app/app_install_progress.h"
#include "chrome/updater/app/app_install_util_win.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "chrome/updater/win/ui/ui_constants.h"
#include "chrome/updater/win/ui/ui_ctls.h"
#include "chrome/updater/win/ui/ui_util.h"

namespace updater::ui {

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
    std::size(kCompletionCodesActionPriority) ==
        static_cast<size_t>(
            CompletionCodes::COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL),
    "completion code is missing");

int GetPriority(CompletionCodes code) {
  for (size_t i = 0; i < std::size(kCompletionCodesActionPriority); ++i) {
    if (kCompletionCodesActionPriority[i] == code) {
      return i;
    }
  }

  NOTREACHED_IN_MIGRATION();
  return -1;
}

// Returns true if all apps are cancelled or if the range is empty.
bool AreAllAppsCanceled(const std::vector<AppCompletionInfo>& apps_info) {
  return base::ranges::all_of(apps_info, [](const AppCompletionInfo& app_info) {
    return app_info.is_canceled;
  });
}

}  // namespace

InstallStoppedWnd::InstallStoppedWnd(WTL::CMessageLoop* message_loop,
                                     HWND parent)
    : message_loop_(message_loop), parent_(parent) {
  CHECK(message_loop);
  CHECK(::IsWindow(parent));
}

InstallStoppedWnd::~InstallStoppedWnd() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsWindow()) {
    CloseWindow();
  }
}

BOOL InstallStoppedWnd::PreTranslateMessage(MSG* msg) {
  return CWindow::IsDialogMessage(msg);
}

HRESULT InstallStoppedWnd::CloseWindow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsWindow());
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
  CHECK(id == IDOK || id == IDCANCEL);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!IsWindow());
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
  HideWindowChildren(*this);

  InitializeDialog();

  SetMarqueeMode(true);

  SetDlgItemText(IDC_INSTALLER_STATE_TEXT,
                 GetLocalizedString(IDS_INITIALIZING_BASE).c_str());
  ChangeControlState();

  handled = true;
  return 1;  // Let the system set the focus.
}

// If closing is disabled, then it does not close the window.
// If in a completion state, then the window is closed.
// Otherwise, the InstallStoppedWnd is displayed and the window is closed only
// if the user chooses cancel.
bool ProgressWnd::MaybeCloseWindow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_close_enabled()) {
    return false;
  }

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
      install_stopped_wnd_->SetWindowText(
          GetLocalizedString(IDS_INSTALLATION_STOPPED_WINDOW_TITLE_BASE)
              .c_str());

      install_stopped_wnd_->SetDlgItemText(
          IDOK, GetLocalizedString(IDS_RESUME_INSTALLATION_BASE).c_str());

      install_stopped_wnd_->SetDlgItemText(
          IDCANCEL, GetLocalizedString(IDS_CANCEL_INSTALLATION_BASE).c_str());

      install_stopped_wnd_->SetDlgItemText(
          IDC_INSTALL_STOPPED_TEXT,
          GetLocalizedString(IDS_INSTALL_STOPPED_BASE).c_str());

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
  CHECK(id == IDC_BUTTON1 || id == IDC_BUTTON2 || id == IDC_CLOSE);
  CHECK(events_sink_);

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
          NOTREACHED_IN_MIGRATION();
      }
      break;
    case IDC_BUTTON2:
      switch (cur_state_) {
        case States::STATE_COMPLETE_RESTART_BROWSER:
        case States::STATE_COMPLETE_RESTART_ALL_BROWSERS:
        case States::STATE_COMPLETE_REBOOT:
          break;
        default:
          NOTREACHED_IN_MIGRATION();
      }
      break;
    case IDC_CLOSE:
      switch (cur_state_) {
        case States::STATE_COMPLETE_SUCCESS:
        case States::STATE_COMPLETE_ERROR:
          return CompleteWnd::OnClickedButton(notify_code, id, wnd_ctl,
                                              handled);
        default:
          NOTREACHED_IN_MIGRATION();
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
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

  CHECK_EQ(msg, WM_INSTALL_STOPPED);
  CHECK(wparam == IDOK || wparam == IDCANCEL);
  switch (wparam) {
    case IDOK:
      break;
    case IDCANCEL:
      HandleCancelRequest();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  handled = true;
  return 0;
}

void ProgressWnd::HandleCancelRequest() {
  SetDlgItemText(IDC_INSTALLER_STATE_TEXT,
                 GetLocalizedString(IDS_CANCELING_BASE).c_str());

  if (is_canceled_) {
    return;
  }
  is_canceled_ = true;
  if (events_sink_) {
    events_sink_->DoCancel();
  }
}

void ProgressWnd::OnCheckingForUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }

  cur_state_ = States::STATE_CHECKING_FOR_UPDATE;

  SetDlgItemText(IDC_INSTALLER_STATE_TEXT,
                 GetLocalizedString(IDS_WAITING_TO_CONNECT_BASE).c_str());

  ChangeControlState();
}

void ProgressWnd::OnUpdateAvailable(const std::string& app_id,
                                    const std::u16string& app_name,
                                    const base::Version& version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ProgressWnd::OnWaitingToDownload(const std::string& app_id,
                                      const std::u16string& app_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }
  cur_state_ = States::STATE_WAITING_TO_DOWNLOAD;
  SetDlgItemText(IDC_INSTALLER_STATE_TEXT, L"");
  ChangeControlState();
}

// May be called repeatedly during download.
void ProgressWnd::OnDownloading(
    const std::string& app_id,
    const std::u16string& app_name,
    const std::optional<base::TimeDelta> time_remaining,
    int pos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }

  CHECK(0 <= pos && pos <= 100);

  cur_state_ = States::STATE_DOWNLOADING;

  std::wstring s;

  if (is_canceled_) {
    s = GetLocalizedString(IDS_CANCELING_BASE);
  } else if (!time_remaining) {
    s = GetLocalizedString(IDS_DOWNLOADING_BASE);
  } else if (!time_remaining->InSeconds()) {
    s = GetLocalizedString(IDS_DOWNLOADING_COMPLETED_BASE);
  } else if (!time_remaining->InMinutes()) {
    // Less than one minute remaining.
    s = GetLocalizedStringF(IDS_DOWNLOADING_SHORT_BASE,
                            base::NumberToWString(time_remaining->InSeconds()));
  } else if (!time_remaining->InHours()) {
    // Less than one hour remaining.
    s = GetLocalizedStringF(IDS_DOWNLOADING_LONG_BASE,
                            base::NumberToWString(time_remaining->InMinutes()));
  } else {
    s = GetLocalizedStringF(IDS_DOWNLOADING_VERY_LONG_BASE,
                            base::NumberToWString(time_remaining->InHours()));
  }

  // Reduces flicker by only updating the control if the text has changed.
  std::wstring current_text;
  ui::GetDlgItemText(*this, IDC_INSTALLER_STATE_TEXT, &current_text);
  if (s != current_text) {
    SetDlgItemText(IDC_INSTALLER_STATE_TEXT, s.c_str());
  }

  SetMarqueeMode(pos == 0);
  if (pos > 0) {
    SendDlgItemMessage(IDC_PROGRESS, PBM_SETPOS, pos, 0);
  }

  ChangeControlState();
}

void ProgressWnd::OnWaitingRetryDownload(const std::string& app_id,
                                         const std::u16string& app_name,
                                         const base::Time& next_retry_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }

  cur_state_ = States::STATE_WAITING_TO_DOWNLOAD;
  SetDlgItemText(IDC_INSTALLER_STATE_TEXT, L"");
  ChangeControlState();
}

void ProgressWnd::OnWaitingToInstall(const std::string& app_id,
                                     const std::u16string& app_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }

  if (States::STATE_WAITING_TO_INSTALL != cur_state_) {
    cur_state_ = States::STATE_WAITING_TO_INSTALL;
    SetDlgItemText(IDC_INSTALLER_STATE_TEXT,
                   GetLocalizedString(IDS_WAITING_TO_INSTALL_BASE).c_str());
    ChangeControlState();
  }
}

// May be called repeatedly during install.
void ProgressWnd::OnInstalling(
    const std::string& app_id,
    const std::u16string& app_name,
    const std::optional<base::TimeDelta> time_remaining,
    int pos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }

  if (States::STATE_INSTALLING != cur_state_) {
    cur_state_ = States::STATE_INSTALLING;
    SetDlgItemText(IDC_INSTALLER_STATE_TEXT,
                   GetLocalizedString(IDS_INSTALLING_BASE).c_str());
    ChangeControlState();
  }

  SetMarqueeMode(pos <= 0);
  if (pos > 0) {
    SendDlgItemMessage(IDC_PROGRESS, PBM_SETPOS, pos, 0);
  }
}

void ProgressWnd::OnPause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }

  cur_state_ = States::STATE_PAUSED;
  ChangeControlState();
}

void ProgressWnd::DeterminePostInstallUrls(const ObserverCompletionInfo& info) {
  CHECK(post_install_urls_.empty());
  post_install_urls_.clear();

  for (const AppCompletionInfo& app_info : info.apps_info) {
    if (!app_info.post_install_url.is_empty() &&
        (app_info.completion_code ==
             CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS ||
         app_info.completion_code ==
             CompletionCodes::COMPLETION_CODE_RESTART_BROWSER)) {
      post_install_urls_.push_back(app_info.post_install_url);
    }
  }
  CHECK(!post_install_urls_.empty());
}

CompletionCodes ProgressWnd::GetBundleCompletionCode(
    const ObserverCompletionInfo& info) {
  if (info.completion_code == CompletionCodes::COMPLETION_CODE_ERROR ||
      info.completion_code ==
          CompletionCodes::COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL) {
    return info.completion_code;
  }

  CHECK_EQ(info.completion_code, CompletionCodes::COMPLETION_CODE_SUCCESS);

  return info.apps_info.empty()
             ? kCompletionCodesActionPriority[0]
             : base::ranges::max_element(
                   info.apps_info,
                   [](const auto& app_info1, const auto& app_info2) {
                     return GetPriority(app_info1.completion_code) <
                            GetPriority(app_info2.completion_code);
                   })
                   ->completion_code;
}

std::wstring ProgressWnd::GetBundleCompletionErrorMessages(
    const ObserverCompletionInfo& info) {
  // Combine non-empty app installation completion messages. App-specific
  // installation error message usually gives more details than the generic one.
  std::vector<std::u16string> completion_texts;
  for (const AppCompletionInfo& app_info : info.apps_info) {
    if (!app_info.completion_message.empty()) {
      completion_texts.push_back(app_info.completion_message);
    }
  }

  // Or fallback to the default bundle failure message if nothing is available.
  if (completion_texts.empty() && !info.completion_text.empty()) {
    completion_texts.push_back(info.completion_text);
  }

  return base::UTF16ToWide(base::JoinString(completion_texts, u"\n"));
}

void ProgressWnd::OnComplete(const ObserverCompletionInfo& observer_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CompleteWnd::OnComplete()) {
    return;
  }

  CloseInstallStoppedWindow();

  bool launch_commands_succeeded = LaunchCmdLines(observer_info);

  CompletionCodes overall_completion_code =
      GetBundleCompletionCode(observer_info);
  switch (overall_completion_code) {
    case CompletionCodes::COMPLETION_CODE_SUCCESS:
    case CompletionCodes::COMPLETION_CODE_LAUNCH_COMMAND:
    case CompletionCodes::COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      CompleteWnd::DisplayCompletionDialog(
          true, base::UTF16ToWide(observer_info.completion_text),
          observer_info.help_url.possibly_invalid_spec());
      break;
    case CompletionCodes::COMPLETION_CODE_ERROR:
      if (AreAllAppsCanceled(observer_info.apps_info)) {
        CloseWindow();
        return;
      }
      cur_state_ = States::STATE_COMPLETE_ERROR;
      CompleteWnd::DisplayCompletionDialog(
          false, GetBundleCompletionErrorMessages(observer_info),
          observer_info.help_url.possibly_invalid_spec());
      break;
    case CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS:
      cur_state_ = States::STATE_COMPLETE_RESTART_ALL_BROWSERS;
      SetDlgItemText(IDC_BUTTON1,
                     GetLocalizedString(IDS_RESTART_NOW_BASE).c_str());
      SetDlgItemText(IDC_BUTTON2,
                     GetLocalizedString(IDS_RESTART_LATER_BASE).c_str());
      SetDlgItemText(IDC_COMPLETE_TEXT,
                     GetLocalizedStringF(IDS_TEXT_RESTART_ALL_BROWSERS_BASE,
                                         base::UTF16ToWide(bundle_name()))
                         .c_str());
      DeterminePostInstallUrls(observer_info);
      break;
    case CompletionCodes::COMPLETION_CODE_RESTART_BROWSER:
      cur_state_ = States::STATE_COMPLETE_RESTART_BROWSER;
      SetDlgItemText(IDC_BUTTON1,
                     GetLocalizedString(IDS_RESTART_NOW_BASE).c_str());
      SetDlgItemText(IDC_BUTTON2,
                     GetLocalizedString(IDS_RESTART_LATER_BASE).c_str());
      SetDlgItemText(IDC_COMPLETE_TEXT,
                     GetLocalizedStringF(IDS_TEXT_RESTART_BROWSER_BASE,
                                         base::UTF16ToWide(bundle_name()))
                         .c_str());
      DeterminePostInstallUrls(observer_info);
      break;
    case CompletionCodes::COMPLETION_CODE_REBOOT:
      cur_state_ = States::STATE_COMPLETE_REBOOT;
      SetDlgItemText(IDC_BUTTON1,
                     GetLocalizedString(IDS_RESTART_NOW_BASE).c_str());
      SetDlgItemText(IDC_BUTTON2,
                     GetLocalizedString(IDS_RESTART_LATER_BASE).c_str());
      SetDlgItemText(IDC_COMPLETE_TEXT,
                     GetLocalizedStringF(IDS_TEXT_RESTART_COMPUTER_BASE,
                                         base::UTF16ToWide(bundle_name()))
                         .c_str());
      break;
    case CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      CompleteWnd::DisplayCompletionDialog(
          true,
          GetLocalizedStringF(IDS_TEXT_RESTART_ALL_BROWSERS_BASE,
                              base::UTF16ToWide(bundle_name())),
          observer_info.help_url.possibly_invalid_spec());
      break;
    case CompletionCodes::COMPLETION_CODE_REBOOT_NOTICE_ONLY:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      CompleteWnd::DisplayCompletionDialog(
          true,
          GetLocalizedStringF(IDS_TEXT_RESTART_COMPUTER_BASE,
                              base::UTF16ToWide(bundle_name())),
          observer_info.help_url.possibly_invalid_spec());
      break;
    case CompletionCodes::COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      CompleteWnd::DisplayCompletionDialog(
          true,
          GetLocalizedStringF(IDS_TEXT_RESTART_BROWSER_BASE,
                              base::UTF16ToWide(bundle_name())),
          observer_info.help_url.possibly_invalid_spec());
      break;
    case CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      if (launch_commands_succeeded) {
        CloseWindow();
        return;
      }
      CompleteWnd::DisplayCompletionDialog(
          true, base::UTF16ToWide(observer_info.completion_text),
          observer_info.help_url.possibly_invalid_spec());
      break;
    case CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      CloseWindow();
      return;
  }

  ChangeControlState();
}

HRESULT ProgressWnd::ChangeControlState() {
  for (const auto& ctl : ctls_) {
    SetControlAttributes(ctl.id, ctl.attr[static_cast<size_t>(cur_state_)]);
  }
  return S_OK;
}

HRESULT ProgressWnd::SetMarqueeMode(bool is_marquee) {
  CWindow progress_bar = GetDlgItem(IDC_PROGRESS);
  LONG_PTR style = progress_bar.GetWindowLongPtr(GWL_STYLE);
  if (is_marquee) {
    style |= PBS_MARQUEE;
  } else {
    style &= ~PBS_MARQUEE;
  }
  progress_bar.SetWindowLongPtr(GWL_STYLE, style);
  progress_bar.SendMessage(PBM_SETMARQUEE, is_marquee, 0);

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

}  // namespace updater::ui
