// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "chromeos/ash/components/audio/audio_device_selection_test_base.h"

#include "ash/constants/ash_features.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/audio_device_encoding.h"
#include "chromeos/ash/components/audio/audio_device_metrics_handler.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// The bucket sample constant for testing the system switch or not switch
// decision after audio device has changed.
constexpr uint8_t kSystemNotSwitchSample = 0;
constexpr uint8_t kSystemSwitchSample = 1;

// Test the histogram of system switch or not switch decision after audio device
// has changed.
void ExpectSystemDecisionHistogramCount(
    const base::HistogramTester& histogram_tester,
    uint16_t expected_system_switch_input_count,
    uint16_t expected_system_not_switch_input_count,
    uint16_t expected_system_switch_output_count,
    uint16_t expected_system_not_switch_output_count,
    bool is_chrome_restarts) {
  histogram_tester.ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchInputAudio, kSystemSwitchSample,
      expected_system_switch_input_count);
  histogram_tester.ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchInputAudio,
      kSystemNotSwitchSample, expected_system_not_switch_input_count);
  histogram_tester.ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchOutputAudio, kSystemSwitchSample,
      expected_system_switch_output_count);
  histogram_tester.ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchOutputAudio,
      kSystemNotSwitchSample, expected_system_not_switch_output_count);

  histogram_tester.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      AudioDeviceMetricsHandler::AudioSelectionEvents::kSystemSwitchInput,
      expected_system_switch_input_count);
  histogram_tester.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      AudioDeviceMetricsHandler::AudioSelectionEvents::kSystemNotSwitchInput,
      expected_system_not_switch_input_count);
  histogram_tester.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      AudioDeviceMetricsHandler::AudioSelectionEvents::kSystemSwitchOutput,
      expected_system_switch_output_count);
  histogram_tester.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      AudioDeviceMetricsHandler::AudioSelectionEvents::kSystemNotSwitchOutput,
      expected_system_not_switch_output_count);

  if (is_chrome_restarts) {
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kSystemSwitchInputAudioChromeRestarts,
        kSystemSwitchSample, expected_system_switch_input_count);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kSystemSwitchInputAudioChromeRestarts,
        kSystemNotSwitchSample, expected_system_not_switch_input_count);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kSystemSwitchOutputAudioChromeRestarts,
        kSystemSwitchSample, expected_system_switch_output_count);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kSystemSwitchOutputAudioChromeRestarts,
        kSystemNotSwitchSample, expected_system_not_switch_output_count);

    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionPerformance,
        AudioDeviceMetricsHandler::AudioSelectionEvents::
            kSystemSwitchInputChromeRestart,
        expected_system_switch_input_count);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionPerformance,
        AudioDeviceMetricsHandler::AudioSelectionEvents::
            kSystemNotSwitchInputChromeRestart,
        expected_system_not_switch_input_count);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionPerformance,
        AudioDeviceMetricsHandler::AudioSelectionEvents::
            kSystemSwitchOutputChromeRestart,
        expected_system_switch_output_count);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionPerformance,
        AudioDeviceMetricsHandler::AudioSelectionEvents::
            kSystemNotSwitchOutputChromeRestart,
        expected_system_not_switch_output_count);
  } else {
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kSystemSwitchInputAudioNonChromeRestarts,
        kSystemSwitchSample, expected_system_switch_input_count);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kSystemSwitchInputAudioNonChromeRestarts,
        kSystemNotSwitchSample, expected_system_not_switch_input_count);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kSystemSwitchOutputAudioNonChromeRestarts,
        kSystemSwitchSample, expected_system_switch_output_count);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kSystemSwitchOutputAudioNonChromeRestarts,
        kSystemNotSwitchSample, expected_system_not_switch_output_count);

    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionPerformance,
        AudioDeviceMetricsHandler::AudioSelectionEvents::
            kSystemSwitchInputNonChromeRestart,
        expected_system_switch_input_count);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionPerformance,
        AudioDeviceMetricsHandler::AudioSelectionEvents::
            kSystemNotSwitchInputNonChromeRestart,
        expected_system_not_switch_input_count);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionPerformance,
        AudioDeviceMetricsHandler::AudioSelectionEvents::
            kSystemSwitchOutputNonChromeRestart,
        expected_system_switch_output_count);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionPerformance,
        AudioDeviceMetricsHandler::AudioSelectionEvents::
            kSystemNotSwitchOutputNonChromeRestart,
        expected_system_not_switch_output_count);
  }
}

