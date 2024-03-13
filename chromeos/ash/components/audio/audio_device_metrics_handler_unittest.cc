// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device_metrics_handler.h"

#include "base/test/metrics/histogram_tester.h"

namespace ash {
class AudioDeviceMetricsHandlerTest : public testing::Test {
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
       RecordAudioSelectionMetricsSeparatedByChromeRestarts) {
  for (const bool is_input : {true, false}) {
    for (const bool is_switched : {true, false}) {
      for (const bool is_chrome_restarts : {true, false}) {
        audio_device_metrics_handler()
            .RecordAudioSelectionMetricsSeparatedByChromeRestarts(
                is_input, is_switched, is_chrome_restarts);

        std::string system_switch_histogram_name;
        if (is_chrome_restarts) {
          system_switch_histogram_name =
              is_input ? AudioDeviceMetricsHandler::
                             kSystemSwitchInputAudioChromeRestarts
                       : AudioDeviceMetricsHandler::
                             kSystemSwitchOutputAudioChromeRestarts;
        } else {
          system_switch_histogram_name =
              is_input ? AudioDeviceMetricsHandler::
                             kSystemSwitchInputAudioNonChromeRestarts
                       : AudioDeviceMetricsHandler::
                             kSystemSwitchOutputAudioNonChromeRestarts;
        }

        histogram_tester().ExpectBucketCount(system_switch_histogram_name,
                                             is_switched, 1);
      }
    }
  }
}

}  // namespace ash
