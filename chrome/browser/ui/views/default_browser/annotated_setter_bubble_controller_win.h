// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DEFAULT_BROWSER_ANNOTATED_SETTER_BUBBLE_CONTROLLER_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_DEFAULT_BROWSER_ANNOTATED_SETTER_BUBBLE_CONTROLLER_WIN_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/windows_types.h"
#include "chrome/browser/default_browser/win/settings_row_locator_win.h"
#include "chrome/browser/default_browser/win/settings_window_finder_win.h"
#include "chrome/browser/ui/views/default_browser/annotated_setter_bubble_controller.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace views {
class Widget;
}  // namespace views

namespace default_browser {

class AnnotatedSetterBubble;

// Points a bubble at the "Set default" button of the Windows Settings window,
// as located by UI Automation. The Settings window is never moved or resized.
//
// Single-run RAII object: construction starts looking for the Settings window,
// and destruction stops the guidance and closes the bubble without running
// `on_complete`.
class AnnotatedSetterBubbleControllerWin final
    : public AnnotatedSetterBubbleController {
 public:
  // Queries and observes the Win32 state of the Settings window.
  class WindowInspector {
   public:
    virtual ~WindowInspector() = default;

    // In physical pixels; nullopt if unavailable or empty.
    virtual std::optional<gfx::Rect> GetWindowRect(HWND hwnd) const = 0;
    // Returns true if `hwnd` was destroyed, now belongs to another process,
    // or is hidden or cloaked without being minimized.
    virtual bool IsWindowGone(HWND hwnd,
                              const base::Process& process) const = 0;
    virtual bool IsWindowMinimized(HWND hwnd) const = 0;
    virtual bool IsWindowActive(HWND hwnd) const = 0;
    virtual void StartObservingForegroundChanges(
        base::RepeatingClosure on_foreground_changed) = 0;
    virtual void StopObservingForegroundChanges() = 0;
  };

  // Lets the Settings page reflow after a resize before it is measured.
  static constexpr base::TimeDelta kMeasureDebounce = base::Milliseconds(300);
  // Before measuring again after a failed measurement, while budget remains.
  // Longer than the debounce so a cold-launched Settings page has time to
  // load.
  static constexpr base::TimeDelta kMeasureRetryDelay = base::Seconds(1);
  static constexpr int kMaxConsecutiveMeasurementFailures = 3;

  static std::unique_ptr<AnnotatedSetterBubbleController> Create(
      base::TimeDelta find_timeout,
      bool can_pin_to_taskbar,
      CompletionCallback on_complete);

  // `on_complete` runs when the guidance ends on its own, but not on
  // destruction.
  AnnotatedSetterBubbleControllerWin(base::TimeDelta find_timeout,
                                     bool can_pin_to_taskbar,
                                     CompletionCallback on_complete);
  AnnotatedSetterBubbleControllerWin(
      base::TimeDelta find_timeout,
      bool can_pin_to_taskbar,
      CompletionCallback on_complete,
      std::unique_ptr<SettingsWindowFinderWin> settings_window_finder,
      std::unique_ptr<SettingsRowLocatorWin> row_locator,
      std::unique_ptr<WindowInspector> window_inspector);

  AnnotatedSetterBubbleControllerWin(
      const AnnotatedSetterBubbleControllerWin&) = delete;
  AnnotatedSetterBubbleControllerWin& operator=(
      const AnnotatedSetterBubbleControllerWin&) = delete;

  ~AnnotatedSetterBubbleControllerWin() override;

  views::Widget* bubble_widget_for_testing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return bubble_widget_.get();
  }
  AnnotatedSetterBubble* bubble_for_testing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return bubble_.get();
  }

 private:
  void OnSettingsWindowFound(HWND hwnd, base::Process process)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void OnFindSettingsTimeout() VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Updates the bubble and schedules a measurement if the window's size
  // changed.
  void OnSettingsWindowChanged() VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Opens the service's handle to the Settings process on the thread pool,
  // then asks the locator for the button.
  void MeasureButtonNow() VALID_CONTEXT_REQUIRED(sequence_checker_);
  // `measured_size` is the window's size when the request was made.
  void OnSettingsProcessOpened(gfx::Size measured_size,
                               base::Process settings_process)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void OnButtonMeasured(gfx::Size measured_size,
                        SettingsRowLocatorWin::ButtonResult result)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  // `button_screen_rect` is in physical pixels.
  void ShowBubble(const gfx::Rect& button_screen_rect)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void HideBubble() VALID_CONTEXT_REQUIRED(sequence_checker_);
  bool IsSettingsOrBubbleActive() const
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void OnBubbleClosed() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void TearDown() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void Finish(Outcome outcome) VALID_CONTEXT_REQUIRED(sequence_checker_);

  const bool can_pin_to_taskbar_;
  CompletionCallback on_complete_ GUARDED_BY_CONTEXT(sequence_checker_);

  const std::unique_ptr<WindowInspector> window_inspector_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Kept until destruction, since Finish() can run inside their callbacks.
  const std::unique_ptr<SettingsWindowFinderWin> settings_window_finder_
      GUARDED_BY_CONTEXT(sequence_checker_);
  const std::unique_ptr<SettingsRowLocatorWin> row_locator_
      GUARDED_BY_CONTEXT(sequence_checker_);

  HWND settings_hwnd_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
  // Holding the owner's handle keeps its pid from being reused.
  base::Process settings_process_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::OneShotTimer measure_timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  int consecutive_measurement_failures_ GUARDED_BY_CONTEXT(sequence_checker_) =
      0;
  std::optional<gfx::Size> measured_window_size_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Relative to the window's origin, in physical pixels.
  std::optional<gfx::Rect> button_bounds_in_window_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The widget must be destroyed before its delegate.
  std::unique_ptr<AnnotatedSetterBubble> bubble_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<views::Widget> bubble_widget_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AnnotatedSetterBubbleControllerWin> weak_ptr_factory_{
      this};
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_UI_VIEWS_DEFAULT_BROWSER_ANNOTATED_SETTER_BUBBLE_CONTROLLER_WIN_H_