// Test the histogram of user override after system switch or not switch.
void ExpectUserOverrideSystemDecisionHistogramCount(
    const base::HistogramTester& histogram_tester,
    uint16_t expected_user_override_system_switch_input_count,
    uint16_t expected_user_override_system_not_switch_input_count,
    uint16_t expected_user_override_system_switch_output_count,
    uint16_t expected_user_override_system_not_switch_output_count,
    bool is_chrome_restarts) {
  histogram_tester.ExpectTotalCount(
      AudioDeviceMetricsHandler::kUserOverrideSystemSwitchInputAudio,
      expected_user_override_system_switch_input_count);
  histogram_tester.ExpectTotalCount(
      AudioDeviceMetricsHandler::kUserOverrideSystemNotSwitchInputAudio,
      expected_user_override_system_not_switch_input_count);
  histogram_tester.ExpectTotalCount(
      AudioDeviceMetricsHandler::kUserOverrideSystemSwitchOutputAudio,
      expected_user_override_system_switch_output_count);
  histogram_tester.ExpectTotalCount(
      AudioDeviceMetricsHandler::kUserOverrideSystemNotSwitchOutputAudio,
      expected_user_override_system_not_switch_output_count);

  if (is_chrome_restarts) {
    histogram_tester.ExpectTotalCount(
        AudioDeviceMetricsHandler::
            kUserOverrideSystemSwitchInputAudioChromeRestarts,
        expected_user_override_system_switch_input_count);
    histogram_tester.ExpectTotalCount(
        AudioDeviceMetricsHandler::
            kUserOverrideSystemNotSwitchInputAudioChromeRestarts,
        expected_user_override_system_not_switch_input_count);
    histogram_tester.ExpectTotalCount(
        AudioDeviceMetricsHandler::
            kUserOverrideSystemSwitchOutputAudioChromeRestarts,
        expected_user_override_system_switch_output_count);
    histogram_tester.ExpectTotalCount(
        AudioDeviceMetricsHandler::
            kUserOverrideSystemNotSwitchOutputAudioChromeRestarts,
        expected_user_override_system_not_switch_output_count);
  } else {
    histogram_tester.ExpectTotalCount(
        AudioDeviceMetricsHandler::
            kUserOverrideSystemSwitchInputAudioNonChromeRestarts,
        expected_user_override_system_switch_input_count);
    histogram_tester.ExpectTotalCount(
        AudioDeviceMetricsHandler::
            kUserOverrideSystemNotSwitchInputAudioNonChromeRestarts,
        expected_user_override_system_not_switch_input_count);
    histogram_tester.ExpectTotalCount(
        AudioDeviceMetricsHandler::
            kUserOverrideSystemSwitchOutputAudioNonChromeRestarts,
        expected_user_override_system_switch_output_count);
    histogram_tester.ExpectTotalCount(
        AudioDeviceMetricsHandler::
            kUserOverrideSystemNotSwitchOutputAudioNonChromeRestarts,
        expected_user_override_system_not_switch_output_count);
  }
}

// Test the histogram of user override after system switch or not switch, these
// metrics are only fired within limited time frame.
void ExpectUserOverrideSystemDecisionHistogramCountInLimitedTime(
    const base::HistogramTester& histogram_tester,
    uint16_t expected_user_override_system_switch_input_count_in_limited_time,
    uint16_t
        expected_user_override_system_not_switch_input_count_in_limited_time,
    uint16_t expected_user_override_system_switch_output_count_in_limited_time,
    uint16_t
        expected_user_override_system_not_switch_output_count_in_limited_time,
    bool is_chrome_restarts) {
  histogram_tester.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      AudioDeviceMetricsHandler::AudioSelectionEvents::
          kUserOverrideSystemSwitchInput,
      expected_user_override_system_switch_input_count_in_limited_time);
  histogram_tester.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      AudioDeviceMetricsHandler::AudioSelectionEvents::
          kUserOverrideSystemNotSwitchInput,
      expected_user_override_system_not_switch_input_count_in_limited_time);
  histogram_tester.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      AudioDeviceMetricsHandler::AudioSelectionEvents::
          kUserOverrideSystemSwitchOutput,
      expected_user_override_system_switch_output_count_in_limited_time);
  histogram_tester.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      AudioDeviceMetricsHandler::AudioSelectionEvents::
          kUserOverrideSystemNotSwitchOutput,
      expected_user_override_system_not_switch_output_count_in_limited_time);

  if (is_chrome_restarts) {
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionPerformance,
        AudioDeviceMetricsHandler::AudioSelectionEvents::
            kUserOverrideSystemSwitchInputChromeRestart,
        expected_user_override_system_switch_input_count_in_limited_time);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionPerformance,
        AudioDeviceMetricsHandler::AudioSelectionEvents::
            kUserOverrideSystemNotSwitchInputChromeRestart,
        expected_user_override_system_not_switch_input_count_in_limited_time);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionPerformance,
        AudioDeviceMetricsHandler::AudioSelectionEvents::
            kUserOverrideSystemSwitchOutputChromeRestart,
        expected_user_override_system_switch_output_count_in_limited_time);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionPerformance,
        AudioDeviceMetricsHandler::AudioSelectionEvents::
            kUserOverrideSystemNotSwitchOutputChromeRestart,
        expected_user_override_system_not_switch_output_count_in_limited_time);
  } else {
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionPerformance,
        AudioDeviceMetricsHandler::AudioSelectionEvents::
            kUserOverrideSystemSwitchInputNonChromeRestart,
        expected_user_override_system_switch_input_count_in_limited_time);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionPerformance,
        AudioDeviceMetricsHandler::AudioSelectionEvents::
            kUserOverrideSystemNotSwitchInputNonChromeRestart,
        expected_user_override_system_not_switch_input_count_in_limited_time);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionPerformance,
        AudioDeviceMetricsHandler::AudioSelectionEvents::
            kUserOverrideSystemSwitchOutputNonChromeRestart,
        expected_user_override_system_switch_output_count_in_limited_time);
    histogram_tester.ExpectBucketCount(
        AudioDeviceMetricsHandler::kAudioSelectionPerformance,
        AudioDeviceMetricsHandler::AudioSelectionEvents::
            kUserOverrideSystemNotSwitchOutputNonChromeRestart,
        expected_user_override_system_not_switch_output_count_in_limited_time);
  }
}

