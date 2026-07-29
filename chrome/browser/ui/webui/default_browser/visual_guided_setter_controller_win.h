// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_CONTROLLER_WIN_H_
#define CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_CONTROLLER_WIN_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "base/win/windows_types.h"
#include "chrome/browser/ui/webui/default_browser/settings_window_finder_win.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class WebContents;
}

class GuidedSetterOverlayWindowWin;

// Drives the "mimicked embedding" of the Windows Default Apps Settings window
// over the chrome://default-browser WebUI stage, and the
// transparent guidance overlay that points at the Settings "Set default"
// button.
//
// All public methods and timer callbacks run on the browser UI sequence.
class VisualGuidedSetterControllerWin : public views::WidgetObserver,
                                        public content::WebContentsObserver {
 public:
  // Specifies the behavior of the Settings window.
  enum class TopmostPolicy {
    // The Settings window drops its topmost state when Chrome loses focus.
    kRequiresFocus,
    // The Settings window always remains topmost, regardless of Chrome's focus.
    kAlways,
  };

  // Outcomes for the UMA histogram "DefaultBrowser.VisualGuide.Outcome".
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(DefaultBrowserVisualGuideOutcome)
  enum class Outcome {
    kSuccess = 0,
    kSettingsWindowNotFound = 1,
    // The Settings window and the Chrome window are on monitors with different
    // DPI scaling. We degrade to floating to avoid rendering/docking glitches.
    kDpiMismatch = 2,
    // The WebUI stage (the container area where the Settings window is docked)
    // is smaller than the minimum required size. We degrade to floating.
    kStageTooSmall = 3,
    // The user manually closed the Settings window before flow completion.
    kSettingsWindowClosed = 4,
    kMaxValue = kSettingsWindowClosed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/ui/enums.xml:DefaultBrowserVisualGuideOutcome)

  using ErrorCallback = base::RepeatingCallback<void(bool)>;

  explicit VisualGuidedSetterControllerWin(views::Widget* parent_widget);

  VisualGuidedSetterControllerWin(const VisualGuidedSetterControllerWin&) =
      delete;
  VisualGuidedSetterControllerWin& operator=(
      const VisualGuidedSetterControllerWin&) = delete;

  ~VisualGuidedSetterControllerWin() override;

  void Start();
  void Stop();

  void SetTopmostPolicy(TopmostPolicy policy);
  void SetAnchorRectInWebUi(const gfx::Rect& rect);
  void SetWebContents(content::WebContents* web_contents);
  void SetErrorCallback(ErrorCallback callback);

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void PrimaryPageChanged(content::Page& page) override;

  bool is_running() const { return is_running_; }
  bool has_anchor_rect() const { return has_anchor_rect_; }
  HWND settings_hwnd_for_testing() const { return settings_hwnd_; }

 protected:
  // Virtual for testing.
  virtual bool IsSettingsWindowAlive() const;
  virtual bool IsSettingsWindowValid() const;
  virtual bool IsSettingsWindowClosed() const;
  virtual std::optional<gfx::Rect> GetAnchorRectScreen() const;
  virtual void ApplySettingsRectAndZOrder(const gfx::Rect& target_rect,
                                          HWND insert_after);
  virtual bool IsChromeWindowActive() const;
  virtual void LaunchSettings();
  virtual std::unique_ptr<SettingsWindowFinderWin> CreateSettingsWindowFinder();
  virtual bool IsDpiCompatibleForDocking(HWND hwnd,
                                         const gfx::Rect& target_rect) const;

 private:
  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroyed(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetShowStateChanged(views::Widget* widget) override;
  void OnWidgetThemeChanged(views::Widget* widget) override;

  // Starts the polling and event-hooking logic to find the Settings window.
  void StartFindSettingsWindow();
  void OnSettingsWindowFound(HWND hwnd);
  void OnFindSettingsTimeout();

  // Asynchronously spawns the OS Default Apps settings URI.
  void StartRuntimeTimers();
  void StopAllTimers();
  void PauseLayoutObservation();
  void ResumeLayoutObservation();
  void OnWebContentsHidden();

  // Synchronizes the docked Settings window to the Chrome anchor.
  void UpdateDockedLayout();

  // Switches into kDegradedFloating: hides the overlay and clears the topmost
  // bit so the Settings window behaves like a normal floating window.
  void EnterDegradedFloating(Outcome reason);

  void UpdateOverlay();
  void UpdateOverlayColor();

  gfx::Rect GetAnchorRectScreenDip() const;

  // Notifies error_callback_ of error status changes.
  void NotifyErrorState(bool is_error);

  // Halts the controller, kills timers, and releases OS resources.
  void TearDownInternal();

  ErrorCallback error_callback_;
  std::optional<bool> last_reported_error_;

  raw_ptr<views::Widget> parent_widget_ = nullptr;
  HWND chrome_hwnd_ = nullptr;
  HWND settings_hwnd_ = nullptr;

  std::unique_ptr<GuidedSetterOverlayWindowWin> overlay_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  std::unique_ptr<SettingsWindowFinderWin> settings_window_finder_;
  base::TimeTicks find_settings_start_time_;
  base::RepeatingTimer dock_timer_;

  TopmostPolicy topmost_policy_ = TopmostPolicy::kRequiresFocus;
  bool is_running_ = false;
  bool is_degraded_ = false;
  bool last_known_chrome_active_ = true;

  gfx::Rect anchor_rect_in_webui_;
  bool has_anchor_rect_ = false;
  bool is_continuous_docking_enabled_ = false;

  std::optional<Outcome> outcome_ = std::nullopt;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<VisualGuidedSetterControllerWin> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_CONTROLLER_WIN_H_
