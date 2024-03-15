// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device_metrics_handler.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"

namespace ash {

void AudioDeviceMetricsHandler::
    RecordAudioSelectionMetricsSeparatedByChromeRestarts(
        bool is_input,
        bool is_switched,
        bool is_chrome_restarts,
        const AudioDeviceList& current_device_list) const {
  std::string system_switch_histogram_name;
  std::string device_count_histogram_name;
  if (is_chrome_restarts) {
    system_switch_histogram_name =
        is_input
            ? AudioDeviceMetricsHandler::kSystemSwitchInputAudioChromeRestarts
            : AudioDeviceMetricsHandler::kSystemSwitchOutputAudioChromeRestarts;

    if (is_switched) {
      device_count_histogram_name =
          is_input ? AudioDeviceMetricsHandler::
                         kSystemSwitchInputAudioDeviceCountChromeRestarts
                   : AudioDeviceMetricsHandler::
                         kSystemSwitchOutputAudioDeviceCountChromeRestarts;
    } else {
      device_count_histogram_name =
          is_input ? AudioDeviceMetricsHandler::
                         kSystemNotSwitchInputAudioDeviceCountChromeRestarts
                   : AudioDeviceMetricsHandler::
                         kSystemNotSwitchOutputAudioDeviceCountChromeRestarts;
    }
  } else {
    system_switch_histogram_name =
        is_input ? AudioDeviceMetricsHandler::
                       kSystemSwitchInputAudioNonChromeRestarts
                 : AudioDeviceMetricsHandler::
                       kSystemSwitchOutputAudioNonChromeRestarts;

    if (is_switched) {
      device_count_histogram_name =
          is_input ? AudioDeviceMetricsHandler::
                         kSystemSwitchInputAudioDeviceCountNonChromeRestarts
                   : AudioDeviceMetricsHandler::
                         kSystemSwitchOutputAudioDeviceCountNonChromeRestarts;
    } else {
      device_count_histogram_name =
          is_input
              ? AudioDeviceMetricsHandler::
                    kSystemNotSwitchInputAudioDeviceCountNonChromeRestarts
              : AudioDeviceMetricsHandler::
                    kSystemNotSwitchOutputAudioDeviceCountNonChromeRestarts;
    }
  }

  // Record the system switch decision.
  base::UmaHistogramBoolean(system_switch_histogram_name, is_switched);

  // Record the number of audio devices.
  base::UmaHistogramExactLinear(device_count_histogram_name,
                                current_device_list.size(),
                                CrasAudioHandler::kMaxAudioDevicesCount);
}

}  // namespace ash
