// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_controller_win.h"

#include <windows.h>

#include <cmath>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/ui/webui/default_browser/guided_setter_overlay_window_win.h"
#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_layout_utils.h"
#include "chrome/installer/util/shell_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "ui/display/screen.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/win/hwnd_util.h"
#include "url/gurl.h"

namespace {

bool IsDefaultBrowserWebUiUrl(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host().starts_with("default-browser");
}

}  // namespace

VisualGuidedSetterControllerWin::VisualGuidedSetterControllerWin(
    views::Widget* parent_widget)
    : parent_widget_(parent_widget),
      is_continuous_docking_enabled_(
          default_browser::IsVisualGuidedSetterDockingEnabled()) {
  CHECK(parent_widget_);
  if (is_continuous_docking_enabled_) {
    widget_observation_.Observe(parent_widget_);
  }

  CHECK(parent_widget_->GetNativeWindow());
  chrome_hwnd_ = views::HWNDForNativeWindow(parent_widget_->GetNativeWindow());
  CHECK(::IsWindow(chrome_hwnd_));
}

VisualGuidedSetterControllerWin::~VisualGuidedSetterControllerWin() {
  Stop();
}

void VisualGuidedSetterControllerWin::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_running_) {
    return;
  }

  CHECK(parent_widget_ && chrome_hwnd_);

  is_running_ = true;
  is_degraded_ = false;
  outcome_ = std::nullopt;

  if (!overlay_) {
    overlay_ = std::make_unique<GuidedSetterOverlayWindowWin>(
        parent_widget_->GetNativeWindow());
    UpdateOverlayColor();
  }

  LaunchSettings();
  StartFindSettingsWindow();
}

void VisualGuidedSetterControllerWin::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_running_) {
    return;
  }
  TearDownInternal();
}

void VisualGuidedSetterControllerWin::SetTopmostPolicy(TopmostPolicy policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  topmost_policy_ = policy;
  if (is_running_ && IsSettingsWindowValid()) {
    UpdateDockedLayout();
  }
}

void VisualGuidedSetterControllerWin::SetWebContents(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::WebContentsObserver::Observe(web_contents);

  if (is_running_) {
    UpdateDockedLayout();
  }
}

void VisualGuidedSetterControllerWin::SetErrorCallback(ErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  error_callback_ = std::move(callback);
}

void VisualGuidedSetterControllerWin::NotifyErrorState(bool is_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (last_reported_error_ != is_error) {
    last_reported_error_ = is_error;
    if (error_callback_) {
      error_callback_.Run(is_error);
    }
  }
}

void VisualGuidedSetterControllerWin::SetAnchorRectInWebUi(
    const gfx::Rect& rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  anchor_rect_in_webui_ = rect;
  has_anchor_rect_ = !rect.IsEmpty();
  if (!is_running_) {
    return;
  }
  UpdateDockedLayout();
}

void VisualGuidedSetterControllerWin::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(widget, parent_widget_);
  if (!is_running_) {
    return;
  }
  UpdateDockedLayout();
}

void VisualGuidedSetterControllerWin::OnWidgetDestroyed(views::Widget* widget) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(widget, parent_widget_);
  Stop();
  widget_observation_.Reset();
  parent_widget_ = nullptr;
  chrome_hwnd_ = nullptr;
}

void VisualGuidedSetterControllerWin::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(widget, parent_widget_);
  if (!is_running_) {
    return;
  }
  if (!visible) {
    OnWebContentsHidden();
    return;
  }
  if (web_contents() &&
      web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
    return;
  }
  if (IsSettingsWindowValid()) {
    ResumeLayoutObservation();
  }
  UpdateDockedLayout();
}

void VisualGuidedSetterControllerWin::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(widget, parent_widget_);
  if (!is_running_) {
    return;
  }
  last_known_chrome_active_ = active;
  UpdateDockedLayout();
}

