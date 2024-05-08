// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device_metrics_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_device_encoding.h"
#include "chromeos/ash/components/audio/audio_device_selection_test_base.h"

namespace ash {

namespace {

constexpr uint16_t kTimeDeltaInMinute = 2;

}  // namespace

class AudioDeviceMetricsHandlerTest : public AudioDeviceSelectionTestBase {
 public:
  const base::HistogramTester& histogram_tester() { return histogram_tester_; }
  AudioDeviceMetricsHandler& audio_device_metrics_handler() {
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
                                           /*expected_count=*/1);

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
          /*expected_count=*/1);

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
          /*expected_count=*/1);

      // Test user override metrics.
      audio_device_metrics_handler()
          .RecordUserOverrideMetricsSeparatedByChromeRestarts(
              is_input, is_switched, /*is_chrome_restarts=*/false,
              /*time_delta_since_system_decision=*/kTimeDeltaInMinute);

      std::string user_override_histogram_name;
      if (is_switched) {
        user_override_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::
                      kUserOverrideSystemSwitchInputAudioNonChromeRestarts
                : AudioDeviceMetricsHandler::
                      kUserOverrideSystemSwitchOutputAudioNonChromeRestarts;

      } else {
        user_override_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::
                      kUserOverrideSystemNotSwitchInputAudioNonChromeRestarts
                : AudioDeviceMetricsHandler::
                      kUserOverrideSystemNotSwitchOutputAudioNonChromeRestarts;
      }

      histogram_tester().ExpectTotalCount(user_override_histogram_name,
                                          /*expected_count=*/1);
      histogram_tester().ExpectTimeBucketCount(
          user_override_histogram_name,
          base::Minutes(kTimeDeltaInMinute) / base::Minutes(1).InMilliseconds(),
          /*expected_count=*/1);
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
                                           /*expected_count=*/1);

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
          /*expected_count=*/1);

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
          /*expected_count=*/1);

      // Test user override metrics.
      audio_device_metrics_handler()
          .RecordUserOverrideMetricsSeparatedByChromeRestarts(
              is_input, is_switched, /*is_chrome_restarts=*/true,
              /*time_delta_since_system_decision=*/kTimeDeltaInMinute);

      std::string user_override_histogram_name;
      if (is_switched) {
        user_override_histogram_name =
            is_input ? AudioDeviceMetricsHandler::
                           kUserOverrideSystemSwitchInputAudioChromeRestarts
                     : AudioDeviceMetricsHandler::
                           kUserOverrideSystemSwitchOutputAudioChromeRestarts;

      } else {
        user_override_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::
                      kUserOverrideSystemNotSwitchInputAudioChromeRestarts
                : AudioDeviceMetricsHandler::
                      kUserOverrideSystemNotSwitchOutputAudioChromeRestarts;
      }

      histogram_tester().ExpectTotalCount(user_override_histogram_name,
                                          /*expected_count=*/1);
      histogram_tester().ExpectTimeBucketCount(
          user_override_histogram_name,
          base::Minutes(kTimeDeltaInMinute) / base::Minutes(1).InMilliseconds(),
          /*expected_count=*/1);
    }
  }
}

