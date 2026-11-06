// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/default_browser/annotated_setter_bubble_controller_win.h"

#include <windows.h>

#include <cstdint>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/views/default_browser/annotated_setter_bubble.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_util.h"

namespace default_browser {

namespace {

class Win32WindowInspector
    : public AnnotatedSetterBubbleControllerWin::WindowInspector {
 public:
  Win32WindowInspector() { DETACH_FROM_SEQUENCE(sequence_checker_); }

  Win32WindowInspector(const Win32WindowInspector&) = delete;
  Win32WindowInspector& operator=(const Win32WindowInspector&) = delete;

  ~Win32WindowInspector() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    StopObservingForegroundChanges();
  }

  std::optional<gfx::Rect> GetWindowRect(HWND hwnd) const override {
    RECT rect;
    if (!::GetWindowRect(hwnd, &rect)) {
      return std::nullopt;
    }

    const gfx::Rect bounds(rect);
    if (bounds.IsEmpty()) {
      return std::nullopt;
    }
    return bounds;
  }

  bool IsWindowGone(HWND hwnd, const base::Process& process) const override {
    if (!::IsWindow(hwnd)) {
      return true;
    }

    // The HWND may have been reused by another process's window.
    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0 || pid != process.Pid()) {
      return true;
    }

    // The shell cloaks minimized UWP windows.
    if (::IsIconic(hwnd)) {
      return false;
    }

    return !::IsWindowVisible(hwnd) || gfx::IsWindowCloaked(hwnd);
  }

  bool IsWindowMinimized(HWND hwnd) const override { return ::IsIconic(hwnd); }

  bool IsWindowActive(HWND hwnd) const override {
    if (!hwnd) {
      return false;
    }
    HWND foreground = ::GetForegroundWindow();
    return foreground && (foreground == hwnd ||
                          ::GetAncestor(foreground, GA_ROOTOWNER) == hwnd);
  }

  void StartObservingForegroundChanges(
      base::RepeatingClosure on_foreground_changed) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    StopObservingForegroundChanges();
    on_foreground_changed_ = std::move(on_foreground_changed);
    // System-wide and including the current process, so switching focus to a
    // Chrome window or another app is observed as well as returning to
    // Settings.
    foreground_hook_ = ::SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
        &Win32WindowInspector::OnWinEvent, /*idProcess=*/0, /*idThread=*/0,
        WINEVENT_OUTOFCONTEXT);
    if (foreground_hook_) {
      GetActiveHooks().emplace(reinterpret_cast<uintptr_t>(foreground_hook_),
                               this);
    }
  }

  void StopObservingForegroundChanges() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (foreground_hook_) {
      GetActiveHooks().erase(reinterpret_cast<uintptr_t>(foreground_hook_));
      ::UnhookWinEvent(foreground_hook_);
      foreground_hook_ = nullptr;
    }
    on_foreground_changed_.Reset();
  }

 private:
  using HookMap = base::flat_map<uintptr_t, raw_ptr<Win32WindowInspector>>;

  static HookMap& GetActiveHooks() {
    static base::NoDestructor<HookMap> hooks;
    return *hooks;
  }

  static void CALLBACK OnWinEvent(HWINEVENTHOOK hook,
                                  DWORD event,
                                  HWND hwnd,
                                  LONG id_object,
                                  LONG id_child,
                                  DWORD event_thread,
                                  DWORD event_time) {
    if (!hwnd || id_object != OBJID_WINDOW ||
        event != EVENT_SYSTEM_FOREGROUND) {
      return;
    }
    auto it = GetActiveHooks().find(reinterpret_cast<uintptr_t>(hook));
    if (it == GetActiveHooks().end()) {
      return;
    }
    Win32WindowInspector* inspector = it->second;
    DCHECK_CALLED_ON_VALID_SEQUENCE(inspector->sequence_checker_);
    if (inspector->on_foreground_changed_) {
      // Run a copy in case the callback stops observation reentrantly.
      base::RepeatingClosure callback = inspector->on_foreground_changed_;
      callback.Run();
    }
  }

  // RAW_PTR_EXCLUSION: HWINEVENTHOOK is an opaque Win32 handle (not a real
  // pointer, and never allocated by PartitionAlloc).
  RAW_PTR_EXCLUSION HWINEVENTHOOK foreground_hook_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
  base::RepeatingClosure on_foreground_changed_
      GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

// static
std::unique_ptr<AnnotatedSetterBubbleController>
AnnotatedSetterBubbleControllerWin::Create(base::TimeDelta find_timeout,
                                           bool can_pin_to_taskbar,
                                           CompletionCallback on_complete) {
  return std::make_unique<AnnotatedSetterBubbleControllerWin>(
      find_timeout, can_pin_to_taskbar, std::move(on_complete));
}