void VisualGuidedSetterControllerWin::OnWidgetShowStateChanged(
    views::Widget* widget) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(widget, parent_widget_);
  if (!is_running_) {
    return;
  }
  UpdateDockedLayout();
}

void VisualGuidedSetterControllerWin::OnWidgetThemeChanged(
    views::Widget* widget) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(widget, parent_widget_);
  UpdateOverlayColor();
}

void VisualGuidedSetterControllerWin::OnVisibilityChanged(
    content::Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_running_) {
    return;
  }
  if (visibility != content::Visibility::VISIBLE) {
    OnWebContentsHidden();
    return;
  }
  if (parent_widget_ && !parent_widget_->IsVisible()) {
    return;
  }
  if (IsSettingsWindowValid()) {
    ResumeLayoutObservation();
  }
  UpdateDockedLayout();
}

void VisualGuidedSetterControllerWin::PrimaryPageChanged(content::Page& page) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_running_) {
    return;
  }
  if (!web_contents() ||
      !IsDefaultBrowserWebUiUrl(web_contents()->GetLastCommittedURL())) {
    outcome_ = Outcome::kSuccess;
    CloseSettingsWindow();
    Stop();
    return;
  }
  UpdateDockedLayout();
}

void VisualGuidedSetterControllerWin::LaunchSettings() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Querying base::FILE_EXE and launching the Settings shell URI may block, so
  // run it off the UI thread.
  base::ThreadPool::CreateCOMSTATaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
      ->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce([]() {
            base::FilePath chrome_exe;
            return base::PathService::Get(base::FILE_EXE, &chrome_exe) &&
                   ShellUtil::ShowMakeChromeDefaultSystemUI(chrome_exe);
          }),
          base::BindOnce(
              &VisualGuidedSetterControllerWin::OnLaunchSettingsResult,
              weak_ptr_factory_.GetWeakPtr()));
}

void VisualGuidedSetterControllerWin::OnLaunchSettingsResult(bool succeeded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_running_ || succeeded) {
    return;
  }

  outcome_ = Outcome::kSettingsLaunchFailed;
  NotifyErrorState(true);
  TearDownInternal();
}

void VisualGuidedSetterControllerWin::StartFindSettingsWindow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  find_settings_start_time_ = base::TimeTicks::Now();
  if (!settings_window_finder_) {
    settings_window_finder_ = CreateSettingsWindowFinder();
  }
  settings_window_finder_->Start(
      default_browser::kFindSettingsTimeout.Get(),
      base::BindOnce(&VisualGuidedSetterControllerWin::OnSettingsWindowFound,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VisualGuidedSetterControllerWin::OnFindSettingsTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VisualGuidedSetterControllerWin::OnSettingsWindowFound(HWND hwnd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(hwnd);

  if (!find_settings_start_time_.is_null()) {
    base::UmaHistogramTimes("DefaultBrowser.VisualGuide.EventHookDiscoveryTime",
                            base::TimeTicks::Now() - find_settings_start_time_);
    find_settings_start_time_ = base::TimeTicks();
  }

  settings_hwnd_ = hwnd;
  ::GetWindowThreadProcessId(hwnd, &settings_pid_);

  if ((web_contents() &&
       web_contents()->GetVisibility() != content::Visibility::VISIBLE) ||
      (parent_widget_ && !parent_widget_->IsVisible())) {
    OnWebContentsHidden();
    return;
  }

  ResumeLayoutObservation();
  UpdateDockedLayout();
}

void VisualGuidedSetterControllerWin::OnFindSettingsTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  find_settings_start_time_ = base::TimeTicks();
  outcome_ = Outcome::kSettingsWindowNotFound;
  NotifyErrorState(true);
  TearDownInternal();
}

void VisualGuidedSetterControllerWin::StartRuntimeTimers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  constexpr base::TimeDelta kDockTickInterval = base::Milliseconds(100);
  if (!dock_timer_.IsRunning()) {
    dock_timer_.Start(FROM_HERE, kDockTickInterval,
                      base::BindRepeating(
                          &VisualGuidedSetterControllerWin::UpdateDockedLayout,
                          // base::Unretained is safe because dock_timer_ is
                          // owned by `this` and destroyed before `this`.
                          base::Unretained(this)));
  }
}

