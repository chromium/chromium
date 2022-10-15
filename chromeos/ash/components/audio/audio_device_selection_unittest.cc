// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device_selection_test_base.h"

#include "base/test/metrics/user_action_tester.h"
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

}  // namespace
}  // namespace ash
