// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device_metrics_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_device_encoding.h"
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
  AudioDevice input_USB = AudioDevice(NewInputNode("USB"));
  AudioDevice input_BLUETOOTH = AudioDevice(NewInputNode("BLUETOOTH"));
  AudioDeviceList previous_devices = {input_USB};
  AudioDeviceList current_devices = {input_USB, input_BLUETOOTH};

  for (const bool is_input : {true, false}) {
    for (const bool is_switched : {true, false}) {
      audio_device_metrics_handler()
          .RecordAudioSelectionMetricsSeparatedByChromeRestarts(
              is_input, is_switched, /*is_chrome_restarts=*/false,
              previous_devices, current_devices);

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

      std::string device_set_histogram_name;
      if (is_switched) {
        device_set_histogram_name =
            is_input ? AudioDeviceMetricsHandler::
                           kSystemSwitchInputAudioDeviceSetNonChromeRestarts
                     : AudioDeviceMetricsHandler::
                           kSystemSwitchOutputAudioDeviceSetNonChromeRestarts;
      } else {
        device_set_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::
                      kSystemNotSwitchInputAudioDeviceSetNonChromeRestarts
                : AudioDeviceMetricsHandler::
                      kSystemNotSwitchOutputAudioDeviceSetNonChromeRestarts;
      }

      histogram_tester().ExpectBucketCount(
          device_set_histogram_name, EncodeAudioDeviceSet(current_devices),
          /*bucket_count=*/1);

      std::string before_and_after_device_set_histogram_name;
      if (is_switched) {
        before_and_after_device_set_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::
                      kSystemSwitchInputBeforeAndAfterAudioDeviceSetNonChromeRestarts
                : AudioDeviceMetricsHandler::
                      kSystemSwitchOutputBeforeAndAfterAudioDeviceSetNonChromeRestarts;
      } else {
        before_and_after_device_set_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::
                      kSystemNotSwitchInputBeforeAndAfterAudioDeviceSetNonChromeRestarts
                : AudioDeviceMetricsHandler::
                      kSystemNotSwitchOutputBeforeAndAfterAudioDeviceSetNonChromeRestarts;
      }
      histogram_tester().ExpectBucketCount(
          before_and_after_device_set_histogram_name,
          EncodeBeforeAndAfterAudioDeviceSets(previous_devices,
                                              current_devices),
          /*bucket_count=*/1);
    }
  }
}

TEST_F(AudioDeviceMetricsHandlerTest,
       RecordAudioSelectionMetrics_ChromeRestarts) {
  AudioDevice input_USB = AudioDevice(NewInputNode("USB"));
  AudioDevice input_BLUETOOTH = AudioDevice(NewInputNode("BLUETOOTH"));
  AudioDeviceList previous_devices = {};
  AudioDeviceList current_devices = {input_USB, input_BLUETOOTH};

  for (const bool is_input : {true, false}) {
    for (const bool is_switched : {true, false}) {
      audio_device_metrics_handler()
          .RecordAudioSelectionMetricsSeparatedByChromeRestarts(
              is_input, is_switched, /*is_chrome_restarts=*/true,
              previous_devices, current_devices);

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

      std::string device_set_histogram_name;
      if (is_switched) {
        device_set_histogram_name =
            is_input ? AudioDeviceMetricsHandler::
                           kSystemSwitchInputAudioDeviceSetChromeRestarts
                     : AudioDeviceMetricsHandler::
                           kSystemSwitchOutputAudioDeviceSetChromeRestarts;
      } else {
        device_set_histogram_name =
            is_input ? AudioDeviceMetricsHandler::
                           kSystemNotSwitchInputAudioDeviceSetChromeRestarts
                     : AudioDeviceMetricsHandler::
                           kSystemNotSwitchOutputAudioDeviceSetChromeRestarts;
      }
      histogram_tester().ExpectBucketCount(
          device_set_histogram_name, EncodeAudioDeviceSet(current_devices),
          /*bucket_count=*/1);

      std::string before_and_after_device_set_histogram_name;
      if (is_switched) {
        before_and_after_device_set_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::
                      kSystemSwitchInputBeforeAndAfterAudioDeviceSetChromeRestarts
                : AudioDeviceMetricsHandler::
                      kSystemSwitchOutputBeforeAndAfterAudioDeviceSetChromeRestarts;
      } else {
        before_and_after_device_set_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::
                      kSystemNotSwitchInputBeforeAndAfterAudioDeviceSetChromeRestarts
                : AudioDeviceMetricsHandler::
                      kSystemNotSwitchOutputBeforeAndAfterAudioDeviceSetChromeRestarts;
      }
      histogram_tester().ExpectBucketCount(
          before_and_after_device_set_histogram_name,
          EncodeBeforeAndAfterAudioDeviceSets(previous_devices,
                                              current_devices),
          /*bucket_count=*/1);
    }
  }
}

}  // namespace ash