AnnotatedSetterBubbleControllerWin::AnnotatedSetterBubbleControllerWin(
    base::TimeDelta find_timeout,
    bool can_pin_to_taskbar,
    CompletionCallback on_complete)
    : AnnotatedSetterBubbleControllerWin(
          find_timeout,
          can_pin_to_taskbar,
          std::move(on_complete),
          std::make_unique<SettingsWindowFinderWin>(),
          std::make_unique<SettingsRowLocatorWin>(),
          std::make_unique<Win32WindowInspector>()) {}

AnnotatedSetterBubbleControllerWin::AnnotatedSetterBubbleControllerWin(
    base::TimeDelta find_timeout,
    bool can_pin_to_taskbar,
    CompletionCallback on_complete,
    std::unique_ptr<SettingsWindowFinderWin> settings_window_finder,
    std::unique_ptr<SettingsRowLocatorWin> row_locator,
    std::unique_ptr<WindowInspector> window_inspector)
    : can_pin_to_taskbar_(can_pin_to_taskbar),
      on_complete_(std::move(on_complete)),
      window_inspector_(std::move(window_inspector)),
      settings_window_finder_(std::move(settings_window_finder)),
      row_locator_(std::move(row_locator)) {
  CHECK(on_complete_);
  CHECK(window_inspector_);
  CHECK(settings_window_finder_);
  CHECK(row_locator_);

  settings_window_finder_->Start(
      find_timeout,
      base::BindOnce(&AnnotatedSetterBubbleControllerWin::OnSettingsWindowFound,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&AnnotatedSetterBubbleControllerWin::OnFindSettingsTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
}

AnnotatedSetterBubbleControllerWin::~AnnotatedSetterBubbleControllerWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TearDown();
}

void AnnotatedSetterBubbleControllerWin::OnSettingsWindowFound(
    HWND hwnd,
    base::Process process) {
  CHECK(hwnd);
  settings_hwnd_ = hwnd;
  settings_process_ = std::move(process);
  settings_window_finder_->StartObservingLocationChanges(
      settings_hwnd_,
      base::BindRepeating(
          &AnnotatedSetterBubbleControllerWin::OnSettingsWindowChanged,
          weak_ptr_factory_.GetWeakPtr()));
  window_inspector_->StartObservingForegroundChanges(base::BindRepeating(
      &AnnotatedSetterBubbleControllerWin::OnSettingsWindowChanged,
      weak_ptr_factory_.GetWeakPtr()));
  OnSettingsWindowChanged();
}

void AnnotatedSetterBubbleControllerWin::OnFindSettingsTimeout() {
  Finish(Outcome::kSettingsWindowNotFound);
}

void AnnotatedSetterBubbleControllerWin::OnSettingsWindowChanged() {
  if (window_inspector_->IsWindowGone(settings_hwnd_, settings_process_)) {
    Finish(Outcome::kSettingsWindowClosed);
    return;
  }

  // Restoring the window fires another change.
  if (window_inspector_->IsWindowMinimized(settings_hwnd_)) {
    HideBubble();
    return;
  }
  const std::optional<gfx::Rect> window_rect =
      window_inspector_->GetWindowRect(settings_hwnd_);
  if (!window_rect) {
    HideBubble();
    return;
  }

  // A resize reflows the page; measure once it settles.
  if (window_rect->size() != measured_window_size_) {
    measured_window_size_ = window_rect->size();
    button_bounds_in_window_.reset();
    measure_timer_.Start(FROM_HERE, kMeasureDebounce, this,
                         &AnnotatedSetterBubbleControllerWin::MeasureButtonNow);
  }

  if (!button_bounds_in_window_ || !IsSettingsOrBubbleActive()) {
    HideBubble();
    return;
  }

  ShowBubble(*button_bounds_in_window_ + window_rect->OffsetFromOrigin());
}

void AnnotatedSetterBubbleControllerWin::MeasureButtonNow() {
  // Never query a reused HWND: UIA on a Chrome window enables accessibility.
  if (window_inspector_->IsWindowGone(settings_hwnd_, settings_process_)) {
    Finish(Outcome::kSettingsWindowClosed);
    return;
  }

  // Restoring the window reschedules the measurement.
  if (window_inspector_->IsWindowMinimized(settings_hwnd_)) {
    measured_window_size_.reset();
    return;
  }

  // The service gets its own handle to the Settings process. Opening one is
  // not cheap (see base::Process::Duplicate()), so it happens on the thread
  // pool, by pid rather than from `settings_process_`, which the pool must not
  // touch: `settings_process_` pins the pid for as long as this flow holds it,
  // and TearDown(), which closes it, also drops this reply.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](base::ProcessId pid) {
            return pid == base::kNullProcessId
                       ? base::Process()
                       : base::Process::OpenWithAccess(
                             pid, PROCESS_QUERY_LIMITED_INFORMATION);
          },
          settings_process_.IsValid() ? settings_process_.Pid()
                                      : base::kNullProcessId),
      base::BindOnce(
          &AnnotatedSetterBubbleControllerWin::OnSettingsProcessOpened,
          weak_ptr_factory_.GetWeakPtr(), *measured_window_size_));
}

