// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/progress_wnd.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/version.h"
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

// Returns true if all apps are cancelled or if the range is empty.
bool AreAllAppsCanceled(const std::vector<AppCompletionInfo>& apps_info) {
  return std::ranges::all_of(apps_info, [](const AppCompletionInfo& app_info) {
    return app_info.is_canceled;
  });
}

}  // namespace

ProgressWnd::ProgressWnd(WTL::CMessageLoop* message_loop, HWND parent)
    : CompleteWnd(IDD_PROGRESS,
                  ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS,
                  message_loop,
                  parent,
                  base::UTF8ToWide(GetTagLanguage())) {}

ProgressWnd::~ProgressWnd() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsWindow()) {
    // TODO(crbug.com/447657543): replace with a check when the bug is fixed.
    base::debug::DumpWithoutCrashing();
  }
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

  SetControlText(IDC_INSTALLER_STATE_TEXT,
                 GetLocalizedString(IDS_INITIALIZING_BASE, lang()).c_str());
  ChangeControlState();

  handled = true;
  return 1;  // Let the system set the focus.
}

LRESULT ProgressWnd::OnEraseBkgnd(UINT msg,
                                  WPARAM wparam,
                                  LPARAM lparam,
                                  BOOL& handled) {
  const HDC hdc = reinterpret_cast<HDC>(wparam);
  CRect rect;
  GetClientRect(&rect);

  // Fill the entire client area with solid white first to clear any previous
  // artifacts.
  ::FillRect(hdc, &rect, static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH)));

  const int width = rect.Width();
  const int height = rect.Height();

  // Configuration for the rainbow geometry.
  static constexpr size_t kNumStops = 7;
  static constexpr size_t kNumSegments = kNumStops - 1;
  static constexpr size_t kNumVertices = kNumStops * 2;  // Top + bottom row
  static constexpr size_t kNumTriangles = kNumSegments * 2;

  // Layout ratios.
  static constexpr double kYEdgeRatio = 0.69;
  static constexpr double kYCenterRatio = 0.98;

  // Static data for stops and colors.
  static constexpr std::array<double, kNumStops> kStops = {
      0.0, 0.17, 0.32, 0.50, 0.66, 0.81, 1.0};

  static constexpr std::array<COLORREF, kNumStops> kColors = {
      RGB(255, 255, 220),  // Light Yellow
      RGB(255, 240, 210),  // Light Orange
      RGB(255, 225, 225),  // Light Red
      RGB(255, 235, 245),  // Light Pink
      RGB(250, 230, 255),  // Light Magenta
      RGB(240, 230, 255),  // Light Violet
      RGB(220, 255, 255)   // Light Aqua
  };

  // Define the curve parameters:
  // y_edge: The height where the rainbow starts at the left/right edges.
  // y_center: The height where the rainbow is thinnest at the center.
  const int y_edge = static_cast<int>(height * kYEdgeRatio);
  const int y_center = static_cast<int>(height * kYCenterRatio);

  // Define the rainbow mesh vertices.
  std::array<TRIVERTEX, kNumVertices> vertices;
  auto v_span = base::span(vertices);

  auto set_vertex = [](base::span<TRIVERTEX> vertices, size_t index, int x,
                       int y, COLORREF color) {
    TRIVERTEX& vertex = vertices[index];
    vertex.x = x;
    vertex.y = y;
    vertex.Red = static_cast<COLOR16>(GetRValue(color) << 8);
    vertex.Green = static_cast<COLOR16>(GetGValue(color) << 8);
    vertex.Blue = static_cast<COLOR16>(GetBValue(color) << 8);
    vertex.Alpha = 0;
  };

  for (size_t i = 0; i < kNumStops; ++i) {
    const double stop = kStops[i];

    // Use the width of the rect to ensure we hit the right edge perfectly.
    const int x =
        (i == kNumStops - 1) ? rect.right : static_cast<int>(width * stop);

    // Calculate the concave (U-shaped) boundary using a parabola.
    const double factor = (2.0 * stop - 1.0);
    const int y_boundary =
        static_cast<int>(y_center - (y_center - y_edge) * (factor * factor));

    // Top row of the mesh (White boundary following the curve).
    set_vertex(v_span, i, x, y_boundary, RGB(255, 255, 255));

    // Bottom row of the mesh (Light rainbow colors). Stretch to the very
    // bottom.
    set_vertex(v_span, i + kNumStops, x, rect.bottom, kColors[i]);
  }

  // Create the triangles, 2 triangles per segment.
  std::array<GRADIENT_TRIANGLE, kNumTriangles> mesh;
  for (size_t i = 0; i < kNumSegments; ++i) {
    // Triangle 1.
    GRADIENT_TRIANGLE& tri1 = mesh[i * 2];
    tri1.Vertex1 = static_cast<ULONG>(i);
    tri1.Vertex2 = static_cast<ULONG>(i + 1);
    tri1.Vertex3 = static_cast<ULONG>(i + kNumStops);

    // Triangle 2.
    GRADIENT_TRIANGLE& tri2 = mesh[i * 2 + 1];
    tri2.Vertex1 = static_cast<ULONG>(i + 1);
    tri2.Vertex2 = static_cast<ULONG>(i + kNumStops + 1);
    tri2.Vertex3 = static_cast<ULONG>(i + kNumStops);
  }

  ::GradientFill(hdc, v_span.data(), static_cast<ULONG>(v_span.size()),
                 mesh.data(), static_cast<ULONG>(mesh.size()),
                 GRADIENT_FILL_TRIANGLE);

  handled = TRUE;
  return 1;
}

