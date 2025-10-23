// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_WAAP_UI_METRICS_SERVICE_H_
#define CHROME_BROWSER_UI_WAAP_WAAP_UI_METRICS_SERVICE_H_

#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_recorder.h"
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

  // Convenient method to get an instance for the given `profile`.
  // May return nullptr.
  static WaapUIMetricsService* Get(Profile* profile);

  // Called whenever the WaaP UI has its first paint finished.
  void OnFirstPaint(base::TimeTicks time);

  // Called whenever the WaaP UI has its first contentful paint finished.
  void OnFirstContentfulPaint(base::TimeTicks time);

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