void AnnotatedSetterBubbleControllerWin::OnSettingsProcessOpened(
    gfx::Size measured_size,
    base::Process settings_process) {
  // The window may have changed while the handle was being opened; the next
  // measurement is already scheduled then.
  if (measured_window_size_ != measured_size) {
    return;
  }

  // An invalid handle (the process exited meanwhile) is answered kWindowGone.
  row_locator_->MeasureButton(
      settings_hwnd_, std::move(settings_process),
      base::BindOnce(&AnnotatedSetterBubbleControllerWin::OnButtonMeasured,
                     weak_ptr_factory_.GetWeakPtr(), measured_size));
}

void AnnotatedSetterBubbleControllerWin::OnButtonMeasured(
    gfx::Size measured_size,
    SettingsRowLocatorWin::ButtonResult result) {
  using FailureReason = SettingsRowLocatorWin::FailureReason;

  // Neither is a failure: a newer request is in flight, or the bounds are
  // for a size the window has left.
  const bool superseded =
      !result.has_value() && result.error() == FailureReason::kSuperseded;
  if (superseded || measured_window_size_ != measured_size) {
    return;
  }

  if (!result.has_value() && result.error() == FailureReason::kWindowGone) {
    Finish(Outcome::kSettingsWindowClosed);
    return;
  }

  if (!result.has_value()) {
    if (++consecutive_measurement_failures_ >=
        kMaxConsecutiveMeasurementFailures) {
      Finish(Outcome::kButtonNotFound);
      return;
    }
    // Retry, since nothing is shown until a measurement lands. A resize in the
    // meantime restarts the timer with the shorter debounce.
    measure_timer_.Start(FROM_HERE, kMeasureRetryDelay, this,
                         &AnnotatedSetterBubbleControllerWin::MeasureButtonNow);
    return;
  }

  consecutive_measurement_failures_ = 0;
  button_bounds_in_window_ = *result;
  OnSettingsWindowChanged();
}

void AnnotatedSetterBubbleControllerWin::ShowBubble(
    const gfx::Rect& button_screen_rect) {
  const gfx::Rect button_rect_dip =
      display::win::GetScreenWin()->ScreenToDIPRect(settings_hwnd_,
                                                    button_screen_rect);
  if (!bubble_widget_) {
    bubble_ = std::make_unique<AnnotatedSetterBubble>(can_pin_to_taskbar_);
    bubble_widget_ = bubble_->Show(
        button_rect_dip,
        base::IgnoreArgs<views::Widget::ClosedReason>(
            base::BindOnce(&AnnotatedSetterBubbleControllerWin::OnBubbleClosed,
                           weak_ptr_factory_.GetWeakPtr())));
    return;
  }

  bubble_->SetAnchorRect(button_rect_dip);

  if (!bubble_widget_->IsVisible()) {
    bubble_widget_->ShowInactive();
  }
}

void AnnotatedSetterBubbleControllerWin::HideBubble() {
  if (bubble_widget_) {
    bubble_widget_->Hide();
  }
}

bool AnnotatedSetterBubbleControllerWin::IsSettingsOrBubbleActive() const {
  if (window_inspector_->IsWindowActive(settings_hwnd_)) {
    return true;
  }
  // Clicking the bubble (e.g. its close button) activates the bubble widget
  // itself; keep it visible so the click completes instead of hiding out from
  // under the cursor.
  return bubble_widget_ && bubble_widget_->IsVisible() &&
         window_inspector_->IsWindowActive(
             views::HWNDForWidget(bubble_widget_.get()));
}

void AnnotatedSetterBubbleControllerWin::OnBubbleClosed() {
  Finish(Outcome::kDismissed);
}

void AnnotatedSetterBubbleControllerWin::TearDown() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  measure_timer_.Stop();
  settings_window_finder_->Stop();
  settings_window_finder_->StopObservingLocationChanges();
  window_inspector_->StopObservingForegroundChanges();

  HideBubble();
  bubble_widget_.reset();
  bubble_.reset();

  settings_hwnd_ = nullptr;
  settings_process_.Close();
}

void AnnotatedSetterBubbleControllerWin::Finish(Outcome outcome) {
  CHECK(on_complete_);
  TearDown();
  std::move(on_complete_).Run(outcome);
}

}  // namespace default_browser