HBRUSH ProgressWnd::OnCtlColorStatic(WTL::CDCHandle dc,
                                     WTL::CStatic wndStatic) {
  dc.SetBkMode(TRANSPARENT);
  return static_cast<HBRUSH>(::GetStockObject(NULL_BRUSH));
}

void ProgressWnd::SetControlText(int id, const std::wstring& text) {
  const HWND hwnd_control = GetDlgItem(id);
  if (!hwnd_control || !::IsWindow(hwnd_control)) {
    return;
  }

  // Reduces flicker by only updating the control if the text has changed.
  std::wstring current_text;
  ui::GetDlgItemText(*this, id, &current_text);
  if (text == current_text) {
    return;
  }

  // Get the control's rectangle relative to the dialog.
  CRect rect;
  ::GetWindowRect(hwnd_control, &rect);
  ScreenToClient(&rect);

  // Invalidate the area on the parent. This forces the parent to redraw the
  // gradient in this specific spot.
  InvalidateRect(&rect, TRUE);

  // Update the text.
  ::SetWindowText(hwnd_control, text.c_str());
}

// If closing is disabled, then it does not close the window.
// If in a completion state, then the window is closed.
// Otherwise, `HandleCancelRequest` is called which attempts to cancel the
// install.
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
    // The UI is not in final state: attempt to cancel the install.
    HandleCancelRequest();
    return false;
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
        case States::STATE_CHECKING_FOR_UPDATE:
        case States::STATE_WAITING_TO_DOWNLOAD:
        case States::STATE_DOWNLOADING:
        case States::STATE_WAITING_TO_INSTALL:
        case States::STATE_INSTALLING:
        case States::STATE_PAUSED:
        case States::STATE_COMPLETE_SUCCESS:
        case States::STATE_COMPLETE_ERROR:
          return CompleteWnd::OnClickedButton(notify_code, id, wnd_ctl,
                                              handled);
        default:
          NOTREACHED();
      }
  }

  handled = true;
  CloseWindow();

  return 0;
}

void ProgressWnd::HandleCancelRequest() {
  SetControlText(IDC_INSTALLER_STATE_TEXT,
                 GetLocalizedString(IDS_CANCELING_BASE, lang()).c_str());

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

  SetControlText(
      IDC_INSTALLER_STATE_TEXT,
      GetLocalizedString(IDS_WAITING_TO_CONNECT_BASE, lang()).c_str());

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
  SetMarqueeMode(true);
  SetControlText(IDC_INSTALLER_STATE_TEXT, L"");
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
    s = GetLocalizedString(IDS_CANCELING_BASE, lang());
  } else if (!time_remaining) {
    s = GetLocalizedString(IDS_DOWNLOADING_BASE, lang());
  } else if (!time_remaining->InSeconds()) {
    s = GetLocalizedString(IDS_DOWNLOADING_COMPLETED_BASE, lang());
  } else if (!time_remaining->InMinutes()) {
    // Less than one minute remaining.
    s = GetLocalizedStringF(IDS_DOWNLOADING_SHORT_BASE,
                            base::NumberToWString(time_remaining->InSeconds()),
                            lang());
  } else if (!time_remaining->InHours()) {
    // Less than one hour remaining.
    s = GetLocalizedStringF(IDS_DOWNLOADING_LONG_BASE,
                            base::NumberToWString(time_remaining->InMinutes()),
                            lang());
  } else {
    s = GetLocalizedStringF(IDS_DOWNLOADING_VERY_LONG_BASE,
                            base::NumberToWString(time_remaining->InHours()),
                            lang());
  }

  SetControlText(IDC_INSTALLER_STATE_TEXT, s.c_str());

  SetMarqueeMode(pos == 0);
  if (pos > 0) {
    SendDlgItemMessage(IDC_PROGRESS, PBM_SETPOS, pos, 0);
  }

  ChangeControlState();
}

