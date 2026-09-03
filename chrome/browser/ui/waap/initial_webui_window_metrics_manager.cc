// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/base_window.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace {

// Tracks process-wide startup metrics. Startup metrics must be recorded at
// most once per browser process lifecycle across all windows.
// Declared in the anonymous namespace so `ResetForTesting()` can reset state
// between tests.
bool g_is_startup_first_paint_recorded = false;
bool g_is_startup_reload_first_paint_recorded = false;
bool g_is_startup_reload_first_contentful_paint_recorded = false;
bool g_is_startup_process_recorded = false;
bool g_process_startup_delta_recorded = false;

}  // namespace

DEFINE_USER_DATA(InitialWebUIWindowMetricsManager);

InitialWebUIWindowMetricsManager::InitialWebUIWindowMetricsManager(
    BrowserWindowInterface* browser)
    : waap_service_(WaapUIMetricsService::Get(browser->GetProfile())),
      browser_(browser),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this),
      was_created_with_existing_windows_(
          ProfileBrowserCollection::GetForProfile(browser->GetProfile())
              ->GetSize() > 0) {}

InitialWebUIWindowMetricsManager::~InitialWebUIWindowMetricsManager() {
  CHECK(waap_service_);
  RecordUnpaintedTermination(UnpaintedTerminationReason::kWindowClosed);

  // TODO(crbug.com/552776216): Skip recording ClosedBeforeFirstPaint metrics
  // when `should_skip_latency_metrics_` is true (e.g. for minimized windows) to
  // avoid logging background idle duration before window closure.

  if (!g_is_startup_first_paint_recorded &&
      !skip_startup_metrics_for_testing_ &&
      window_show_first_requested_time_.has_value()) {
    waap_service_->OnStartupBrowserWindowClosedBeforeFirstPaint(
        *window_show_first_requested_time_, base::TimeTicks::Now());
  } else if (!is_new_window_first_paint_recorded_ &&
             new_window_start_time_.has_value()) {
    waap_service_->OnNewWindowBrowserWindowClosedBeforeFirstPaint(
        creation_source_, was_created_with_existing_windows_,
        *new_window_start_time_, base::TimeTicks::Now());
  }
}

void InitialWebUIWindowMetricsManager::RecordUnpaintedTermination(
    UnpaintedTerminationReason reason) {
  if (unpainted_termination_recorded_ || should_skip_latency_metrics_ ||
      reload_button_first_paint_time_.has_value()) {
    return;
  }
  unpainted_termination_recorded_ = true;

  if (browser_window_first_paint_time_.has_value()) {
    RecordPaintedAtBrowserFirstPaint(/*painted=*/false);
    RecordPaintedWithin10Seconds(/*painted=*/false);
    if (reason == UnpaintedTerminationReason::kWindowClosed) {
      waap_service_->OnReloadButtonBrowserWindowClosedBeforePaint(
          base::TimeTicks::Now() - *browser_window_first_paint_time_);
    }
  }

  using waap::InitialWebUISurfaceSyncResult;
  const bool closed = (reason == UnpaintedTerminationReason::kWindowClosed);
  RecordSurfaceSyncResult(
      browser_window_first_paint_time_.has_value()
          ? (closed ? InitialWebUISurfaceSyncResult::
                          kDeadlineExceededClosedBeforePaint
                    : InitialWebUISurfaceSyncResult::
                          kDeadlineExceededRenderProcessGone)
          : (closed ? InitialWebUISurfaceSyncResult::
                          kClosedBeforeBrowserPresentation
                    : InitialWebUISurfaceSyncResult::
                          kRenderProcessGoneBeforeBrowserPresentation));
}

void InitialWebUIWindowMetricsManager::RecordPaintedAtBrowserFirstPaint(
    bool painted) {
  if (!painted_at_browser_first_paint_recorded_) {
    painted_at_browser_first_paint_recorded_ = true;
    waap_service_->OnReloadButtonPaintedAtBrowserFirstPaint(painted);
  }
}