// Test the time delta histogram of user override after system switch or not
// switch.
void ExpectUserOverrideSystemDecisionTimeDelta(
    const base::HistogramTester& histogram_tester,
    bool is_input,
    bool system_has_switched,
    uint16_t delta_in_minute,
    bool is_chrome_restarts) {
  std::string histogram_name;
  if (is_input) {
    histogram_name =
        system_has_switched
            ? AudioDeviceMetricsHandler::kUserOverrideSystemSwitchInputAudio
            : AudioDeviceMetricsHandler::kUserOverrideSystemNotSwitchInputAudio;
  } else {
    histogram_name =
        system_has_switched
            ? AudioDeviceMetricsHandler::kUserOverrideSystemSwitchOutputAudio
            : AudioDeviceMetricsHandler::
                  kUserOverrideSystemNotSwitchOutputAudio;
  }

  histogram_tester.ExpectTimeBucketCount(
      histogram_name,
      base::Minutes(delta_in_minute) / base::Minutes(1).InMilliseconds(),
      /*expected_count=*/1);

  std::string histogram_name_separated_by_chrome_restarts;

  if (is_chrome_restarts) {
    if (is_input) {
      histogram_name_separated_by_chrome_restarts =
          system_has_switched
              ? AudioDeviceMetricsHandler::
                    kUserOverrideSystemSwitchInputAudioChromeRestarts
              : AudioDeviceMetricsHandler::
                    kUserOverrideSystemNotSwitchInputAudioChromeRestarts;
    } else {
      histogram_name_separated_by_chrome_restarts =
          system_has_switched
              ? AudioDeviceMetricsHandler::
                    kUserOverrideSystemSwitchOutputAudioChromeRestarts
              : AudioDeviceMetricsHandler::
                    kUserOverrideSystemNotSwitchOutputAudioChromeRestarts;
    }
  } else {
    if (is_input) {
      histogram_name_separated_by_chrome_restarts =
          system_has_switched
              ? AudioDeviceMetricsHandler::
                    kUserOverrideSystemSwitchInputAudioNonChromeRestarts
              : AudioDeviceMetricsHandler::
                    kUserOverrideSystemNotSwitchInputAudioNonChromeRestarts;
    } else {
      histogram_name_separated_by_chrome_restarts =
          system_has_switched
              ? AudioDeviceMetricsHandler::
                    kUserOverrideSystemSwitchOutputAudioNonChromeRestarts
              : AudioDeviceMetricsHandler::
                    kUserOverrideSystemNotSwitchOutputAudioNonChromeRestarts;
    }
  }

  histogram_tester.ExpectTimeBucketCount(
      histogram_name_separated_by_chrome_restarts,
      base::Minutes(delta_in_minute) / base::Minutes(1).InMilliseconds(),
      /*expected_count=*/1);
}

// Test the histogram of consecutive decives change.
void ExpectConsecutiveDeviceChangeHistogramCount(
    const base::HistogramTester& histogram_tester,
    uint16_t expected_consecutive_input_device_changed,
    uint16_t expected_consecutive_output_device_changed,
    uint16_t expected_consecutive_input_device_added,
    uint16_t expected_consecutive_output_device_added) {
  histogram_tester.ExpectTotalCount(
      AudioDeviceMetricsHandler::kConsecutiveInputDevicsChanged,
      expected_consecutive_input_device_changed);
  histogram_tester.ExpectTotalCount(
      AudioDeviceMetricsHandler::kConsecutiveOutputDevicsChanged,
      expected_consecutive_output_device_changed);
  histogram_tester.ExpectTotalCount(
      AudioDeviceMetricsHandler::kConsecutiveInputDevicsAdded,
      expected_consecutive_input_device_added);
  histogram_tester.ExpectTotalCount(
      AudioDeviceMetricsHandler::kConsecutiveOutputDevicsAdded,
      expected_consecutive_output_device_added);
}

class AudioDeviceSelectionTest : public AudioDeviceSelectionTestBase {};