void VisualGuidedSetterControllerWin::StopAllTimers() {
  if (settings_window_finder_) {
    settings_window_finder_->Stop();
    settings_window_finder_->StopObservingLocationChanges();
  }
  dock_timer_.Stop();
}

void VisualGuidedSetterControllerWin::ResumeLayoutObservation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_continuous_docking_enabled_) {
    StartRuntimeTimers();
  } else if (settings_window_finder_) {
    settings_window_finder_->StartObservingLocationChanges(
        settings_hwnd_,
        base::BindRepeating(
            &VisualGuidedSetterControllerWin::UpdateDockedLayout,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void VisualGuidedSetterControllerWin::PauseLayoutObservation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (settings_window_finder_) {
    settings_window_finder_->StopObservingLocationChanges();
  }
  dock_timer_.Stop();
}

void VisualGuidedSetterControllerWin::OnWebContentsHidden() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (overlay_) {
    overlay_->Hide();
  }
  if (IsSettingsWindowAlive()) {
    // Drop HWND_TOPMOST so the Settings window behaves like a normal floating
    // window and doesn't remain pinned on top of Chrome while viewing other
    // tabs. We avoid calling ::ShowWindow(SW_HIDE) because hiding external
    // UWP apps (SystemSettings.exe) suspends their UI thread and breaks input
    // control.
    HWND insert_after = (chrome_hwnd_ && ::IsWindow(chrome_hwnd_))
                            ? chrome_hwnd_
                            : HWND_NOTOPMOST;
    ::SetWindowPos(
        settings_hwnd_, insert_after, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
  }
  PauseLayoutObservation();
}

void VisualGuidedSetterControllerWin::UpdateDockedLayout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_running_) {
    return;
  }
  if (!web_contents() ||
      web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
    return;
  }
  if (IsSettingsWindowClosed()) {
    outcome_ = Outcome::kSettingsWindowClosed;
    TearDownInternal();
    return;
  }
  if (!IsSettingsWindowValid()) {
    return;
  }

  std::optional<gfx::Rect> anchor_rect_screen = GetAnchorRectScreen();
  if (!anchor_rect_screen.has_value()) {
    // If the anchor is unavailable (e.g. Chrome window was closed), enter
    // degraded floating mode.
    EnterDegradedFloating(Outcome::kStageTooSmall);
    return;
  }

  if (!visual_guided_setter::IsAnchorLargeEnoughForDocking(
          *anchor_rect_screen)) {
    EnterDegradedFloating(Outcome::kStageTooSmall);
    return;
  }

  const gfx::Rect work_area =
      display::win::GetScreenWin()
          ->GetScreenWinDisplayWithDisplayId(
              display::Screen::Get()
                  ->GetDisplayNearestWindow(parent_widget_->GetNativeWindow())
                  .id())
          .screen_work_rect();
  gfx::Rect anchor_rect_screen_dip = anchor_rect_in_webui_;
  if (web_contents()) {
    anchor_rect_screen_dip.Offset(
        web_contents()->GetViewBounds().OffsetFromOrigin());
  } else if (parent_widget_ && parent_widget_->GetContentsView()) {
    anchor_rect_screen_dip.Offset(parent_widget_->GetContentsView()
                                      ->GetBoundsInScreen()
                                      .OffsetFromOrigin());
  }
  const gfx::Rect settings_target =
      visual_guided_setter::ComputeDockedSettingsRectFromAnchor(
          chrome_hwnd_, anchor_rect_screen_dip, work_area, settings_hwnd_);

  bool dpi_compatible =
      IsDpiCompatibleForDocking(chrome_hwnd_, settings_target);
  if (!dpi_compatible) {
    EnterDegradedFloating(Outcome::kDpiMismatch);
    return;
  }

  if (is_degraded_) {
    outcome_ = std::nullopt;
    is_degraded_ = false;
  }
  NotifyErrorState(false);

  HWND insert_after = HWND_TOPMOST;
  if (topmost_policy_ == TopmostPolicy::kRequiresFocus &&
      !IsChromeWindowActive()) {
    insert_after = HWND_NOTOPMOST;
  }

  ApplySettingsRectAndZOrder(settings_target, insert_after);
  UpdateOverlay();
}