// Tests that no audio selection metrics are fired when device set hasn't
// changed.
TEST_F(AudioDeviceMetricsHandlerTest,
       NoAudioSelectionMetricsRecordedWhenNoDeviceChanges) {
  AudioDevice input_USB = AudioDevice(NewInputNode("USB"));
  AudioDevice input_BLUETOOTH = AudioDevice(NewInputNode("BLUETOOTH"));
  AudioDeviceMap previous_devices_map;
  previous_devices_map[input_USB.id] = input_USB;
  previous_devices_map[input_BLUETOOTH.id] = input_BLUETOOTH;
  AudioDeviceMap current_devices_map;
  current_devices_map[input_USB.id] = input_USB;
  current_devices_map[input_BLUETOOTH.id] = input_BLUETOOTH;
  AudioDeviceList previous_devices = {input_USB, input_BLUETOOTH};
  AudioDeviceList current_devices = {input_USB, input_BLUETOOTH};

  for (const bool is_input : {true, false}) {
    for (const bool is_switched : {true, false}) {
      audio_device_metrics_handler().MaybeRecordSystemSwitchDecisionAndContext(
          is_input, /*has_alternative_device=*/true, is_switched,
          current_devices_map, previous_devices_map);

      std::string system_switch_histogram_name =
          is_input ? AudioDeviceMetricsHandler::kSystemSwitchInputAudio
                   : AudioDeviceMetricsHandler::kSystemSwitchOutputAudio;
      histogram_tester().ExpectTotalCount(system_switch_histogram_name, 0);

      std::string device_count_histogram_name;
      if (is_switched) {
        device_count_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::kSystemSwitchInputAudioDeviceCount
                : AudioDeviceMetricsHandler::
                      kSystemSwitchOutputAudioDeviceCount;
      } else {
        device_count_histogram_name =
            is_input ? AudioDeviceMetricsHandler::
                           kSystemNotSwitchInputAudioDeviceCount
                     : AudioDeviceMetricsHandler::
                           kSystemNotSwitchOutputAudioDeviceCount;
      }
      histogram_tester().ExpectTotalCount(device_count_histogram_name,
                                          /*expected_count=*/0);

      std::string device_set_histogram_name;
      if (is_switched) {
        device_set_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::kSystemSwitchInputAudioDeviceSet
                : AudioDeviceMetricsHandler::kSystemSwitchOutputAudioDeviceSet;
      } else {
        device_set_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::kSystemNotSwitchInputAudioDeviceSet
                : AudioDeviceMetricsHandler::
                      kSystemNotSwitchOutputAudioDeviceSet;
      }

      histogram_tester().ExpectTotalCount(device_set_histogram_name,
                                          /*expected_count=*/0);

      std::string before_and_after_device_set_histogram_name;
      if (is_switched) {
        before_and_after_device_set_histogram_name =
            is_input ? AudioDeviceMetricsHandler::
                           kSystemSwitchInputBeforeAndAfterAudioDeviceSet
                     : AudioDeviceMetricsHandler::
                           kSystemSwitchOutputBeforeAndAfterAudioDeviceSet;
      } else {
        before_and_after_device_set_histogram_name =
            is_input ? AudioDeviceMetricsHandler::
                           kSystemNotSwitchInputBeforeAndAfterAudioDeviceSet
                     : AudioDeviceMetricsHandler::
                           kSystemNotSwitchOutputBeforeAndAfterAudioDeviceSet;
      }
      histogram_tester().ExpectTotalCount(
          before_and_after_device_set_histogram_name,
          /*expected_count=*/0);
    }
  }
}

TEST_F(AudioDeviceMetricsHandlerTest, RecordConsecutiveAudioDevicsChange) {
  uint16_t expected_input_devices_changed_count = 0;
  uint16_t expected_output_devices_changed_count = 0;
  uint16_t expected_input_devices_added_count = 0;
  uint16_t expected_output_devices_added_count = 0;
  for (const bool is_input : {true, false}) {
    for (const bool is_device_added : {true, false}) {
      audio_device_metrics_handler()
          .RecordConsecutiveAudioDevicsChangeTimeElapsed(is_input,
                                                         is_device_added);
      std::string devices_changed_histogram_name =
          is_input ? AudioDeviceMetricsHandler::kConsecutiveInputDevicsChanged
                   : AudioDeviceMetricsHandler::kConsecutiveOutputDevicsChanged;
      histogram_tester().ExpectTotalCount(
          devices_changed_histogram_name,
          (is_input ? expected_input_devices_changed_count
                    : expected_output_devices_changed_count)++);

      std::string devices_added_histogram_name =
          is_input ? AudioDeviceMetricsHandler::kConsecutiveInputDevicsChanged
                   : AudioDeviceMetricsHandler::kConsecutiveOutputDevicsChanged;
      histogram_tester().ExpectTotalCount(
          devices_added_histogram_name,
          is_input ? expected_input_devices_added_count
                   : expected_output_devices_added_count);
      if (is_device_added) {
        (is_input ? expected_input_devices_added_count
                  : expected_output_devices_added_count)++;
      }
    }
  }
}

TEST_F(AudioDeviceMetricsHandlerTest,
       RecordConsecutiveAudioDevicsChangeTimeElapsed) {
  constexpr uint16_t kTimeDeltaInSecondA = 5;
  constexpr uint16_t kTimeDeltaInSecondB = 5;

  for (const bool is_input : {true, false}) {
    // Test consecutive devices change.
    audio_device_metrics_handler()
        .RecordConsecutiveAudioDevicsChangeTimeElapsed(
            is_input,
            /*is_device_added=*/false);
    FastForwardBy(base::Seconds(kTimeDeltaInSecondA));
    audio_device_metrics_handler()
        .RecordConsecutiveAudioDevicsChangeTimeElapsed(
            is_input,
            /*is_device_added=*/false);
    std::string devices_changed_histogram_name =
        is_input ? AudioDeviceMetricsHandler::kConsecutiveInputDevicsChanged
                 : AudioDeviceMetricsHandler::kConsecutiveOutputDevicsChanged;
    histogram_tester().ExpectBucketCount(devices_changed_histogram_name,
                                         /*sample=*/kTimeDeltaInSecondA,
                                         /*expected_count=*/1);

    // Test consecutive devices addition.
    audio_device_metrics_handler()
        .RecordConsecutiveAudioDevicsChangeTimeElapsed(
            is_input,
            /*is_device_added=*/true);
    FastForwardBy(base::Seconds(kTimeDeltaInSecondB));
    audio_device_metrics_handler()
        .RecordConsecutiveAudioDevicsChangeTimeElapsed(
            is_input,
            /*is_device_added=*/true);
    std::string devices_added_histogram_name =
        is_input ? AudioDeviceMetricsHandler::kConsecutiveInputDevicsAdded
                 : AudioDeviceMetricsHandler::kConsecutiveOutputDevicsAdded;
    histogram_tester().ExpectBucketCount(devices_added_histogram_name,
                                         /*sample=*/kTimeDeltaInSecondB,
                                         /*expected_count=*/1);
  }
}