TEST_F(AudioDeviceSelectionTest, PlugUnplugMetricAction) {
  AudioNode input1 = NewInputNode("USB");
  AudioNode input2 = NewInputNode("USB");
  AudioNode output3 = NewOutputNode("USB");
  AudioNode output4 = NewOutputNode("USB");

  {
    base::UserActionTester actions;
    Plug(input1);
    Plug(output3);
    ASSERT_EQ(ActiveInputNodeId(), input1.id);
    ASSERT_EQ(ActiveOutputNodeId(), output3.id);
    Plug(input2);
    Plug(output4);
    ASSERT_EQ(ActiveInputNodeId(), input2.id);
    ASSERT_EQ(ActiveOutputNodeId(), output4.id);
    // Automatic switches should not generate events.
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchInput),
              0);
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchOutput),
              0);
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchInputOverridden),
              0);
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchOutputOverridden),
              0);
  }

  {
    base::UserActionTester actions;
    Select(input1);
    ASSERT_EQ(ActiveInputNodeId(), input1.id);
    ASSERT_EQ(ActiveOutputNodeId(), output4.id);
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchInput),
              1);
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchOutput),
              0);
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchInputOverridden),
              1);
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchOutputOverridden),
              0);
  }

  {
    base::UserActionTester actions;
    Select(output3);
    ASSERT_EQ(ActiveInputNodeId(), input1.id);
    ASSERT_EQ(ActiveOutputNodeId(), output3.id);
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchInput),
              0);
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchOutput),
              1);
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchInputOverridden),
              0);
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchOutputOverridden),
              1);
  }

  {
    base::UserActionTester actions;
    Select(input2);
    Select(output4);
    ASSERT_EQ(ActiveInputNodeId(), input2.id);
    ASSERT_EQ(ActiveOutputNodeId(), output4.id);
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchInput),
              1);
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchOutput),
              1);
    // Switching back and forth should not be counted.
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchInputOverridden),
              0);
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchOutputOverridden),
              0);
  }

  {
    base::UserActionTester actions;
    Unplug(input1);
    Plug(input1);
    ASSERT_EQ(ActiveInputNodeId(), input2.id);
    Select(input1);
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchInput),
              1);
    // Switching after the system decides to do nothing, should be counted.
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchInputOverridden),
              1);
  }

  {
    base::UserActionTester actions;
    Unplug(output3);
    Plug(output3);
    ASSERT_EQ(ActiveOutputNodeId(), output4.id);
    Select(output3);
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchOutput),
              1);
    // Switching after the system decides to do nothing, should be counted.
    EXPECT_EQ(actions.GetActionCount(
                  AudioDeviceMetricsHandler::kUserActionSwitchOutputOverridden),
              1);
  }
}