void InitialWebUIWindowMetricsManager::RecordPaintedWithin10Seconds(
    bool painted) {
  if (!painted_within_10s_recorded_) {
    painted_within_10s_recorded_ = true;
    unpainted_10s_timer_.Stop();
    waap_service_->OnReloadButtonPaintedWithin10SecondsAfterBrowserPaint(
        painted);
  }
}

void InitialWebUIWindowMetricsManager::RecordSurfaceSyncResult(
    waap::InitialWebUISurfaceSyncResult result) {
  if (!surface_sync_result_recorded_ &&
      base::FeatureList::IsEnabled(blink::features::kInitialWebUISurfaceSync)) {
    surface_sync_result_recorded_ = true;
    waap_service_->OnInitialWebUISurfaceSyncResult(result);
  }
}

void InitialWebUIWindowMetricsManager::
    RecordSurfaceSyncTimeToPaintAfterDeadline(base::TimeDelta delta) {
  if (!surface_sync_result_recorded_ &&
      base::FeatureList::IsEnabled(blink::features::kInitialWebUISurfaceSync)) {
    waap_service_->OnSurfaceSyncTimeToPaintAfterDeadline(delta);
  }
}

void InitialWebUIWindowMetricsManager::OnReloadButtonRenderProcessGone() {
  CHECK(waap_service_);
  RecordUnpaintedTermination(UnpaintedTerminationReason::kRenderProcessGone);
}

void InitialWebUIWindowMetricsManager::OnUnpainted10sTimeout() {
  if (should_skip_latency_metrics_) {
    return;
  }
  CHECK(waap_service_);
  RecordPaintedAtBrowserFirstPaint(/*painted=*/false);
  RecordPaintedWithin10Seconds(/*painted=*/false);
}

InitialWebUIWindowMetricsManager* InitialWebUIWindowMetricsManager::From(
    BrowserWindowInterface* browser_window_interface) {
  return browser_window_interface
             ? Get(browser_window_interface->GetUnownedUserDataHost())
             : nullptr;
}

void InitialWebUIWindowMetricsManager::SetWindowCreationInfo(
    waap::NewWindowCreationSource source,
    base::TimeTicks creation_time) {
  creation_source_ = source;
  new_window_start_time_ = creation_time;
}

void InitialWebUIWindowMetricsManager::OnBrowserWindowShowRequested(
    base::TimeTicks time) {
  if (browser_ && browser_->GetWindow() &&
      browser_->GetWindow()->IsMinimized()) {
    should_skip_latency_metrics_ = true;
    return;
  }
  if (!window_show_first_requested_time_.has_value()) {
    window_show_first_requested_time_ = time;
  }
}

void InitialWebUIWindowMetricsManager::OnBrowserWindowFirstPresentation(
    base::TimeTicks timestamp) {
  if (should_skip_latency_metrics_ || timestamp.is_null()) {
    return;
  }
  // Ensures only one startup window is recorded per browser process.
  bool& is_startup_first_paint_recorded = g_is_startup_first_paint_recorded;
  CHECK(waap_service_);

  if (window_show_first_requested_time_.has_value()) {
    // Record ShowRequestedToFirstPaint metric.
    if (!is_startup_first_paint_recorded &&
        !skip_startup_metrics_for_testing_) {
      waap_service_->OnStartupBrowserWindowShowRequestedToFirstPaint(
          *window_show_first_requested_time_, timestamp);
    } else if (!is_new_window_first_paint_recorded_ &&
               new_window_start_time_.has_value()) {
      waap_service_->OnNewWindowBrowserWindowShowRequestedToFirstPaint(
          creation_source_, was_created_with_existing_windows_,
          *window_show_first_requested_time_, timestamp);
    }
  }

  if (!browser_window_first_paint_time_.has_value()) {
    browser_window_first_paint_time_ = timestamp;
    const bool reload_button_painted =
        reload_button_first_paint_time_.has_value();
    if (reload_button_painted) {
      if (!unpainted_termination_recorded_) {
        RecordPaintedAtBrowserFirstPaint(/*painted=*/true);
        RecordPaintedWithin10Seconds(/*painted=*/true);
        RecordSurfaceSyncResult(
            waap::InitialWebUISurfaceSyncResult::kReadyWithinDeadline);
      }
    } else if (!unpainted_termination_recorded_) {
      unpainted_10s_timer_.Start(
          FROM_HERE, base::Seconds(10),
          base::BindOnce(
              &InitialWebUIWindowMetricsManager::OnUnpainted10sTimeout,
              weak_ptr_factory_.GetWeakPtr()));
    } else {
      RecordPaintedAtBrowserFirstPaint(/*painted=*/false);
    }
    RecordPaintDeltaIfAvailable();
  }

  if (!is_startup_first_paint_recorded && !skip_startup_metrics_for_testing_) {
    is_startup_first_paint_recorded = true;
    is_new_window_first_paint_recorded_ = true;
    waap_service_->OnBrowserWindowFirstPresentation(timestamp);
  } else if (!is_new_window_first_paint_recorded_ &&
             new_window_start_time_.has_value()) {
    is_new_window_first_paint_recorded_ = true;
    waap_service_->OnNewWindowBrowserWindowFirstPresentation(
        creation_source_, was_created_with_existing_windows_,
        *new_window_start_time_, timestamp);
  }
}

