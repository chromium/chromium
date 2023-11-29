// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "chromeos/ash/components/audio/audio_device_selection_test_base.h"

#include "ash/constants/ash_features.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
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
    uint16_t expected_system_not_switch_output_count) {
  histogram_tester.ExpectBucketCount(CrasAudioHandler::kSystemSwitchInputAudio,
                                     kSystemSwitchSample,
                                     expected_system_switch_input_count);
  histogram_tester.ExpectBucketCount(CrasAudioHandler::kSystemSwitchInputAudio,
                                     kSystemNotSwitchSample,
                                     expected_system_not_switch_input_count);
  histogram_tester.ExpectBucketCount(CrasAudioHandler::kSystemSwitchOutputAudio,
                                     kSystemSwitchSample,
                                     expected_system_switch_output_count);
  histogram_tester.ExpectBucketCount(CrasAudioHandler::kSystemSwitchOutputAudio,
                                     kSystemNotSwitchSample,
                                     expected_system_not_switch_output_count);
}

// Test the histogram of user override after system switch or not switch.
void ExpectUserOverrideSystemDecisionHistogramCount(
    const base::HistogramTester& histogram_tester,
    uint16_t expected_user_override_system_switch_input_count,
    uint16_t expected_user_override_system_not_switch_input_count,
    uint16_t expected_user_override_system_switch_output_count,
    uint16_t expected_user_override_system_not_switch_output_count) {
  histogram_tester.ExpectTotalCount(
      CrasAudioHandler::kUserOverrideSystemSwitchInputAudio,
      expected_user_override_system_switch_input_count);
  histogram_tester.ExpectTotalCount(
      CrasAudioHandler::kUserOverrideSystemNotSwitchInputAudio,
      expected_user_override_system_not_switch_input_count);
  histogram_tester.ExpectTotalCount(
      CrasAudioHandler::kUserOverrideSystemSwitchOutputAudio,
      expected_user_override_system_switch_output_count);
  histogram_tester.ExpectTotalCount(
      CrasAudioHandler::kUserOverrideSystemNotSwitchOutputAudio,
      expected_user_override_system_not_switch_output_count);
}