void ProgressWnd::OnWaitingRetryDownload(const std::string& app_id,
                                         const std::u16string& app_name,
                                         base::Time next_retry_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }

  cur_state_ = States::STATE_WAITING_TO_DOWNLOAD;
  SetMarqueeMode(true);
  SetControlText(IDC_INSTALLER_STATE_TEXT, L"");
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
    SetMarqueeMode(true);
    SetControlText(
        IDC_INSTALLER_STATE_TEXT,
        GetLocalizedString(IDS_WAITING_TO_INSTALL_BASE, lang()).c_str());
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
    SetControlText(IDC_INSTALLER_STATE_TEXT,
                   GetLocalizedString(IDS_INSTALLING_BASE, lang()).c_str());
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
             : std::ranges::max_element(
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
      SetControlText(IDC_BUTTON1,
                     GetLocalizedString(IDS_RESTART_NOW_BASE, lang()).c_str());
      SetControlText(
          IDC_BUTTON2,
          GetLocalizedString(IDS_RESTART_LATER_BASE, lang()).c_str());
      SetControlText(
          IDC_COMPLETE_TEXT,
          GetLocalizedStringF(IDS_TEXT_RESTART_ALL_BROWSERS_BASE,
                              base::UTF16ToWide(bundle_name()), lang())
              .c_str());
      DeterminePostInstallUrls(observer_info);
      break;
    case CompletionCodes::COMPLETION_CODE_RESTART_BROWSER:
      cur_state_ = States::STATE_COMPLETE_RESTART_BROWSER;
      SetControlText(IDC_BUTTON1,
                     GetLocalizedString(IDS_RESTART_NOW_BASE, lang()).c_str());
      SetControlText(
          IDC_BUTTON2,
          GetLocalizedString(IDS_RESTART_LATER_BASE, lang()).c_str());
      SetControlText(
          IDC_COMPLETE_TEXT,
          GetLocalizedStringF(IDS_TEXT_RESTART_BROWSER_BASE,
                              base::UTF16ToWide(bundle_name()), lang())
              .c_str());
      DeterminePostInstallUrls(observer_info);
      break;
    case CompletionCodes::COMPLETION_CODE_REBOOT:
      cur_state_ = States::STATE_COMPLETE_REBOOT;
      SetControlText(IDC_BUTTON1,
                     GetLocalizedString(IDS_RESTART_NOW_BASE, lang()).c_str());
      SetControlText(
          IDC_BUTTON2,
          GetLocalizedString(IDS_RESTART_LATER_BASE, lang()).c_str());
      SetControlText(
          IDC_COMPLETE_TEXT,
          GetLocalizedStringF(IDS_TEXT_RESTART_COMPUTER_BASE,
                              base::UTF16ToWide(bundle_name()), lang())
              .c_str());
      break;
    case CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      CompleteWnd::DisplayCompletionDialog(
          true,
          GetLocalizedStringF(IDS_TEXT_RESTART_ALL_BROWSERS_BASE,
                              base::UTF16ToWide(bundle_name()), lang()),
          observer_info.help_url.possibly_invalid_spec());
      break;
    case CompletionCodes::COMPLETION_CODE_REBOOT_NOTICE_ONLY:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      CompleteWnd::DisplayCompletionDialog(
          true,
          GetLocalizedStringF(IDS_TEXT_RESTART_COMPUTER_BASE,
                              base::UTF16ToWide(bundle_name()), lang()),
          observer_info.help_url.possibly_invalid_spec());
      break;
    case CompletionCodes::COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      CompleteWnd::DisplayCompletionDialog(
          true,
          GetLocalizedStringF(IDS_TEXT_RESTART_BROWSER_BASE,
                              base::UTF16ToWide(bundle_name()), lang()),
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
  for (const ControlState& ctl : ctls_) {
    const size_t i = std::to_underlying(cur_state_);
    CHECK_LE(i, std::size(ctl.attr));
    SetControlAttributes(ctl.id, ctl.attr[i]);
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

}  // namespace updater::ui
