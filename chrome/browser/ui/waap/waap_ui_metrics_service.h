// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_WAAP_UI_METRICS_SERVICE_H_
#define CHROME_BROWSER_UI_WAAP_WAAP_UI_METRICS_SERVICE_H_

#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_recorder.h"
#include "chrome/browser/ui/waap/waap_utils.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class WaapUIMetricsServiceFactory;

// `WaapUIMetricsService` is responsible for receiving UI metrics from WaaP UI
// elements, either renderers or browsers.
//
// It is scoped to the lifetime of a Profile, and is expected to be created in
// all kinds of profiles.
class WaapUIMetricsService : public KeyedService {
 public:
  explicit WaapUIMetricsService(base::PassKey<WaapUIMetricsServiceFactory>);

  // Disallow copy and assign.
  WaapUIMetricsService(const WaapUIMetricsService&) = delete;
  WaapUIMetricsService& operator=(const WaapUIMetricsService&) = delete;

  ~WaapUIMetricsService() override;

  // Returns the instance of the service for the profile.
  // May return nullptr if profile is null.
  static WaapUIMetricsService* Get(Profile* profile);

  // Resets static markers tracking whether startup metrics have been emitted.
  static void ResetForTesting();

  // Called when the browser window is created.
  void OnBrowserWindowCreated();

  // Called when the ReloadButton is created.
  void OnReloadButtonCreated();

  // Called when the renderer process is created and launched.
  void OnReloadButtonRendererProcessCreatedAndLaunched(
      base::TimeTicks created_timestamp,
      base::TimeTicks launched_timestamp);

  // Called when the browser window is presented onto the screen for the first
  // time.
  void OnBrowserWindowFirstPresentation(base::TimeTicks time);

  // Called whenever the WaaP UI has its first paint finished.
  void OnFirstPaint(base::TimeTicks time);

  // Called whenever the WaaP UI has its first contentful paint finished.
  void OnFirstContentfulPaint(base::TimeTicks time);

  // Called when a new browser window (not the initial one) is first painted.
  void OnNewWindowBrowserWindowFirstPresentation(
      waap::NewWindowCreationSource source,
      bool with_existing_window,
      base::TimeTicks start_time,
      base::TimeTicks paint_time);

  // Called when the ReloadButton in a new browser window is first painted.
  void OnNewWindowReloadButtonFirstPaint(waap::NewWindowCreationSource source,
                                         bool with_existing_window,
                                         base::TimeTicks start_time,
                                         base::TimeTicks paint_time);

  // Called when the ReloadButton in a new browser window is first contentful
  // painted.
  void OnNewWindowReloadButtonFirstContentfulPaint(
      waap::NewWindowCreationSource source,
      bool with_existing_window,
      base::TimeTicks start_time,
      base::TimeTicks paint_time);

  // Called when both the browser window and the ReloadButton have painted for
  // the first time during startup.
  void OnStartupBrowserWindowToReloadButtonFirstPaintGap(
      base::TimeTicks browser_window_paint_time,
      base::TimeTicks reload_button_paint_time);

  // Called when both the browser window and the ReloadButton have painted for
  // the first time in a new window.
  void OnNewWindowBrowserWindowToReloadButtonFirstPaintGap(
      waap::NewWindowCreationSource source,
      bool with_existing_window,
      base::TimeTicks browser_window_paint_time,
      base::TimeTicks reload_button_paint_time);

  // Records whether the reload button was painted when the browser window
  // first presents onto the screen.
  void OnReloadButtonPaintedAtBrowserFirstPaint(bool reload_button_painted);

  // Records whether the reload button was painted within 10 seconds of the
  // browser window's first presentation.
  void OnReloadButtonPaintedWithin10SecondsAfterBrowserPaint(
      bool painted_within_10s);

  // Records the result of initial surface synchronization for the WebUI
  // toolbar.
  void OnInitialWebUISurfaceSyncResult(
      waap::InitialWebUISurfaceSyncResult result);

  // Records the latency from surface sync deadline expiration to reload button
  // paint.
  void OnSurfaceSyncTimeToPaintAfterDeadline(base::TimeDelta delta);

  // Records the duration from the browser window's first presentation until
  // the reload button's first paint.
  void OnBrowserPaintToReloadButtonPaint(base::TimeDelta delta);

  // Records the duration from the browser window's first presentation to
  // window closure when the reload button has not yet completed its first
  // paint.
  void OnReloadButtonBrowserWindowClosedBeforePaint(base::TimeDelta delta);

  // Records the input type used to activate the ReloadButton.
  void OnReloadButtonInput(
      WaapUIMetricsRecorder::ReloadButtonInputType input_type);

  void RecordReloadButtonInteractionToReload(
      base::TimeTicks interaction_ticks,
      base::TimeTicks execution_ticks,
      WaapUIMetricsRecorder::ReloadButtonInputType input_type);

  // Called when the first browser window is painted after it's requested to be
  // shown during startup.
  void OnStartupBrowserWindowShowRequestedToFirstPaint(
      base::TimeTicks request_time,
      base::TimeTicks paint_time);

  // Called when a new browser window (not the initial one) is first painted
  // after it's requested to be shown.
  void OnNewWindowBrowserWindowShowRequestedToFirstPaint(
      waap::NewWindowCreationSource source,
      bool with_existing_window,
      base::TimeTicks request_time,
      base::TimeTicks paint_time);

  // Called when the browser window is closed before the first paint.
  void OnStartupBrowserWindowClosedBeforeFirstPaint(
      base::TimeTicks request_time,
      base::TimeTicks close_time);

  // Called when a new browser window (not the initial one) is closed before the
  // first paint.
  void OnNewWindowBrowserWindowClosedBeforeFirstPaint(
      waap::NewWindowCreationSource source,
      bool with_existing_window,
      base::TimeTicks start_time,
      base::TimeTicks close_time);
};

#endif  // CHROME_BROWSER_UI_WAAP_WAAP_UI_METRICS_SERVICE_H_