void InitialWebUIWindowMetricsManager::OnBrowserWindowCreated() {
  CHECK(waap_service_);
  waap_service_->OnBrowserWindowCreated();
}

void InitialWebUIWindowMetricsManager::OnReloadButtonCreated() {
  CHECK(waap_service_);
  waap_service_->OnReloadButtonCreated();
}

void InitialWebUIWindowMetricsManager::OnReloadButtonFirstPaint(
    base::TimeTicks timestamp) {
  if (should_skip_latency_metrics_ || timestamp.is_null()) {
    return;
  }
  // Ensures only one startup reload button is recorded per browser process.
  bool& is_startup_first_paint_recorded =
      g_is_startup_reload_first_paint_recorded;
  CHECK(waap_service_);

  if (!reload_button_first_paint_time_.has_value()) {
    reload_button_first_paint_time_ = timestamp;
    if (browser_window_first_paint_time_.has_value() &&
        !unpainted_termination_recorded_) {
      const bool painted_at_first_paint =
          timestamp <= *browser_window_first_paint_time_;
      RecordPaintedAtBrowserFirstPaint(painted_at_first_paint);

      const bool painted_within_10s =
          (timestamp - *browser_window_first_paint_time_) <= base::Seconds(10);
      RecordPaintedWithin10Seconds(painted_within_10s);
      if (!surface_sync_result_recorded_) {
        const base::TimeDelta delta =
            timestamp - *browser_window_first_paint_time_;
        if (delta <= base::TimeDelta()) {
          RecordSurfaceSyncResult(
              waap::InitialWebUISurfaceSyncResult::kReadyWithinDeadline);
        } else {
          RecordSurfaceSyncTimeToPaintAfterDeadline(delta);
          RecordSurfaceSyncResult(waap::InitialWebUISurfaceSyncResult::
                                      kDeadlineExceededPaintedLater);
        }
      }
    }
    RecordPaintDeltaIfAvailable();
  }

  if (!is_startup_first_paint_recorded && !skip_startup_metrics_for_testing_) {
    is_startup_first_paint_recorded = true;
    is_new_window_reload_button_first_paint_recorded_ = true;
    waap_service_->OnFirstPaint(timestamp);
  } else if (!is_new_window_reload_button_first_paint_recorded_ &&
             new_window_start_time_.has_value()) {
    is_new_window_reload_button_first_paint_recorded_ = true;
    waap_service_->OnNewWindowReloadButtonFirstPaint(
        creation_source_, was_created_with_existing_windows_,
        *new_window_start_time_, timestamp);
  }
}