TEST_F(AudioDeviceSelectionTest, PlugUnplugHistogramMetrics) {
  // Time delta constant for testing the user override system decision.
  constexpr uint16_t kTimeDeltaInMinuteA = 2;
  constexpr uint16_t kTimeDeltaInMinuteB = 30;
  constexpr uint16_t kTimeDeltaInMinuteC = 200;

  AudioNode input_internal = NewInputNode("INTERNAL_MIC");
  AudioNode input_USB = NewInputNode("USB");
  AudioNode input_bluetooth_nb = NewInputNode("BLUETOOTH_NB_MIC");
  AudioNode output_internal = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode output_USB = NewOutputNode("USB");

  uint16_t expected_system_switch_input_count = 0;
  uint16_t expected_system_not_switch_input_count = 0;
  uint16_t expected_system_switch_output_count = 0;
  uint16_t expected_system_not_switch_output_count = 0;

  uint16_t expected_user_override_system_switch_input_count = 0;
  uint16_t expected_user_override_system_not_switch_input_count = 0;
  uint16_t expected_user_override_system_switch_output_count = 0;
  uint16_t expected_user_override_system_not_switch_output_count = 0;

  uint16_t expected_user_override_system_switch_input_count_in_limited_time = 0;
  uint16_t
      expected_user_override_system_not_switch_input_count_in_limited_time = 0;
  uint16_t expected_user_override_system_switch_output_count_in_limited_time =
      0;
  uint16_t
      expected_user_override_system_not_switch_output_count_in_limited_time = 0;

  uint16_t num_of_input_devices = 0;
  uint16_t num_of_output_devices = 0;

  uint16_t expected_consecutive_input_device_changed = 0;
  uint16_t expected_consecutive_output_device_changed = 0;
  uint16_t expected_consecutive_input_device_added = 0;
  uint16_t expected_consecutive_output_device_added = 0;

  // Plug in internal mic and speaker.
  // Do not record if there is no alternative device available.
  Plug(input_internal);
  Plug(output_internal);
  num_of_input_devices++;
  num_of_output_devices++;

  ExpectSystemDecisionHistogramCount(
      histogram_tester(), expected_system_switch_input_count,
      expected_system_not_switch_input_count,
      expected_system_switch_output_count,
      expected_system_not_switch_output_count, /*is_chrome_restarts=*/false);

  ExpectConsecutiveDeviceChangeHistogramCount(
      histogram_tester(), expected_consecutive_input_device_changed,
      expected_consecutive_output_device_changed,
      expected_consecutive_input_device_added,
      expected_consecutive_output_device_added);

  // Plug in USB devices with higher priority than current active one.
  // Expect to record system has switched both input and output.
  Plug(input_USB);
  Plug(output_USB);
  num_of_input_devices++;
  num_of_output_devices++;

  ExpectSystemDecisionHistogramCount(
      histogram_tester(), ++expected_system_switch_input_count,
      expected_system_not_switch_input_count,
      ++expected_system_switch_output_count,
      expected_system_not_switch_output_count, /*is_chrome_restarts=*/false);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchInputAudioDeviceCount,
      num_of_input_devices, /*bucket_count=*/1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchOutputAudioDeviceCount,
      num_of_output_devices, /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemSwitchInputAudioDeviceCountNonChromeRestarts,
      num_of_input_devices, /*bucket_count=*/1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemSwitchOutputAudioDeviceCountNonChromeRestarts,
      num_of_output_devices, /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchInputAudioDeviceSet,
      EncodeAudioDeviceSet(
          {AudioDevice(input_internal), AudioDevice(input_USB)}),
      /*bucket_count=*/1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchOutputAudioDeviceSet,
      EncodeAudioDeviceSet(
          {AudioDevice(output_internal), AudioDevice(output_USB)}),
      /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemSwitchInputAudioDeviceSetNonChromeRestarts,
      EncodeAudioDeviceSet(
          {AudioDevice(output_internal), AudioDevice(output_USB)}),
      /*bucket_count=*/1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemSwitchOutputAudioDeviceSetNonChromeRestarts,
      EncodeAudioDeviceSet(
          {AudioDevice(output_internal), AudioDevice(output_USB)}),
      /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchInputBeforeAndAfterAudioDeviceSet,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{AudioDevice(output_internal)},
          /*device_set_after=*/{AudioDevice(output_internal),
                                AudioDevice(output_USB)}),
      /*bucket_count=*/1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemSwitchOutputBeforeAndAfterAudioDeviceSet,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{AudioDevice(output_internal)},
          /*device_set_after=*/{AudioDevice(output_internal),
                                AudioDevice(output_USB)}),
      /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemSwitchInputBeforeAndAfterAudioDeviceSetNonChromeRestarts,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{AudioDevice(output_internal)},
          /*device_set_after=*/{AudioDevice(output_internal),
                                AudioDevice(output_USB)}),
      /*bucket_count=*/1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemSwitchOutputBeforeAndAfterAudioDeviceSetNonChromeRestarts,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{AudioDevice(output_internal)},
          /*device_set_after=*/{AudioDevice(output_internal),
                                AudioDevice(output_USB)}),
      /*bucket_count=*/1);

  ExpectConsecutiveDeviceChangeHistogramCount(
      histogram_tester(), ++expected_consecutive_input_device_changed,
      ++expected_consecutive_output_device_changed,
      ++expected_consecutive_input_device_added,
      ++expected_consecutive_output_device_added);

  // User switches input device immediately.
  // Expect to record user overrides system decision of switching input
  // device.
  Select(input_internal);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kUserOverrideSystemSwitchInputBeforeAndAfterAudioDeviceSet,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{AudioDevice(input_internal)},
          /*device_set_after=*/{AudioDevice(input_internal),
                                AudioDevice(input_USB)}),
      /*bucket_count=*/1);

  ExpectUserOverrideSystemDecisionHistogramCount(
      histogram_tester(), ++expected_user_override_system_switch_input_count,
      expected_user_override_system_not_switch_input_count,
      expected_user_override_system_switch_output_count,
      expected_user_override_system_not_switch_output_count,
      /*is_chrome_restarts=*/false);
  ExpectUserOverrideSystemDecisionHistogramCountInLimitedTime(
      histogram_tester(),
      ++expected_user_override_system_switch_input_count_in_limited_time,
      expected_user_override_system_not_switch_input_count_in_limited_time,
      expected_user_override_system_switch_output_count_in_limited_time,
      expected_user_override_system_not_switch_output_count_in_limited_time,
      /*is_chrome_restarts=*/false);
  ExpectUserOverrideSystemDecisionTimeDelta(
      histogram_tester(), /*is_input=*/true, /*system_has_switched=*/true,
      /*delta_in_minute=*/0, /*is_chrome_restarts=*/false);

  // User switches output device after some time.
  // Expect to record user overrides system decision of switching output
  // device.
  FastForwardBy(base::Minutes(kTimeDeltaInMinuteA));
  Select(output_internal);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kUserOverrideSystemSwitchOutputBeforeAndAfterAudioDeviceSet,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{AudioDevice(output_internal)},
          /*device_set_after=*/{AudioDevice(output_internal),
                                AudioDevice(output_USB)}),
      /*bucket_count=*/1);

  ExpectUserOverrideSystemDecisionHistogramCount(
      histogram_tester(), expected_user_override_system_switch_input_count,
      expected_user_override_system_not_switch_input_count,
      ++expected_user_override_system_switch_output_count,
      expected_user_override_system_not_switch_output_count,
      /*is_chrome_restarts=*/false);
  ExpectUserOverrideSystemDecisionHistogramCountInLimitedTime(
      histogram_tester(),
      expected_user_override_system_switch_input_count_in_limited_time,
      expected_user_override_system_not_switch_input_count_in_limited_time,
      ++expected_user_override_system_switch_output_count_in_limited_time,
      expected_user_override_system_not_switch_output_count_in_limited_time,
      /*is_chrome_restarts=*/false);
  ExpectUserOverrideSystemDecisionTimeDelta(
      histogram_tester(), /*is_input=*/false, /*system_has_switched=*/true,
      /*delta_in_minute=*/kTimeDeltaInMinuteA, /*is_chrome_restarts=*/false);

  // User switches output device again.
  // Do not record since user has just switched output device previously and
  // there is no system switch or not switch decision in between.
  Select(output_USB);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kUserOverrideSystemSwitchOutputBeforeAndAfterAudioDeviceSet,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{AudioDevice(output_internal)},
          /*device_set_after=*/{AudioDevice(output_internal),
                                AudioDevice(output_USB)}),
      /*bucket_count=*/1);

  ExpectUserOverrideSystemDecisionHistogramCount(
      histogram_tester(), expected_user_override_system_switch_input_count,
      expected_user_override_system_not_switch_input_count,
      expected_user_override_system_switch_output_count,
      expected_user_override_system_not_switch_output_count,
      /*is_chrome_restarts=*/false);
  ExpectUserOverrideSystemDecisionHistogramCountInLimitedTime(
      histogram_tester(),
      expected_user_override_system_switch_input_count_in_limited_time,
      expected_user_override_system_not_switch_input_count_in_limited_time,
      expected_user_override_system_switch_output_count_in_limited_time,
      expected_user_override_system_not_switch_output_count_in_limited_time,
      /*is_chrome_restarts=*/false);

  // Plug in a bluetooth nb mic with lower priority than current active one.
  // Expect to record system does not switch input.
  Plug(input_bluetooth_nb);
  num_of_input_devices++;

  ExpectSystemDecisionHistogramCount(
      histogram_tester(), expected_system_switch_input_count,
      ++expected_system_not_switch_input_count,
      expected_system_switch_output_count,
      expected_system_not_switch_output_count, /*is_chrome_restarts=*/false);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemNotSwitchInputAudioDeviceCount,
      num_of_input_devices, /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemNotSwitchInputAudioDeviceCountNonChromeRestarts,
      num_of_input_devices, /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemNotSwitchInputAudioDeviceSet,
      EncodeAudioDeviceSet({AudioDevice(input_internal), AudioDevice(input_USB),
                            AudioDevice(input_bluetooth_nb)}),
      /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemNotSwitchInputAudioDeviceSetNonChromeRestarts,
      EncodeAudioDeviceSet({AudioDevice(input_internal), AudioDevice(input_USB),
                            AudioDevice(input_bluetooth_nb)}),
      /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemNotSwitchInputBeforeAndAfterAudioDeviceSet,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{AudioDevice(output_internal),
                                 AudioDevice(output_USB)},
          /*device_set_after=*/{AudioDevice(output_internal),
                                AudioDevice(output_USB),
                                AudioDevice(input_bluetooth_nb)}),
      /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemNotSwitchInputBeforeAndAfterAudioDeviceSetNonChromeRestarts,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{AudioDevice(output_internal),
                                 AudioDevice(output_USB)},
          /*device_set_after=*/{AudioDevice(output_internal),
                                AudioDevice(output_USB),
                                AudioDevice(input_bluetooth_nb)}),
      /*bucket_count=*/1);

  ExpectConsecutiveDeviceChangeHistogramCount(
      histogram_tester(), ++expected_consecutive_input_device_changed,
      expected_consecutive_output_device_changed,
      ++expected_consecutive_input_device_added,
      expected_consecutive_output_device_added);

  // User switches to USB input after some time.
  // Expect to record user overrides system decision of not switching input
  // device.
  FastForwardBy(base::Minutes(kTimeDeltaInMinuteB));
  Select(input_USB);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kUserOverrideSystemNotSwitchInputBeforeAndAfterAudioDeviceSet,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{AudioDevice(input_internal),
                                 AudioDevice(input_USB)},
          /*device_set_after=*/{AudioDevice(input_internal),
                                AudioDevice(input_USB),
                                AudioDevice(input_bluetooth_nb)}),
      /*bucket_count=*/1);

  ExpectUserOverrideSystemDecisionHistogramCount(
      histogram_tester(), expected_user_override_system_switch_input_count,
      ++expected_user_override_system_not_switch_input_count,
      expected_user_override_system_switch_output_count,
      expected_user_override_system_not_switch_output_count,
      /*is_chrome_restarts=*/false);
  ExpectUserOverrideSystemDecisionHistogramCountInLimitedTime(
      histogram_tester(),
      expected_user_override_system_switch_input_count_in_limited_time,
      ++expected_user_override_system_not_switch_input_count_in_limited_time,
      expected_user_override_system_switch_output_count_in_limited_time,
      expected_user_override_system_not_switch_output_count_in_limited_time,
      /*is_chrome_restarts=*/false);
  ExpectUserOverrideSystemDecisionTimeDelta(
      histogram_tester(), /*is_input=*/true, /*system_has_switched=*/false,
      /*delta_in_minute=*/kTimeDeltaInMinuteB, /*is_chrome_restarts=*/false);

  // User unplugs current active device USB input.
  // Expect to record system has switched input.
  Unplug(input_USB);
  num_of_input_devices--;

  ExpectSystemDecisionHistogramCount(
      histogram_tester(), ++expected_system_switch_input_count,
      expected_system_not_switch_input_count,
      expected_system_switch_output_count,
      expected_system_not_switch_output_count, /*is_chrome_restarts=*/false);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchInputAudioDeviceCount,
      num_of_input_devices, /*bucket_count=*/2);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemSwitchInputAudioDeviceCountNonChromeRestarts,
      num_of_input_devices, /*bucket_count=*/2);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchInputAudioDeviceSet,
      EncodeAudioDeviceSet(
          {AudioDevice(input_internal), AudioDevice(input_bluetooth_nb)}),
      /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemSwitchInputAudioDeviceSetNonChromeRestarts,
      EncodeAudioDeviceSet(
          {AudioDevice(input_internal), AudioDevice(input_bluetooth_nb)}),
      /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchInputBeforeAndAfterAudioDeviceSet,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{AudioDevice(input_internal),
                                 AudioDevice(input_bluetooth_nb),
                                 AudioDevice(input_USB)},
          /*device_set_after=*/{AudioDevice(input_internal),
                                AudioDevice(input_bluetooth_nb)}),
      /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemSwitchInputBeforeAndAfterAudioDeviceSetNonChromeRestarts,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{AudioDevice(input_internal),
                                 AudioDevice(input_bluetooth_nb),
                                 AudioDevice(input_USB)},
          /*device_set_after=*/{AudioDevice(input_internal),
                                AudioDevice(input_bluetooth_nb)}),
      /*bucket_count=*/1);

  ExpectConsecutiveDeviceChangeHistogramCount(
      histogram_tester(), ++expected_consecutive_input_device_changed,
      expected_consecutive_output_device_changed,
      expected_consecutive_input_device_added,
      expected_consecutive_output_device_added);

  // User switches to input_bluetooth_nb after some time.
  // Expect to record user overrides system decision of switching input device.
  FastForwardBy(base::Minutes(kTimeDeltaInMinuteC));
  Select(input_bluetooth_nb);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kUserOverrideSystemSwitchInputBeforeAndAfterAudioDeviceSet,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{AudioDevice(input_internal),
                                 AudioDevice(input_bluetooth_nb),
                                 AudioDevice(input_USB)},
          /*device_set_after=*/{AudioDevice(input_internal),
                                AudioDevice(input_bluetooth_nb)}),
      /*bucket_count=*/1);

  ExpectUserOverrideSystemDecisionHistogramCount(
      histogram_tester(), ++expected_user_override_system_switch_input_count,
      expected_user_override_system_not_switch_input_count,
      expected_user_override_system_switch_output_count,
      expected_user_override_system_not_switch_output_count,
      /*is_chrome_restarts=*/false);
  ExpectUserOverrideSystemDecisionHistogramCountInLimitedTime(
      histogram_tester(),
      expected_user_override_system_switch_input_count_in_limited_time,
      expected_user_override_system_not_switch_input_count_in_limited_time,
      expected_user_override_system_switch_output_count_in_limited_time,
      expected_user_override_system_not_switch_output_count_in_limited_time,
      /*is_chrome_restarts=*/false);
  ExpectUserOverrideSystemDecisionTimeDelta(
      histogram_tester(), /*is_input=*/true, /*system_has_switched=*/true,
      /*delta_in_minute=*/kTimeDeltaInMinuteC, /*is_chrome_restarts=*/false);

  // User unplugs active device input_bluetooth_nb.
  // Do not record if there is no alternative device available.
  Unplug(input_bluetooth_nb);

  ExpectSystemDecisionHistogramCount(
      histogram_tester(), expected_system_switch_input_count,
      expected_system_not_switch_input_count,
      expected_system_switch_output_count,
      expected_system_not_switch_output_count, /*is_chrome_restarts=*/false);

  ExpectConsecutiveDeviceChangeHistogramCount(
      histogram_tester(), ++expected_consecutive_input_device_changed,
      expected_consecutive_output_device_changed,
      expected_consecutive_input_device_added,
      expected_consecutive_output_device_added);
}

