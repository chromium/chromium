// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device_metrics_handler.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/audio/audio_device_encoding.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"

namespace ash {

void AudioDeviceMetricsHandler::
    RecordAudioSelectionMetricsSeparatedByChromeRestarts(
        bool is_input,
        bool is_switched,
        bool is_chrome_restarts,
        const AudioDeviceList& previous_device_list,
        const AudioDeviceList& current_device_list) const {
  std::string system_switch_histogram_name;
  std::string device_count_histogram_name;
  std::string device_set_histogram_name;
  std::string before_and_after_device_set_histogram_name;

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
      device_set_histogram_name =
          is_input ? AudioDeviceMetricsHandler::
                         kSystemSwitchInputAudioDeviceSetChromeRestarts
                   : AudioDeviceMetricsHandler::
                         kSystemSwitchOutputAudioDeviceSetChromeRestarts;
      before_and_after_device_set_histogram_name =
          is_input
              ? AudioDeviceMetricsHandler::
                    kSystemSwitchInputBeforeAndAfterAudioDeviceSetChromeRestarts
              : AudioDeviceMetricsHandler::
                    kSystemSwitchOutputBeforeAndAfterAudioDeviceSetChromeRestarts;
    } else {
      device_count_histogram_name =
          is_input ? AudioDeviceMetricsHandler::
                         kSystemNotSwitchInputAudioDeviceCountChromeRestarts
                   : AudioDeviceMetricsHandler::
                         kSystemNotSwitchOutputAudioDeviceCountChromeRestarts;
      device_set_histogram_name =
          is_input ? AudioDeviceMetricsHandler::
                         kSystemNotSwitchInputAudioDeviceSetChromeRestarts
                   : AudioDeviceMetricsHandler::
                         kSystemNotSwitchOutputAudioDeviceSetChromeRestarts;
      before_and_after_device_set_histogram_name =
          is_input
              ? AudioDeviceMetricsHandler::
                    kSystemNotSwitchInputBeforeAndAfterAudioDeviceSetChromeRestarts
              : AudioDeviceMetricsHandler::
                    kSystemNotSwitchOutputBeforeAndAfterAudioDeviceSetChromeRestarts;
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
      device_set_histogram_name =
          is_input ? AudioDeviceMetricsHandler::
                         kSystemSwitchInputAudioDeviceSetNonChromeRestarts
                   : AudioDeviceMetricsHandler::
                         kSystemSwitchOutputAudioDeviceSetNonChromeRestarts;
      before_and_after_device_set_histogram_name =
          is_input
              ? AudioDeviceMetricsHandler::
                    kSystemSwitchInputBeforeAndAfterAudioDeviceSetNonChromeRestarts
              : AudioDeviceMetricsHandler::
                    kSystemSwitchOutputBeforeAndAfterAudioDeviceSetNonChromeRestarts;
    } else {
      device_count_histogram_name =
          is_input
              ? AudioDeviceMetricsHandler::
                    kSystemNotSwitchInputAudioDeviceCountNonChromeRestarts
              : AudioDeviceMetricsHandler::
                    kSystemNotSwitchOutputAudioDeviceCountNonChromeRestarts;
      device_set_histogram_name =
          is_input ? AudioDeviceMetricsHandler::
                         kSystemNotSwitchInputAudioDeviceSetNonChromeRestarts
                   : AudioDeviceMetricsHandler::
                         kSystemNotSwitchOutputAudioDeviceSetNonChromeRestarts;
      before_and_after_device_set_histogram_name =
          is_input
              ? AudioDeviceMetricsHandler::
                    kSystemNotSwitchInputBeforeAndAfterAudioDeviceSetNonChromeRestarts
              : AudioDeviceMetricsHandler::
                    kSystemNotSwitchOutputBeforeAndAfterAudioDeviceSetNonChromeRestarts;
    }
  }

  // Record the system switch decision.
  base::UmaHistogramBoolean(system_switch_histogram_name, is_switched);

  // Record the number of audio devices.
  base::UmaHistogramExactLinear(device_count_histogram_name,
                                current_device_list.size(),
                                CrasAudioHandler::kMaxAudioDevicesCount);

  // Record the encoded device set.
  base::UmaHistogramSparse(device_set_histogram_name,
                           EncodeAudioDeviceSet(current_device_list));

  // Record the before and after encoded device sets.
  base::UmaHistogramSparse(before_and_after_device_set_histogram_name,
                           EncodeBeforeAndAfterAudioDeviceSets(
                               previous_device_list, current_device_list));
}

}  // namespace ash