void InitialWebUIWindowMetricsManager::OnReloadButtonFirstContentfulPaint(
    base::TimeTicks timestamp) {
  if (should_skip_latency_metrics_ || timestamp.is_null()) {
    return;
  }
  // Ensures only one startup reload button is recorded per browser process.
  bool& is_startup_first_contentful_paint_recorded =
      g_is_startup_reload_first_contentful_paint_recorded;
  CHECK(waap_service_);

  if (!is_startup_first_contentful_paint_recorded &&
      !skip_startup_metrics_for_testing_) {
    is_startup_first_contentful_paint_recorded = true;
    is_new_window_reload_button_first_contentful_paint_recorded_ = true;
    waap_service_->OnFirstContentfulPaint(timestamp);
  } else if (!is_new_window_reload_button_first_contentful_paint_recorded_ &&
             new_window_start_time_.has_value()) {
    is_new_window_reload_button_first_contentful_paint_recorded_ = true;
    waap_service_->OnNewWindowReloadButtonFirstContentfulPaint(
        creation_source_, was_created_with_existing_windows_,
        *new_window_start_time_, timestamp);
  }
}

void InitialWebUIWindowMetricsManager::
    OnReloadButtonRendererProcessCreatedAndLaunched(
        base::TimeTicks created_timestamp,
        base::TimeTicks launched_timestamp) {
  if (should_skip_latency_metrics_) {
    return;
  }
  // Ensures only one startup process launch is recorded per browser process.
  bool& is_startup_process_recorded = g_is_startup_process_recorded;
  CHECK(waap_service_);

  if (!is_startup_process_recorded && !skip_startup_metrics_for_testing_) {
    is_startup_process_recorded = true;
    waap_service_->OnReloadButtonRendererProcessCreatedAndLaunched(
        created_timestamp, launched_timestamp);
  }
}

void InitialWebUIWindowMetricsManager::SkipStartupForTesting() {
  skip_startup_metrics_for_testing_ = true;
}

void InitialWebUIWindowMetricsManager::RecordPaintDeltaIfAvailable() {
  CHECK(waap_service_);
  if (!browser_window_first_paint_time_.has_value() ||
      !reload_button_first_paint_time_.has_value()) {
    return;
  }

  // Handle Startup metrics only once per process.
  bool& process_startup_delta_recorded = g_process_startup_delta_recorded;
  bool is_startup_metric = false;
  if (!process_startup_delta_recorded) {
    // Update the `process_startup_delta_recorded` before the negative delta check to make
    // sure that we are in the correct state. Even if the recording is skipped, we don't
    // accidentally record a startup metric for the new window case later.
    is_startup_metric = true;
    process_startup_delta_recorded = true;
  }

  base::TimeDelta delta =
      *reload_button_first_paint_time_ - *browser_window_first_paint_time_;

  if (!unpainted_termination_recorded_) {
    waap_service_->OnBrowserPaintToReloadButtonPaint(delta);
  }

  // We only care about positive deltas.
  if (delta.is_negative()) {
    return;
  }

  if (is_startup_metric && !skip_startup_metrics_for_testing_) {
    startup_delta_recorded_ = true;
    waap_service_->OnStartupBrowserWindowToReloadButtonFirstPaintGap(
        *browser_window_first_paint_time_, *reload_button_first_paint_time_);
  } else if (!new_window_delta_recorded_ &&
             new_window_start_time_.has_value()) {
    // Handle New Window metrics once per window.
    new_window_delta_recorded_ = true;
    waap_service_->OnNewWindowBrowserWindowToReloadButtonFirstPaintGap(
        creation_source_, was_created_with_existing_windows_,
        *browser_window_first_paint_time_, *reload_button_first_paint_time_);
  }
}

// static
void InitialWebUIWindowMetricsManager::ResetForTesting() {
  g_is_startup_first_paint_recorded = false;
  g_is_startup_reload_first_paint_recorded = false;
  g_is_startup_reload_first_contentful_paint_recorded = false;
  g_is_startup_process_recorded = false;
  g_process_startup_delta_recorded = false;
  WaapUIMetricsService::ResetForTesting();
}