TEST_F(AudioDeviceSelectionTest, SystemBootsHistogramMetrics) {
  AudioNode input_internal = NewInputNode("INTERNAL_MIC");
  AudioNode input_USB = NewInputNode("USB");
  AudioNode output_internal = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode output_USB = NewOutputNode("USB");

  uint16_t expected_system_switch_input_count = 0;
  uint16_t expected_system_not_switch_input_count = 0;
  uint16_t expected_system_switch_output_count = 0;
  uint16_t expected_system_not_switch_output_count = 0;

  uint16_t num_of_input_devices = 0;
  uint16_t num_of_output_devices = 0;

  uint16_t expected_user_override_system_switch_input_count = 0;
  uint16_t expected_user_override_system_not_switch_input_count = 0;
  uint16_t expected_user_override_system_switch_output_count = 0;
  uint16_t expected_user_override_system_not_switch_output_count = 0;

  uint16_t expected_user_override_system_switch_input_count_in_limited_time = 0;
  uint16_t
      expected_user_override_system_not_switch_input_count_in_limited_time = 0;
  uint16_t expected_user_override_system_switch_output_count_in_limited_time =
      0;
  uint16_t
      expected_user_override_system_not_switch_output_count_in_limited_time = 0;

  // System boots with multiple audio devices.
  // Expect to record system has switched both input and output.
  SystemBootsWith({input_internal, input_USB, output_internal, output_USB});
  num_of_input_devices += 2;
  num_of_output_devices += 2;

  ExpectSystemDecisionHistogramCount(
      histogram_tester(), ++expected_system_switch_input_count,
      expected_system_not_switch_input_count,
      ++expected_system_switch_output_count,
      expected_system_not_switch_output_count, /*is_chrome_restarts=*/true);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchInputAudioDeviceCount,
      num_of_input_devices, /*bucket_count=*/1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchOutputAudioDeviceCount,
      num_of_output_devices, /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemSwitchInputAudioDeviceCountChromeRestarts,
      num_of_input_devices, /*bucket_count=*/1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemSwitchOutputAudioDeviceCountChromeRestarts,
      num_of_output_devices, /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchInputAudioDeviceSet,
      EncodeAudioDeviceSet(
          {AudioDevice(input_internal), AudioDevice(input_USB)}),
      /*bucket_count=*/1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchOutputAudioDeviceSet,
      EncodeAudioDeviceSet(
          {AudioDevice(input_internal), AudioDevice(input_USB)}),
      /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchInputAudioDeviceSetChromeRestarts,
      EncodeAudioDeviceSet(
          {AudioDevice(output_internal), AudioDevice(output_USB)}),
      /*bucket_count=*/1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemSwitchOutputAudioDeviceSetChromeRestarts,
      EncodeAudioDeviceSet(
          {AudioDevice(output_internal), AudioDevice(output_USB)}),
      /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kSystemSwitchInputBeforeAndAfterAudioDeviceSet,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{},
          /*device_set_after=*/{AudioDevice(input_internal),
                                AudioDevice(input_USB)}),
      /*bucket_count=*/1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemSwitchOutputBeforeAndAfterAudioDeviceSet,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{},
          /*device_set_after=*/{AudioDevice(input_internal),
                                AudioDevice(input_USB)}),
      /*bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemSwitchInputBeforeAndAfterAudioDeviceSetChromeRestarts,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{},
          /*device_set_after=*/{AudioDevice(input_internal),
                                AudioDevice(input_USB)}),
      /*bucket_count=*/1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kSystemSwitchOutputBeforeAndAfterAudioDeviceSetChromeRestarts,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{},
          /*device_set_after=*/{AudioDevice(input_internal),
                                AudioDevice(input_USB)}),
      /*bucket_count=*/1);

  // Mock user switching to internal mic.
  Select(input_internal);

  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::
          kUserOverrideSystemSwitchInputBeforeAndAfterAudioDeviceSet,
      EncodeBeforeAndAfterAudioDeviceSets(
          /*device_set_before=*/{},
          /*device_set_after=*/{AudioDevice(input_internal),
                                AudioDevice(input_USB)}),
      /*bucket_count=*/1);

  ExpectUserOverrideSystemDecisionHistogramCount(
      histogram_tester(), ++expected_user_override_system_switch_input_count,
      expected_user_override_system_not_switch_input_count,
      expected_user_override_system_switch_output_count,
      expected_user_override_system_not_switch_output_count,
      /*is_chrome_restarts=*/true);
  ExpectUserOverrideSystemDecisionHistogramCountInLimitedTime(
      histogram_tester(),
      ++expected_user_override_system_switch_input_count_in_limited_time,
      expected_user_override_system_not_switch_input_count_in_limited_time,
      expected_user_override_system_switch_output_count_in_limited_time,
      expected_user_override_system_not_switch_output_count_in_limited_time,
      /*is_chrome_restarts=*/true);

  ExpectUserOverrideSystemDecisionTimeDelta(
      histogram_tester(), /*is_input=*/true, /*system_has_switched=*/true,
      /*delta_in_minute=*/0, /*is_chrome_restarts=*/true);
}

