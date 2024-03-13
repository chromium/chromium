// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_METRICS_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_METRICS_HANDLER_H_

#include "base/component_export.h"

namespace ash {

// AudioDeviceMetricsHandler handles the firing of cras audio related histogram
// metrics.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO)
    AudioDeviceMetricsHandler {
 public:
  AudioDeviceMetricsHandler() = default;
  AudioDeviceMetricsHandler(const AudioDeviceMetricsHandler&) = delete;
  AudioDeviceMetricsHandler& operator=(const AudioDeviceMetricsHandler&) =
      delete;
  ~AudioDeviceMetricsHandler() = default;

  // A series of histogram metrics to record system selection decision after
  // audio device has changed.
  static constexpr char kSystemSwitchInputAudioChromeRestarts[] =
      "ChromeOS.AudioSelection.Input.SystemSwitchAudio.ChromeRestarts";
  static constexpr char kSystemSwitchOutputAudioChromeRestarts[] =
      "ChromeOS.AudioSelection.Output.SystemSwitchAudio.ChromeRestarts";
  static constexpr char kSystemSwitchInputAudioNonChromeRestarts[] =
      "ChromeOS.AudioSelection.Input.SystemSwitchAudio.NonChromeRestarts";
  static constexpr char kSystemSwitchOutputAudioNonChromeRestarts[] =
      "ChromeOS.AudioSelection.Output.SystemSwitchAudio.NonChromeRestarts";

  // Record system selection related metrics in the case of chrome restarts,
  // including system boots and users sign out, as well as the case of normal
  // user hotplug or unplug.
  void RecordAudioSelectionMetricsSeparatedByChromeRestarts(
      bool is_input,
      bool is_switched,
      bool is_chrome_restarts) const;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_METRICS_HANDLER_H_
