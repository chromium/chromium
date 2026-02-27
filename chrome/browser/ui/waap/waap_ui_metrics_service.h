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
  WaapUIMetricsService(base::PassKey<WaapUIMetricsServiceFactory>,
                       const Profile* profile);

  // Disallow copy and assign.
  WaapUIMetricsService(const WaapUIMetricsService&) = delete;
  WaapUIMetricsService& operator=(const WaapUIMetricsService&) = delete;

  ~WaapUIMetricsService() override;

  // Returns the instance of the service for the profile.
  // May return nullptr.
  static WaapUIMetricsService* Get(Profile* profile);

  // Called when the browser window is created.
  void OnBrowserWindowCreated();

  // Called when the ReloadButton is created.
  void OnReloadButtonCreated();

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
      base::TimeTicks start_time,
      base::TimeTicks paint_time);

  // Called when the ReloadButton in a new browser window is first painted.
  void OnNewWindowReloadButtonFirstPaint(waap::NewWindowCreationSource source,
                                         base::TimeTicks start_time,
                                         base::TimeTicks paint_time);

  // Called when the ReloadButton in a new browser window is first contentful
  // painted.
  void OnNewWindowReloadButtonFirstContentfulPaint(
      waap::NewWindowCreationSource source,
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
      base::TimeTicks browser_window_paint_time,
      base::TimeTicks reload_button_paint_time);

  // Records the time duration from a mousedown event on the WaaP UI element to
  // its visual update, i.e. paint.
  void OnReloadButtonMousePressToNextPaint(base::TimeTicks start_ticks,
                                           base::TimeTicks end_ticks);

  // Records the time duration from a mouseenter event on the WaaP UI element to
  // its visual update, i.e. paint.
  void OnReloadButtonMouseHoverToNextPaint(base::TimeTicks start_ticks,
                                           base::TimeTicks end_ticks);

  // Records the input type used to activate the ReloadButton.
  void OnReloadButtonInput(
      WaapUIMetricsRecorder::ReloadButtonInputType input_type);

  // Records the latency from an input event to the completion of the browser's
  // reload command execution.
  void OnReloadButtonInputToReload(
      base::TimeTicks start_ticks,
      base::TimeTicks end_ticks,
      WaapUIMetricsRecorder::ReloadButtonInputType input_type);

  // Records the latency from an input event to the completion of the browser's
  // stop command execution.
  void OnReloadButtonInputToStop(
      base::TimeTicks start_ticks,
      base::TimeTicks end_ticks,
      WaapUIMetricsRecorder::ReloadButtonInputType input_type);

  // Records the latency from an input event to the next paint of the button.
  void OnReloadButtonInputToNextPaint(
      base::TimeTicks start_ticks,
      base::TimeTicks end_ticks,
      WaapUIMetricsRecorder::ReloadButtonInputType input_type);

  // Records the latency from the initiation of a visible mode change to the
  // first paint of the button in the new mode.
  void OnReloadButtonChangeVisibleModeToNextPaint(
      base::TimeTicks start_ticks,
      base::TimeTicks end_ticks,
      WaapUIMetricsRecorder::ReloadButtonMode new_mode);
};

#endif  // CHROME_BROWSER_UI_WAAP_WAAP_UI_METRICS_SERVICE_H_