TEST_F(AudioDeviceSelectionTest, DevicePrefEviction) {
  std::vector<AudioNode> nodes;
  for (int i = 0; i < 101; i++) {
    nodes.push_back(NewInputNode("USB"));
  }

  Plug(nodes[0]);
  for (int i = 1; i < 101; i++) {
    FastForwardBy(base::Seconds(1));
    Plug(nodes[i]);
    ASSERT_EQ(ActiveInputNodeId(), nodes[i].id) << " i = " << i;
    ASSERT_NE(audio_pref_handler_->GetUserPriority(AudioDevice(nodes[i])),
              kUserPriorityNone)
        << " i = " << i;

    FastForwardBy(base::Seconds(1));
    Unplug(nodes[i]);
  }

  // We store at most 100 devices in prefs.
  EXPECT_NE(audio_pref_handler_->GetUserPriority(AudioDevice(nodes[0])),
            kUserPriorityNone)
      << "nodes[0] should be kept because it is connected";
  EXPECT_EQ(audio_pref_handler_->GetUserPriority(AudioDevice(nodes[1])),
            kUserPriorityNone)
      << "nodes[1] should be evicted because it is unplugged and the least "
         "recently seen";
  for (int i = 2; i < 101; i++) {
    EXPECT_NE(audio_pref_handler_->GetUserPriority(AudioDevice(nodes[i])),
              kUserPriorityNone)
        << "nodes[" << i
        << "] should be kept because it is within the recent 100 seen devices";
  }
}

}  // namespace

}  // namespace ash