// Test the time delta histogram of user override after system switch or not
// switch.
void ExpectUserOverrideSystemDecisionTimeDelta(
    const base::HistogramTester& histogram_tester,
    bool is_input,
    bool system_has_switched,
    uint16_t delta_in_minute) {
  std::string histogram_name;
  if (is_input) {
    histogram_name =
        system_has_switched
            ? CrasAudioHandler::kUserOverrideSystemSwitchInputAudio
            : CrasAudioHandler::kUserOverrideSystemNotSwitchInputAudio;
  } else {
    histogram_name =
        system_has_switched
            ? CrasAudioHandler::kUserOverrideSystemSwitchOutputAudio
            : CrasAudioHandler::kUserOverrideSystemNotSwitchOutputAudio;
  }

  histogram_tester.ExpectTimeBucketCount(
      histogram_name,
      base::Minutes(delta_in_minute) / base::Minutes(1).InMilliseconds(),
      /*expected_count=*/1);
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
    EXPECT_EQ(actions.GetActionCount(CrasAudioHandler::kUserActionSwitchInput),
              0);
    EXPECT_EQ(actions.GetActionCount(CrasAudioHandler::kUserActionSwitchOutput),
              0);
    EXPECT_EQ(actions.GetActionCount(
                  CrasAudioHandler::kUserActionSwitchInputOverridden),
              0);
    EXPECT_EQ(actions.GetActionCount(
                  CrasAudioHandler::kUserActionSwitchOutputOverridden),
              0);
  }

  {
    base::UserActionTester actions;
    Select(input1);
    ASSERT_EQ(ActiveInputNodeId(), input1.id);
    ASSERT_EQ(ActiveOutputNodeId(), output4.id);
    EXPECT_EQ(actions.GetActionCount(CrasAudioHandler::kUserActionSwitchInput),
              1);
    EXPECT_EQ(actions.GetActionCount(CrasAudioHandler::kUserActionSwitchOutput),
              0);
    EXPECT_EQ(actions.GetActionCount(
                  CrasAudioHandler::kUserActionSwitchInputOverridden),
              1);
    EXPECT_EQ(actions.GetActionCount(
                  CrasAudioHandler::kUserActionSwitchOutputOverridden),
              0);
  }

  {
    base::UserActionTester actions;
    Select(output3);
    ASSERT_EQ(ActiveInputNodeId(), input1.id);
    ASSERT_EQ(ActiveOutputNodeId(), output3.id);
    EXPECT_EQ(actions.GetActionCount(CrasAudioHandler::kUserActionSwitchInput),
              0);
    EXPECT_EQ(actions.GetActionCount(CrasAudioHandler::kUserActionSwitchOutput),
              1);
    EXPECT_EQ(actions.GetActionCount(
                  CrasAudioHandler::kUserActionSwitchInputOverridden),
              0);
    EXPECT_EQ(actions.GetActionCount(
                  CrasAudioHandler::kUserActionSwitchOutputOverridden),
              1);
  }

  {
    base::UserActionTester actions;
    Select(input2);
    Select(output4);
    ASSERT_EQ(ActiveInputNodeId(), input2.id);
    ASSERT_EQ(ActiveOutputNodeId(), output4.id);
    EXPECT_EQ(actions.GetActionCount(CrasAudioHandler::kUserActionSwitchInput),
              1);
    EXPECT_EQ(actions.GetActionCount(CrasAudioHandler::kUserActionSwitchOutput),
              1);
    // Switching back and forth should not be counted.
    EXPECT_EQ(actions.GetActionCount(
                  CrasAudioHandler::kUserActionSwitchInputOverridden),
              0);
    EXPECT_EQ(actions.GetActionCount(
                  CrasAudioHandler::kUserActionSwitchOutputOverridden),
              0);
  }

  {
    base::UserActionTester actions;
    Unplug(input1);
    Plug(input1);
    ASSERT_EQ(ActiveInputNodeId(), input2.id);
    Select(input1);
    EXPECT_EQ(actions.GetActionCount(CrasAudioHandler::kUserActionSwitchInput),
              1);
    // Switching after the system decides to do nothing, should be counted.
    EXPECT_EQ(actions.GetActionCount(
                  CrasAudioHandler::kUserActionSwitchInputOverridden),
              1);
  }

  {
    base::UserActionTester actions;
    Unplug(output3);
    Plug(output3);
    ASSERT_EQ(ActiveOutputNodeId(), output4.id);
    Select(output3);
    EXPECT_EQ(actions.GetActionCount(CrasAudioHandler::kUserActionSwitchOutput),
              1);
    // Switching after the system decides to do nothing, should be counted.
    EXPECT_EQ(actions.GetActionCount(
                  CrasAudioHandler::kUserActionSwitchOutputOverridden),
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

  // Plug in internal mic and speaker.
  // Do not record if there is no alternative device available.
  Plug(input_internal);
  Plug(output_internal);

  ExpectSystemDecisionHistogramCount(histogram_tester(),
                                     expected_system_switch_input_count,
                                     expected_system_not_switch_input_count,
                                     expected_system_switch_output_count,
                                     expected_system_not_switch_output_count);

  // Plug in USB devices with higher priority than current active one.
  // Expect to record system has switched both input and output.
  Plug(input_USB);
  Plug(output_USB);

  ExpectSystemDecisionHistogramCount(histogram_tester(),
                                     ++expected_system_switch_input_count,
                                     expected_system_not_switch_input_count,
                                     ++expected_system_switch_output_count,
                                     expected_system_not_switch_output_count);

  // User switches input device immediately.
  // Expect to record user overrides system decision of switching input
  // device.
  Select(input_internal);

  ExpectUserOverrideSystemDecisionHistogramCount(
      histogram_tester(), ++expected_user_override_system_switch_input_count,
      expected_user_override_system_not_switch_input_count,
      expected_user_override_system_switch_output_count,
      expected_user_override_system_not_switch_output_count);
  ExpectUserOverrideSystemDecisionTimeDelta(
      histogram_tester(), /*is_input=*/true, /*system_has_switched=*/true,
      /*delta_in_minute=*/0);

  // User switches output device after some time.
  // Expect to record user overrides system decision of switching output
  // device.
  FastForwardBy(base::Minutes(kTimeDeltaInMinuteA));
  Select(output_internal);

  ExpectUserOverrideSystemDecisionHistogramCount(
      histogram_tester(), expected_user_override_system_switch_input_count,
      expected_user_override_system_not_switch_input_count,
      ++expected_user_override_system_switch_output_count,
      expected_user_override_system_not_switch_output_count);
  ExpectUserOverrideSystemDecisionTimeDelta(
      histogram_tester(), /*is_input=*/false, /*system_has_switched=*/true,
      /*delta_in_minute=*/kTimeDeltaInMinuteA);

  // User switches output device again.
  // Do not record since user has just switched output device previously and
  // there is no system switch or not switch decision in between.
  Select(output_USB);

  ExpectUserOverrideSystemDecisionHistogramCount(
      histogram_tester(), expected_user_override_system_switch_input_count,
      expected_user_override_system_not_switch_input_count,
      expected_user_override_system_switch_output_count,
      expected_user_override_system_not_switch_output_count);

  // Plug in a bluetooth nb mic with lower priority than current active one.
  // Expect to record system does not switch input.
  Plug(input_bluetooth_nb);

  ExpectSystemDecisionHistogramCount(histogram_tester(),
                                     expected_system_switch_input_count,
                                     ++expected_system_not_switch_input_count,
                                     expected_system_switch_output_count,
                                     expected_system_not_switch_output_count);

  // User switches to USB input after some time.
  // Expect to record user overrides system decision of not switching input
  // device.
  FastForwardBy(base::Minutes(kTimeDeltaInMinuteB));
  Select(input_USB);

  ExpectUserOverrideSystemDecisionHistogramCount(
      histogram_tester(), expected_user_override_system_switch_input_count,
      ++expected_user_override_system_not_switch_input_count,
      expected_user_override_system_switch_output_count,
      expected_user_override_system_not_switch_output_count);
  ExpectUserOverrideSystemDecisionTimeDelta(
      histogram_tester(), /*is_input=*/true, /*system_has_switched=*/false,
      /*delta_in_minute=*/kTimeDeltaInMinuteB);

  // User unplugs current active device USB input.
  // Expect to record system has switched input.
  Unplug(input_USB);

  ExpectSystemDecisionHistogramCount(histogram_tester(),
                                     ++expected_system_switch_input_count,
                                     expected_system_not_switch_input_count,
                                     expected_system_switch_output_count,
                                     expected_system_not_switch_output_count);

  // User switches to input_bluetooth_nb after some time.
  // Expect to record user overrides system decision of switching input device.
  FastForwardBy(base::Minutes(kTimeDeltaInMinuteC));
  Select(input_bluetooth_nb);

  ExpectUserOverrideSystemDecisionHistogramCount(
      histogram_tester(), ++expected_user_override_system_switch_input_count,
      expected_user_override_system_not_switch_input_count,
      expected_user_override_system_switch_output_count,
      expected_user_override_system_not_switch_output_count);
  ExpectUserOverrideSystemDecisionTimeDelta(
      histogram_tester(), /*is_input=*/true, /*system_has_switched=*/true,
      /*delta_in_minute=*/kTimeDeltaInMinuteC);

  // User unplugs active device input_bluetooth_nb.
  // Do not record if there is no alternative device available.
  Unplug(input_bluetooth_nb);

  ExpectSystemDecisionHistogramCount(histogram_tester(),
                                     expected_system_switch_input_count,
                                     expected_system_not_switch_input_count,
                                     expected_system_switch_output_count,
                                     expected_system_not_switch_output_count);
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

  // System boots with multiple audio devices.
  // Expect to record system has switched both input and output.
  SystemBootsWith({input_internal, input_USB, output_internal, output_USB});

  ExpectSystemDecisionHistogramCount(histogram_tester(),
                                     ++expected_system_switch_input_count,
                                     expected_system_not_switch_input_count,
                                     ++expected_system_switch_output_count,
                                     expected_system_not_switch_output_count);
}

TEST_F(AudioDeviceSelectionTest, DevicePrefEviction) {
  base::test::ScopedFeatureList features(
      ash::features::kRobustAudioDeviceSelectLogic);

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