bool VisualGuidedSetterControllerWin::IsSettingsWindowAlive() const {
  return settings_hwnd_ && ::IsWindow(settings_hwnd_);
}

bool VisualGuidedSetterControllerWin::IsSettingsWindowValid() const {
  return IsSettingsWindowAlive() && ::IsWindowVisible(settings_hwnd_);
}

bool VisualGuidedSetterControllerWin::IsSettingsWindowClosed() const {
  return settings_hwnd_ && !::IsWindow(settings_hwnd_);
}

std::optional<gfx::Rect> VisualGuidedSetterControllerWin::GetAnchorRectScreen()
    const {
  if (!has_anchor_rect_ || !chrome_hwnd_ || !::IsWindow(chrome_hwnd_) ||
      !parent_widget_ || !parent_widget_->GetContentsView()) {
    return std::nullopt;
  }

  gfx::Rect anchor_rect_dip = GetAnchorRectScreenDip();

  // Scale to physical screen pixels using the monitor's DPI scaling factor so
  // that the overlay and docked window align correctly regardless of display
  // scaling.
  gfx::Rect anchor_rect = display::win::GetScreenWin()->DIPToScreenRect(
      chrome_hwnd_, anchor_rect_dip);

  if (anchor_rect.IsEmpty()) {
    return std::nullopt;
  }

  return anchor_rect;
}

