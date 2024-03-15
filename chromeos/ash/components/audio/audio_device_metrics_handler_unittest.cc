// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device_metrics_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_device_selection_test_base.h"

namespace ash {
class AudioDeviceMetricsHandlerTest : public AudioDeviceSelectionTestBase {
 public:
  const base::HistogramTester& histogram_tester() { return histogram_tester_; }
  const AudioDeviceMetricsHandler& audio_device_metrics_handler() {
    return audio_device_metrics_handler_;
  }

 private:
  base::HistogramTester histogram_tester_;
  AudioDeviceMetricsHandler audio_device_metrics_handler_;
};

TEST_F(AudioDeviceMetricsHandlerTest,
       RecordAudioSelectionMetrics_NonChromeRestarts) {
  AudioDeviceList current_devices = {AudioDevice(NewInputNode("USB")),
                                     AudioDevice(NewInputNode("BLUETOOTH"))};

  for (const bool is_input : {true, false}) {
    for (const bool is_switched : {true, false}) {
      audio_device_metrics_handler()
          .RecordAudioSelectionMetricsSeparatedByChromeRestarts(
              is_input, is_switched, /*is_chrome_restarts=*/false,
              current_devices);

      std::string system_switch_histogram_name =
          is_input ? AudioDeviceMetricsHandler::
                         kSystemSwitchInputAudioNonChromeRestarts
                   : AudioDeviceMetricsHandler::
                         kSystemSwitchOutputAudioNonChromeRestarts;
      histogram_tester().ExpectBucketCount(system_switch_histogram_name,
                                           is_switched, 1);

      std::string device_count_histogram_name;
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
      histogram_tester().ExpectBucketCount(device_count_histogram_name,
                                           current_devices.size(),
                                           /*bucket_count=*/1);
    }
  }
}

TEST_F(AudioDeviceMetricsHandlerTest,
       RecordAudioSelectionMetrics_ChromeRestarts) {
  AudioDeviceList current_devices = {AudioDevice(NewInputNode("USB")),
                                     AudioDevice(NewInputNode("BLUETOOTH"))};

  for (const bool is_input : {true, false}) {
    for (const bool is_switched : {true, false}) {
      audio_device_metrics_handler()
          .RecordAudioSelectionMetricsSeparatedByChromeRestarts(
              is_input, is_switched, /*is_chrome_restarts=*/true,
              current_devices);

      std::string system_switch_histogram_name =
          is_input
              ? AudioDeviceMetricsHandler::kSystemSwitchInputAudioChromeRestarts
              : AudioDeviceMetricsHandler::
                    kSystemSwitchOutputAudioChromeRestarts;
      histogram_tester().ExpectBucketCount(system_switch_histogram_name,
                                           is_switched, 1);

      std::string device_count_histogram_name;
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
      histogram_tester().ExpectBucketCount(device_count_histogram_name,
                                           current_devices.size(),
                                           /*bucket_count=*/1);
    }
  }
}

}  // namespace ash
