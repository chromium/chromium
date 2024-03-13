// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device_metrics_handler.h"

#include "base/metrics/histogram_functions.h"

namespace ash {

void AudioDeviceMetricsHandler::
    RecordAudioSelectionMetricsSeparatedByChromeRestarts(
        bool is_input,
        bool is_switched,
        bool is_chrome_restarts) const {
  std::string system_switch_histogram_name;
  if (is_chrome_restarts) {
    system_switch_histogram_name =
        is_input
            ? AudioDeviceMetricsHandler::kSystemSwitchInputAudioChromeRestarts
            : AudioDeviceMetricsHandler::kSystemSwitchOutputAudioChromeRestarts;
  } else {
    system_switch_histogram_name =
        is_input ? AudioDeviceMetricsHandler::
                       kSystemSwitchInputAudioNonChromeRestarts
                 : AudioDeviceMetricsHandler::
                       kSystemSwitchOutputAudioNonChromeRestarts;
  }

  base::UmaHistogramBoolean(system_switch_histogram_name, is_switched);
}

}  // namespace ash
