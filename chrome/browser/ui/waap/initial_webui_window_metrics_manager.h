// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_WINDOW_METRICS_MANAGER_H_
#define CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_WINDOW_METRICS_MANAGER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/waap/waap_utils.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class Profile;
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

  // Skips recording startup metrics for testing.
  void SkipStartupForTesting();

 private:
  // The service used to record metrics. May be null if the feature is disabled.
  const raw_ptr<WaapUIMetricsService> waap_service_;

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
  std::optional<base::TimeTicks> new_window_start_time_ = std::nullopt;

  bool is_new_window_first_paint_recorded_ = false;
  bool is_new_window_reload_button_first_paint_recorded_ = false;
  bool is_new_window_reload_button_first_contentful_paint_recorded_ = false;

  bool startup_delta_recorded_ = false;
  bool new_window_delta_recorded_ = false;

  // Track timestamps to calculate the delta between the two paint events.
  std::optional<base::TimeTicks> browser_window_first_paint_time_;
  std::optional<base::TimeTicks> reload_button_first_paint_time_;

  // Helper to emit the delta metric once both timestamps are available.
  void RecordPaintDeltaIfAvailable();

  bool skip_startup_metrics_for_testing_ = false;
};

#endif  // CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_WINDOW_METRICS_MANAGER_H_
