// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device_selection_test_base.h"

#include "base/test/metrics/user_action_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    EXPECT_EQ(actions.GetActionCount("StatusArea_Audio_SwitchInputDevice"), 0);
    EXPECT_EQ(actions.GetActionCount("StatusArea_Audio_SwitchOutputDevice"), 0);
  }

  {
    base::UserActionTester actions;
    Select(input1);
    ASSERT_EQ(ActiveInputNodeId(), input1.id);
    ASSERT_EQ(ActiveOutputNodeId(), output4.id);
    EXPECT_EQ(actions.GetActionCount("StatusArea_Audio_SwitchInputDevice"), 1);
    EXPECT_EQ(actions.GetActionCount("StatusArea_Audio_SwitchOutputDevice"), 0);
  }

  {
    base::UserActionTester actions;
    Select(output3);
    ASSERT_EQ(ActiveInputNodeId(), input1.id);
    ASSERT_EQ(ActiveOutputNodeId(), output3.id);
    EXPECT_EQ(actions.GetActionCount("StatusArea_Audio_SwitchInputDevice"), 0);
    EXPECT_EQ(actions.GetActionCount("StatusArea_Audio_SwitchOutputDevice"), 1);
  }
}

}  // namespace
}  // namespace ash
