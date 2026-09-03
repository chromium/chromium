// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_WINDOW_METRICS_MANAGER_H_
#define CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_WINDOW_METRICS_MANAGER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/waap/waap_utils.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class WaapUIMetricsService;
class BrowserWindowInterface;

// Manages the recording of InitialWebUI latency metrics related to the
// creation and first paint of browser windows and its UI elements, e.g.
// ReloadButton.
//
// It distinguishes between "Startup" and "New Window" metrics:
// - "Startup" metrics are recorded only ONCE per browser process lifetime,
//   for the very first window that paints.
// - "New Window" metrics are recorded for *every* window (not including the
//    first one), if the window was created with a valid
//    `new_window_start_time`.
//
// This class is a feature of BrowserWindowFeature, meaning an instance is
// created per browser window and is scoped to the window's lifetime.
class InitialWebUIWindowMetricsManager {
 public:
  DECLARE_USER_DATA(InitialWebUIWindowMetricsManager);

  explicit InitialWebUIWindowMetricsManager(BrowserWindowInterface* browser);

  // Not copyable or movable.
  InitialWebUIWindowMetricsManager(const InitialWebUIWindowMetricsManager&) =
      delete;
  InitialWebUIWindowMetricsManager& operator=(
      const InitialWebUIWindowMetricsManager&) = delete;

  ~InitialWebUIWindowMetricsManager();

  static InitialWebUIWindowMetricsManager* From(
      BrowserWindowInterface* browser_window_interface);

  // Sets the window creation info. This should be called immediately after the
  // `Browser` object is created.
  void SetWindowCreationInfo(waap::NewWindowCreationSource source,
                             base::TimeTicks creation_time);

  // Notifies the manager that a request to show the browser window has been
  // made.
  void OnBrowserWindowShowRequested(base::TimeTicks time);

  // Called when the browser window is presented first time.
  // This handles both startup window and new window metrics.
  void OnBrowserWindowFirstPresentation(base::TimeTicks timestamp);

  // Called when the browser window is created.
  void OnBrowserWindowCreated();

  // Called when the ReloadButton is created.
  void OnReloadButtonCreated();

  // Called when the ReloadButton paints its first frame.
  // This handles reload button metrics in both startup window and new window.
  void OnReloadButtonFirstPaint(base::TimeTicks timestamp);

  // Called when the ReloadButton paints its first contentful paint.
  // This handles reload button metrics in both startup window and new window.
  void OnReloadButtonFirstContentfulPaint(base::TimeTicks timestamp);

  // Called when the renderer process is created and launched.
  void OnReloadButtonRendererProcessCreatedAndLaunched(
      base::TimeTicks created_timestamp,
      base::TimeTicks launched_timestamp);

  // Called when the reload button renderer process terminates or crashes.
  void OnReloadButtonRenderProcessGone();

  // Skips recording startup metrics for testing.
  void SkipStartupForTesting();

  // Resets the static markers tracking whether startup metrics have been
  // emitted.
  static void ResetForTesting();

  base::OneShotTimer* GetUnpainted10sTimerForTesting() {
    return &unpainted_10s_timer_;
  }

 private:
  enum class UnpaintedTerminationReason {
    kWindowClosed,
    kRenderProcessGone,
  };

  // Helper to record unpainted termination metrics on window close or crash.
  void RecordUnpaintedTermination(UnpaintedTerminationReason reason);

  // Helper to record visual readiness at browser first presentation.
  void RecordPaintedAtBrowserFirstPaint(bool painted);

  // Helper to record 10-second SLO metric and stop the timer.
  void RecordPaintedWithin10Seconds(bool painted);

  // Helper to record surface sync outcome when feature is enabled.
  void RecordSurfaceSyncResult(waap::InitialWebUISurfaceSyncResult result);

  // Helper to record latency to paint after deadline when feature is enabled.
  void RecordSurfaceSyncTimeToPaintAfterDeadline(base::TimeDelta delta);

  // Helper to emit the delta metric once both timestamps are available.
  void RecordPaintDeltaIfAvailable();

  // Invoked when 10 seconds elapse after browser window presentation without
  // reload button paint.
  void OnUnpainted10sTimeout();

  // The service used to record metrics. May be null if the profile is null.
  const raw_ptr<WaapUIMetricsService> waap_service_;

  const raw_ptr<BrowserWindowInterface> browser_;

  ui::ScopedUnownedUserData<InitialWebUIWindowMetricsManager>
      scoped_data_holder_;

  // The source of this window's creation.
  // Updated when `SetWindowCreationInfo` is called. Can still be null if the
  // window was created via uninterested paths.
  waap::NewWindowCreationSource creation_source_ =
      waap::NewWindowCreationSource::kUnknown;

  // The timestamp when this window creation was initiated.
  // Updated when `SetWindowCreationInfo` is called. Can still be null if the
  // window was created via uninterested paths.
  std::optional<base::TimeTicks> new_window_start_time_;

  bool is_new_window_first_paint_recorded_ = false;
  bool is_new_window_reload_button_first_paint_recorded_ = false;
  bool is_new_window_reload_button_first_contentful_paint_recorded_ = false;

  bool startup_delta_recorded_ = false;
  bool new_window_delta_recorded_ = false;

  base::OneShotTimer unpainted_10s_timer_;
  bool painted_at_browser_first_paint_recorded_ = false;
  bool painted_within_10s_recorded_ = false;
  bool surface_sync_result_recorded_ = false;
  bool unpainted_termination_recorded_ = false;

  // Track timestamps to calculate the delta between the two paint events.
  std::optional<base::TimeTicks> browser_window_first_paint_time_;
  std::optional<base::TimeTicks> reload_button_first_paint_time_;

  // Track the first time a request to show the window was made.
  std::optional<base::TimeTicks> window_show_first_requested_time_;

  bool skip_startup_metrics_for_testing_ = false;
  bool was_created_with_existing_windows_ = false;
  bool should_skip_latency_metrics_ = false;

  base::WeakPtrFactory<InitialWebUIWindowMetricsManager> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_WINDOW_METRICS_MANAGER_H_
