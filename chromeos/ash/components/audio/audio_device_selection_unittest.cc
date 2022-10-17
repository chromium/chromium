// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device_selection_test_base.h"

#include "ash/constants/ash_features.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

const char* kInputSwitched = "StatusArea_Audio_SwitchInputDevice";
const char* kOutputSwitched = "StatusArea_Audio_SwitchOutputDevice";
const char* kInputOverridden = "StatusArea_Audio_AutoInputSelectionOverridden";
const char* kOutputOverridden =
    "StatusArea_Audio_AutoOutputSelectionOverridden";

namespace ash {
namespace {

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
    EXPECT_EQ(actions.GetActionCount(kInputSwitched), 0);
    EXPECT_EQ(actions.GetActionCount(kOutputSwitched), 0);
    EXPECT_EQ(actions.GetActionCount(kInputOverridden), 0);
    EXPECT_EQ(actions.GetActionCount(kOutputOverridden), 0);
  }

  {
    base::UserActionTester actions;
    Select(input1);
    ASSERT_EQ(ActiveInputNodeId(), input1.id);
    ASSERT_EQ(ActiveOutputNodeId(), output4.id);
    EXPECT_EQ(actions.GetActionCount(kInputSwitched), 1);
    EXPECT_EQ(actions.GetActionCount(kOutputSwitched), 0);
    EXPECT_EQ(actions.GetActionCount(kInputOverridden), 1);
    EXPECT_EQ(actions.GetActionCount(kOutputOverridden), 0);
  }

  {
    base::UserActionTester actions;
    Select(output3);
    ASSERT_EQ(ActiveInputNodeId(), input1.id);
    ASSERT_EQ(ActiveOutputNodeId(), output3.id);
    EXPECT_EQ(actions.GetActionCount(kInputSwitched), 0);
    EXPECT_EQ(actions.GetActionCount(kOutputSwitched), 1);
    EXPECT_EQ(actions.GetActionCount(kInputOverridden), 0);
    EXPECT_EQ(actions.GetActionCount(kOutputOverridden), 1);
  }

  {
    base::UserActionTester actions;
    Select(input2);
    Select(output4);
    ASSERT_EQ(ActiveInputNodeId(), input2.id);
    ASSERT_EQ(ActiveOutputNodeId(), output4.id);
    EXPECT_EQ(actions.GetActionCount(kInputSwitched), 1);
    EXPECT_EQ(actions.GetActionCount(kOutputSwitched), 1);
    // Switching back and forth should not be counted.
    EXPECT_EQ(actions.GetActionCount(kInputOverridden), 0);
    EXPECT_EQ(actions.GetActionCount(kOutputOverridden), 0);
  }

  {
    base::UserActionTester actions;
    Unplug(input1);
    Plug(input1);
    ASSERT_EQ(ActiveInputNodeId(), input2.id);
    Select(input1);
    EXPECT_EQ(actions.GetActionCount(kInputSwitched), 1);
    // Switching after the system decides to do nothing, should be counted.
    EXPECT_EQ(actions.GetActionCount(kInputOverridden), 1);
  }

  {
    base::UserActionTester actions;
    Unplug(output3);
    Plug(output3);
    ASSERT_EQ(ActiveOutputNodeId(), output4.id);
    Select(output3);
    EXPECT_EQ(actions.GetActionCount(kOutputSwitched), 1);
    // Switching after the system decides to do nothing, should be counted.
    EXPECT_EQ(actions.GetActionCount(kOutputOverridden), 1);
  }
}

TEST_F(AudioDeviceSelectionTest, DevicePrefEviction) {
  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        static int i = 0;
        i++;
        return base::Time::FromDoubleT(i);
      },
      nullptr, nullptr);

  base::test::ScopedFeatureList features(
      ash::features::kRobustAudioDeviceSelectLogic);

  std::vector<AudioNode> nodes;
  for (int i = 0; i < 101; i++) {
    nodes.push_back(NewInputNode("USB"));
  }

  Plug(nodes[0]);
  for (int i = 1; i < 101; i++) {
    Plug(nodes[i]);
    ASSERT_EQ(ActiveInputNodeId(), nodes[i].id) << " i = " << i;
    ASSERT_NE(audio_pref_handler_->GetUserPriority(AudioDevice(nodes[i])),
              kUserPriorityNone)
        << " i = " << i;
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