// Tests the audio selection exception rules metrics are fired.
TEST_F(AudioDeviceMetricsHandlerTest, RecordExceptionRulesMet) {
  for (int ruleInt = static_cast<int>(
           AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
               kInputRule1HotPlugPrivilegedDevice);
       ruleInt !=
       static_cast<int>(
           AudioDeviceMetricsHandler::AudioSelectionExceptionRules::kMaxValue);
       ruleInt++) {
    AudioDeviceMetricsHandler::AudioSelectionExceptionRules rule =
        static_cast<AudioDeviceMetricsHandler::AudioSelectionExceptionRules>(
            ruleInt);

    // No histogram is recorded before firing.
    histogram_tester().ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
        /*sample=*/rule,
        /*expected_count=*/0);

    audio_device_metrics_handler().RecordExceptionRulesMet(rule);

    // Histogram is recorded after firing.
    histogram_tester().ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
        /*sample=*/rule,
        /*expected_count=*/1);
    histogram_tester().ExpectTotalCount(
        AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
        /*expected_count=*/ruleInt + 1);
  }
}

// Tests the audio selection notification events metrics are fired.
TEST_F(AudioDeviceMetricsHandlerTest, NotificationEvents) {
  for (int eventInt = static_cast<int>(
           AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
               kNotificationWithBothInputAndOutputDevicesShowsUp);
       eventInt !=
       static_cast<int>(AudioDeviceMetricsHandler::
                            AudioSelectionNotificationEvents::kMaxValue);
       eventInt++) {
    AudioDeviceMetricsHandler::AudioSelectionNotificationEvents event =
        static_cast<
            AudioDeviceMetricsHandler::AudioSelectionNotificationEvents>(
            eventInt);

    // No histogram is recorded before firing.
    histogram_tester().ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionNotification,
        /*sample=*/event,
        /*expected_count=*/0);

    audio_device_metrics_handler().RecordNotificationEvents(event);

    // Histogram is recorded after firing.
    histogram_tester().ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionNotification,
        /*sample=*/event,
        /*expected_count=*/1);
    histogram_tester().ExpectTotalCount(
        AudioDeviceMetricsHandler::kAudioSelectionNotification,
        /*expected_count=*/eventInt + 1);
  }
}

// Tests that no audio selection metrics are fired when there is only one device
// currently connected.
TEST_F(AudioDeviceMetricsHandlerTest,
       NoAudioSelectionMetricsRecordedWhenOnlyOneAudioDeviceConnected) {
  AudioDevice input_USB = AudioDevice(NewInputNode("USB"));
  AudioDevice input_BLUETOOTH = AudioDevice(NewInputNode("BLUETOOTH"));
  AudioDeviceMap previous_devices_map;
  previous_devices_map[input_USB.id] = input_USB;
  previous_devices_map[input_BLUETOOTH.id] = input_BLUETOOTH;
  AudioDeviceMap current_devices_map;
  current_devices_map[input_USB.id] = input_USB;
  AudioDeviceList previous_devices = {input_USB, input_BLUETOOTH};
  AudioDeviceList current_devices = {input_USB};

  for (const bool is_input : {true, false}) {
    for (const bool is_switched : {true, false}) {
      audio_device_metrics_handler().MaybeRecordSystemSwitchDecisionAndContext(
          is_input, /*has_alternative_device=*/true, is_switched,
          current_devices_map, previous_devices_map);

      std::string system_switch_histogram_name =
          is_input ? AudioDeviceMetricsHandler::kSystemSwitchInputAudio
                   : AudioDeviceMetricsHandler::kSystemSwitchOutputAudio;
      histogram_tester().ExpectTotalCount(system_switch_histogram_name, 0);

      std::string device_count_histogram_name;
      if (is_switched) {
        device_count_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::kSystemSwitchInputAudioDeviceCount
                : AudioDeviceMetricsHandler::
                      kSystemSwitchOutputAudioDeviceCount;
      } else {
        device_count_histogram_name =
            is_input ? AudioDeviceMetricsHandler::
                           kSystemNotSwitchInputAudioDeviceCount
                     : AudioDeviceMetricsHandler::
                           kSystemNotSwitchOutputAudioDeviceCount;
      }
      histogram_tester().ExpectTotalCount(device_count_histogram_name,
                                          /*expected_count=*/0);

      std::string device_set_histogram_name;
      if (is_switched) {
        device_set_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::kSystemSwitchInputAudioDeviceSet
                : AudioDeviceMetricsHandler::kSystemSwitchOutputAudioDeviceSet;
      } else {
        device_set_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::kSystemNotSwitchInputAudioDeviceSet
                : AudioDeviceMetricsHandler::
                      kSystemNotSwitchOutputAudioDeviceSet;
      }

      histogram_tester().ExpectTotalCount(device_set_histogram_name,
                                          /*expected_count=*/0);

      std::string before_and_after_device_set_histogram_name;
      if (is_switched) {
        before_and_after_device_set_histogram_name =
            is_input ? AudioDeviceMetricsHandler::
                           kSystemSwitchInputBeforeAndAfterAudioDeviceSet
                     : AudioDeviceMetricsHandler::
                           kSystemSwitchOutputBeforeAndAfterAudioDeviceSet;
      } else {
        before_and_after_device_set_histogram_name =
            is_input ? AudioDeviceMetricsHandler::
                           kSystemNotSwitchInputBeforeAndAfterAudioDeviceSet
                     : AudioDeviceMetricsHandler::
                           kSystemNotSwitchOutputBeforeAndAfterAudioDeviceSet;
      }
      histogram_tester().ExpectTotalCount(
          before_and_after_device_set_histogram_name,
          /*expected_count=*/0);
    }
  }
}

