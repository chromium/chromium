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
#include "chrome/browser/default_browser/win/settings_window_finder_win.h"
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
    // Launching the Windows Settings default-apps UI failed, e.g. because the
    // install mode does not support set-as-default (canary) or registration
    // failed.
    kSettingsLaunchFailed = 5,
    // The user navigated the guide tab away from the guide page before
    // completing the flow.
    kNavigatedAway = 6,
    // The user moved or resized the Settings window, taking over its
    // placement. The flow stops docking and leaves the window floating.
    kUserRepositioned = 7,
    // The width the Settings window would dock at puts the "Default apps" page
    // in a layout whose rows we cannot place: the "Set default" button wraps
    // below the name Chrome is registered under, or the width sits on the
    // breakpoint where the navigation pane collapses. We degrade to floating
    // rather than point the guidance arrow at the wrong row.
    kSettingsLayoutUnstable = 8,
    // The user moved or resized the Chrome window. Without continuous
    // docking the Settings window cannot follow it, so the placement the
    // guide arranged is stale.
    kChromeWindowRepositioned = 9,
    // Without continuous docking: the Chrome window was activated after the
    // Settings window was placed, bringing it in front of the arrangement the
    // guide set up.
    kChromeWindowActivated = 10,
    // Without continuous docking: the Chrome window was minimized or hidden,
    // or the guide tab was hidden, after the Settings window was placed.
    kChromeWindowHidden = 11,
    // Without continuous docking: the user minimized, maximized, snapped, or
    // otherwise changed the Settings window's placement after the guide
    // placed it, other than by dragging it (see kUserRepositioned).
    kSettingsWindowStateChanged = 12,
    kMaxValue = kSettingsWindowStateChanged,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/ui/enums.xml:DefaultBrowserVisualGuideOutcome)

  using ErrorCallback = base::RepeatingCallback<void(bool)>;
  // Receives the docked Settings window's client bounds in WebUI CSS pixels. It
  // only ever run with the bounds of a window that really docked.
  using DockedBoundsCallback = base::RepeatingCallback<void(const gfx::Rect&)>;

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
  // `callback` runs whenever the bounds the docked Settings window really
  // occupies change: the window is asked for the stage's geometry, but the
  // Settings app enforces a minimum size, so what it gets can be taller. Once
  // it has run, the page keeps that size for the rest of the flow.
  void SetDockedBoundsCallback(DockedBoundsCallback callback);

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void PrimaryPageChanged(content::Page& page) override;

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroyed(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetShowStateChanged(views::Widget* widget) override;
  void OnWidgetThemeChanged(views::Widget* widget) override;

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
  virtual void CloseSettingsWindow();
  virtual bool IsValidSettingsProcess(HWND hwnd) const;

  // Low-level Win32 window probes backing the predicates above. Virtual for
  // testing.
  virtual bool IsWindowAlive(HWND hwnd) const;
  virtual bool IsWindowOnScreen(HWND hwnd) const;
  virtual bool IsWindowCloaked(HWND hwnd) const;
  virtual bool IsWindowMinimized(HWND hwnd) const;
  virtual bool IsWindowMaximized(HWND hwnd) const;
  // Screen bounds of the latched Settings window, or nullopt when they are
  // unavailable or empty. Virtual for testing.
  virtual std::optional<gfx::Rect> GetSettingsWindowScreenRect() const;
  // Screen bounds (physical pixels) of the latched Settings window's client
  // area, or nullopt when they are unavailable or empty. Virtual for testing.
  virtual std::optional<gfx::Rect> GetSettingsWindowClientScreenRect() const;
  // Overlay forwarding. Virtual for testing, so tests can observe when the
  // guidance arrow is shown or hidden.
  virtual void ShowOverlayArrow(const gfx::Point& start, const gfx::Point& end);
  virtual void HideOverlayArrow();

  // Called on the UI sequence with the result of the Settings launch posted
  // by LaunchSettings(). A failed launch tears the flow down immediately with
  // Outcome::kSettingsLaunchFailed instead of waiting for the finder timeout.
  void OnLaunchSettingsResult(bool succeeded);

  // Returns the rect to dock the Settings window to, in physical screen
  // pixels. Virtual for testing.
  virtual gfx::Rect ComputeDockedSettingsRect() const;

 private:
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

  // Reports the docked window's current client bounds to
  // docked_bounds_callback_, once the window sits where the last layout put
  // it. Until then its bounds are wherever Settings opened, which the page
  // must not fit its slot to.
  void ReportDockedSettingsBounds();
  // Runs docked_bounds_callback_ if `bounds` differ from the last report.
  void NotifyDockedSettingsBounds(const gfx::Rect& bounds);

  // Halts the controller, kills timers, and releases OS resources.
  void TearDownInternal();

  // Returns the z-order target for the Settings window, consuming a pending
  // topmost re-assert. Call once per applied layout.
  HWND GetSettingsWindowInsertAfter() const;

  // Called when the user starts or finishes dragging or resizing the Settings
  // window.
  void OnSettingsWindowMoveSize(bool in_progress);

  // Whether a user disturbance of the arrangement should degrade the flow:
  // only without continuous docking (with it, the Settings window follows
  // Chrome and the arrangement survives), and only while a running, not yet
  // degraded flow has placed the latched Settings window at least once.
  bool ShouldDegradeOnUserDisturbance() const;

  // Degrades with `reason` if ShouldDegradeOnUserDisturbance(). A Settings
  // window that has already been closed instead ends the flow as
  // kSettingsWindowClosed: closing it hands activation back to Chrome, which
  // must not be mistaken for the user bringing Chrome to the front.
  void MaybeDegradeOnUserDisturbance(Outcome reason);

  // Degrades when the Chrome window is minimized or hidden, or moved or
  // resized away from where the guide last arranged the Settings window
  // against it.
  void MaybeDegradeOnChromeWindowBoundsChanged(const gfx::Rect& new_bounds);

  // Whether the latched Settings window has been minimized, maximized, or
  // moved since it settled where the guide placed it. Only meaningful without
  // continuous docking.
  bool HasUserChangedSettingsWindow() const;

  // Records the Settings window's bounds once the guide's last placement has
  // landed, as the baseline HasUserChangedSettingsWindow() compares against.
  void MaybeRecordSettledSettingsRect();

  ErrorCallback error_callback_;
  std::optional<bool> last_reported_error_;

  DockedBoundsCallback docked_bounds_callback_;
  // Empty until a docked window has been reported, and non-empty from then
  // until Start() begins a new flow.
  gfx::Rect last_reported_docked_bounds_;
  // The rect the last layout asked the Settings window to take, in physical
  // screen pixels. Its origin is honored even when the window's minimum size
  // clamps the requested size, so it tells the two apart.
  std::optional<gfx::Rect> last_applied_settings_rect_;

  raw_ptr<views::Widget> parent_widget_ = nullptr;
  HWND chrome_hwnd_ = nullptr;
  HWND settings_hwnd_ = nullptr;
  DWORD settings_pid_ = 0;

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
  // The Chrome window's screen bounds as of the last layout the guide
  // applied, i.e. what the Settings window was arranged against. Compared
  // against to tell a real move from the redundant bounds changes Windows
  // emits.
  gfx::Rect last_known_chrome_bounds_;
  // The Settings window's actual screen bounds once the last applied layout
  // landed (its origin matches last_applied_settings_rect_). The actual
  // rather than the requested bounds, since the app's minimum size can clamp
  // the size. Reset whenever the guide applies a different rect.
  std::optional<gfx::Rect> settled_settings_rect_;

  gfx::Rect anchor_rect_in_webui_;
  bool has_anchor_rect_ = false;
  bool is_continuous_docking_enabled_ = false;

  std::optional<Outcome> outcome_ = std::nullopt;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<VisualGuidedSetterControllerWin> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_CONTROLLER_WIN_H_