void VisualGuidedSetterControllerWin::EnterDegradedFloating(Outcome reason) {
  outcome_ = reason;
  if (overlay_) {
    overlay_->Hide();
  }
  if (IsSettingsWindowValid()) {
    ::SetWindowPos(
        settings_hwnd_, HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
  }
  is_degraded_ = true;
  NotifyErrorState(true);
}

void VisualGuidedSetterControllerWin::ApplySettingsRectAndZOrder(
    const gfx::Rect& target_rect,
    HWND insert_after) {
  if (!IsSettingsWindowValid()) {
    return;
  }
  WINDOWPLACEMENT placement;
  placement.length = sizeof(placement);
  if (::GetWindowPlacement(settings_hwnd_, &placement) &&
      placement.showCmd != SW_SHOWNORMAL) {
    ::ShowWindow(settings_hwnd_, SW_RESTORE);
  }

  UINT flags = SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_ASYNCWINDOWPOS;
  RECT current_rect;
  if (::GetWindowRect(settings_hwnd_, &current_rect)) {
    gfx::Rect current(current_rect);
    if (current == target_rect) {
      flags |= SWP_NOMOVE | SWP_NOSIZE;
    }
  }

  bool is_currently_topmost =
      (::GetWindowLong(settings_hwnd_, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
  bool want_topmost = (insert_after == HWND_TOPMOST);
  if (is_currently_topmost == want_topmost) {
    flags |= SWP_NOZORDER;
  }

  ::SetWindowPos(settings_hwnd_, insert_after, target_rect.x(), target_rect.y(),
                 target_rect.width(), target_rect.height(), flags);
}

void VisualGuidedSetterControllerWin::UpdateOverlay() {
  if (!overlay_) {
    return;
  }
  if (!is_running_ || is_degraded_ || !IsSettingsWindowValid() ||
      !chrome_hwnd_ || ::IsIconic(chrome_hwnd_) ||
      (parent_widget_ && parent_widget_->IsMinimized())) {
    overlay_->Hide();
    return;
  }

  if (!web_contents() ||
      web_contents()->GetVisibility() != content::Visibility::VISIBLE ||
      !IsDefaultBrowserWebUiUrl(web_contents()->GetLastCommittedURL())) {
    overlay_->Hide();
    return;
  }

  std::optional<gfx::Rect> anchor_rect_screen = GetAnchorRectScreen();
  if (!anchor_rect_screen.has_value()) {
    overlay_->Hide();
    return;
  }

  RECT settings_rect_win;
  if (!::GetWindowRect(settings_hwnd_, &settings_rect_win)) {
    overlay_->Hide();
    return;
  }
  gfx::Rect settings_rect(settings_rect_win);
  if (settings_rect.IsEmpty()) {
    overlay_->Hide();
    return;
  }

  const gfx::Point start =
      visual_guided_setter::ComputeArrowStartPointFromAnchor(
          *anchor_rect_screen);
  const gfx::Point end =
      visual_guided_setter::ComputeArrowEndPoint(settings_rect);

  overlay_->UpdateAndShow(start, end);
}

void VisualGuidedSetterControllerWin::UpdateOverlayColor() {
  if (!overlay_ || !parent_widget_) {
    return;
  }

  const ui::ColorProvider* provider = parent_widget_->GetColorProvider();
  if (!provider) {
    return;
  }

  overlay_->SetArrowColor(provider->GetColor(ui::kColorSysPrimary));
}

gfx::Rect VisualGuidedSetterControllerWin::GetAnchorRectScreenDip() const {
  CHECK(web_contents());
  gfx::Rect anchor_rect_dip = anchor_rect_in_webui_;
  anchor_rect_dip.Offset(web_contents()->GetViewBounds().OffsetFromOrigin());
  return anchor_rect_dip;
}

bool VisualGuidedSetterControllerWin::IsChromeWindowActive() const {
  if (chrome_hwnd_ && ::IsWindow(chrome_hwnd_)) {
    return ::GetForegroundWindow() == chrome_hwnd_;
  }
  return last_known_chrome_active_;
}

void VisualGuidedSetterControllerWin::CloseSettingsWindow() {
  if (IsSettingsWindowAlive() && IsValidSettingsProcess(settings_hwnd_)) {
    ::SetWindowPos(
        settings_hwnd_, HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
    ::PostMessage(settings_hwnd_, WM_CLOSE, 0, 0);
  }
}

bool VisualGuidedSetterControllerWin::IsValidSettingsProcess(HWND hwnd) const {
  if (!hwnd) {
    return false;
  }
  DWORD pid = 0;
  ::GetWindowThreadProcessId(hwnd, &pid);
  return pid != 0 && (settings_pid_ == 0 || pid == settings_pid_);
}

std::unique_ptr<SettingsWindowFinderWin>
VisualGuidedSetterControllerWin::CreateSettingsWindowFinder() {
  return std::make_unique<SettingsWindowFinderWin>();
}

bool VisualGuidedSetterControllerWin::IsDpiCompatibleForDocking(
    HWND hwnd,
    const gfx::Rect& target_rect) const {
  return visual_guided_setter::IsDpiCompatibleForDocking(hwnd, target_rect);
}

void VisualGuidedSetterControllerWin::TearDownInternal() {
  StopAllTimers();
  // Invalidate any outstanding weak replies.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (overlay_) {
    overlay_->Hide();
    overlay_.reset();
  }

  if (IsSettingsWindowAlive()) {
    ::SetWindowPos(
        settings_hwnd_, HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
  }

  settings_hwnd_ = nullptr;
  settings_pid_ = 0;

  if (is_running_) {
    base::UmaHistogramEnumeration(
        "DefaultBrowser.VisualGuide.Outcome",
        outcome_.value_or(Outcome::kSettingsWindowClosed));
  }

  if (outcome_.has_value() && outcome_.value() != Outcome::kSuccess) {
    NotifyErrorState(true);
  }

  is_running_ = false;
  is_degraded_ = false;
}