// Tests that no audio selection metrics are fired when there is no device
// currently connected.
TEST_F(AudioDeviceMetricsHandlerTest,
       NoAudioSelectionMetricsRecordedWhenNoAudioDeviceConnected) {
  AudioDevice input_USB = AudioDevice(NewInputNode("USB"));
  AudioDeviceMap previous_devices_map;
  previous_devices_map[input_USB.id] = input_USB;
  AudioDeviceMap current_devices_map;
  AudioDeviceList previous_devices = {input_USB};
  AudioDeviceList current_devices = {};

  for (const bool is_input : {true, false}) {
    for (const bool is_switched : {true, false}) {
      audio_device_metrics_handler().MaybeRecordSystemSwitchDecisionAndContext(
          is_input, /*has_alternative_device=*/true, is_switched,
          current_devices_map, previous_devices_map);

      std::string system_switch_histogram_name =
          is_input ? AudioDeviceMetricsHandler::kSystemSwitchInputAudio
                   : AudioDeviceMetricsHandler::kSystemSwitchOutputAudio;
      histogram_tester().ExpectTotalCount(system_switch_histogram_name, 0);

      std::string device_count_histogram_name;
      if (is_switched) {
        device_count_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::kSystemSwitchInputAudioDeviceCount
                : AudioDeviceMetricsHandler::
                      kSystemSwitchOutputAudioDeviceCount;
      } else {
        device_count_histogram_name =
            is_input ? AudioDeviceMetricsHandler::
                           kSystemNotSwitchInputAudioDeviceCount
                     : AudioDeviceMetricsHandler::
                           kSystemNotSwitchOutputAudioDeviceCount;
      }
      histogram_tester().ExpectTotalCount(device_count_histogram_name,
                                          /*expected_count=*/0);

      std::string device_set_histogram_name;
      if (is_switched) {
        device_set_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::kSystemSwitchInputAudioDeviceSet
                : AudioDeviceMetricsHandler::kSystemSwitchOutputAudioDeviceSet;
      } else {
        device_set_histogram_name =
            is_input
                ? AudioDeviceMetricsHandler::kSystemNotSwitchInputAudioDeviceSet
                : AudioDeviceMetricsHandler::
                      kSystemNotSwitchOutputAudioDeviceSet;
      }

      histogram_tester().ExpectTotalCount(device_set_histogram_name,
                                          /*expected_count=*/0);

      std::string before_and_after_device_set_histogram_name;
      if (is_switched) {
        before_and_after_device_set_histogram_name =
            is_input ? AudioDeviceMetricsHandler::
                           kSystemSwitchInputBeforeAndAfterAudioDeviceSet
                     : AudioDeviceMetricsHandler::
                           kSystemSwitchOutputBeforeAndAfterAudioDeviceSet;
      } else {
        before_and_after_device_set_histogram_name =
            is_input ? AudioDeviceMetricsHandler::
                           kSystemNotSwitchInputBeforeAndAfterAudioDeviceSet
                     : AudioDeviceMetricsHandler::
                           kSystemNotSwitchOutputBeforeAndAfterAudioDeviceSet;
      }
      histogram_tester().ExpectTotalCount(
          before_and_after_device_set_histogram_name,
          /*expected_count=*/0);
    }
  }
}

}  // namespace ash
