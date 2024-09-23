// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generated test cases from cl/480876614.
// DO NOT EDIT.

#include "chromeos/ash/components/audio/audio_device_selection_test_base.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class AudioDeviceSelectionGeneratedTest : public AudioDeviceSelectionTestBase {
};

TEST_F(AudioDeviceSelectionGeneratedTest, BandDocScenario1Input) {
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // List: internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2 usb3*]
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb3);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, BandDocScenario1Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2 usb3*]
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb3);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, BandDocScenario2Input) {
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // List: internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2 usb3*]
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Plug(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, BandDocScenario2Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2 usb3*]
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Plug(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, BandDocScenario3Input) {
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // List: internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2 usb3*]
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, BandDocScenario3Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2 usb3*]
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, BandDocScenario4Input) {
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // List: internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2 usb3*]
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Plug(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, BandDocScenario4Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2 usb3*]
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Plug(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, BandDocScenario5Input) {
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // List: internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2 usb3*]
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, BandDocScenario5Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2 usb3*]
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, BandDocScenario6Input) {
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // List: internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2 usb3*]
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Plug(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb3);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1*] usb2 usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, BandDocScenario6Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2 usb3*]
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Plug(usb2);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb3);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1*] usb2 usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2* usb3]
  // List: internal1 < usb3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, BandDocScenario7Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode usb3 = NewOutputNode("USB");
  AudioNode headphone4 = NewOutputNode("HEADPHONE");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 usb3 headphone4
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] usb3 headphone4
  // List: internal1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(usb3);
  // Devices: [internal1 hdmi2 usb3*] headphone4
  // List: internal1 < hdmi2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Select(hdmi2);
  // Devices: [internal1 hdmi2* usb3] headphone4
  // List: internal1 < usb3 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(headphone4);
  // Devices: [internal1 hdmi2 usb3 headphone4*]
  // List: internal1 < usb3 < hdmi2 < headphone4
  EXPECT_EQ(ActiveOutputNodeId(), headphone4.id);

  Unplug(headphone4);
  // Devices: [internal1 hdmi2* usb3] headphone4
  // List: internal1 < usb3 < hdmi2 < headphone4
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, DdDd11Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode headphone2 = NewOutputNode("HEADPHONE");
  AudioNode hdmi3 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] headphone2 hdmi3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(headphone2);
  // Devices: [internal1 headphone2*] hdmi3
  // List: internal1 < headphone2
  EXPECT_EQ(ActiveOutputNodeId(), headphone2.id);

  Unplug(headphone2);
  // Devices: [internal1*] headphone2 hdmi3
  // List: internal1 < headphone2
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi3);
  // Devices: [internal1 hdmi3*] headphone2
  // List: internal1 < hdmi3 < headphone2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);

  Select(internal1);
  // Devices: [internal1* hdmi3] headphone2
  // List: hdmi3 < internal1 < headphone2
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(hdmi3);
  // Devices: [internal1*] headphone2 hdmi3
  // List: hdmi3 < internal1 < headphone2
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(headphone2);
  // Devices: [internal1 headphone2*] hdmi3
  // List: hdmi3 < internal1 < headphone2
  EXPECT_EQ(ActiveOutputNodeId(), headphone2.id);

  Unplug(headphone2);
  // Devices: [internal1*] headphone2 hdmi3
  // List: hdmi3 < internal1 < headphone2
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi3);
  // Devices: [internal1* hdmi3] headphone2
  // List: hdmi3 < internal1 < headphone2
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(hdmi3);
  // Devices: [internal1*] headphone2 hdmi3
  // List: hdmi3 < internal1 < headphone2
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, DdDd12Output) {
  AudioNode hdmi1 = NewOutputNode("HDMI");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode headphone3 = NewOutputNode("HEADPHONE");
  AudioNode internal4 = NewOutputNode("INTERNAL_SPEAKER");

  Plug(internal4);
  // Devices: [internal4*] hdmi1 hdmi2 headphone3
  // List: internal4
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Plug(hdmi1);
  // Devices: [hdmi1* internal4] hdmi2 headphone3
  // List: internal4 < hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), hdmi1.id);

  Plug(hdmi2);
  // Devices: [hdmi1 hdmi2* internal4] headphone3
  // List: internal4 < hdmi1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Select(hdmi1);
  // Devices: [hdmi1* hdmi2 internal4] headphone3
  // List: internal4 < hdmi2 < hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), hdmi1.id);

  Unplug(hdmi1);
  // Devices: [hdmi2* internal4] hdmi1 headphone3
  // List: internal4 < hdmi2 < hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Unplug(hdmi2);
  // Devices: [internal4*] hdmi1 hdmi2 headphone3
  // List: internal4 < hdmi2 < hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Plug(headphone3);
  // Devices: [headphone3* internal4] hdmi1 hdmi2
  // List: internal4 < headphone3 < hdmi2 < hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Unplug(headphone3);
  // Devices: [internal4*] hdmi1 hdmi2 headphone3
  // List: internal4 < headphone3 < hdmi2 < hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Plug(hdmi2);
  // Devices: [hdmi2* internal4] hdmi1 headphone3
  // List: internal4 < headphone3 < hdmi2 < hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(hdmi1);
  // Devices: [hdmi1* hdmi2 internal4] headphone3
  // List: internal4 < headphone3 < hdmi2 < hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), hdmi1.id);

  Unplug(hdmi1);
  // Devices: [hdmi2* internal4] hdmi1 headphone3
  // List: internal4 < headphone3 < hdmi2 < hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Unplug(hdmi2);
  // Devices: [internal4*] hdmi1 hdmi2 headphone3
  // List: internal4 < headphone3 < hdmi2 < hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Plug(hdmi1);
  // Devices: [hdmi1* internal4] hdmi2 headphone3
  // List: internal4 < headphone3 < hdmi2 < hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), hdmi1.id);

  Plug(hdmi2);
  // Devices: [hdmi1* hdmi2 internal4] headphone3
  // List: internal4 < headphone3 < hdmi2 < hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), hdmi1.id);

  Unplug(hdmi1);
  // Devices: [hdmi2* internal4] hdmi1 headphone3
  // List: internal4 < headphone3 < hdmi2 < hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Unplug(hdmi2);
  // Devices: [internal4*] hdmi1 hdmi2 headphone3
  // List: internal4 < headphone3 < hdmi2 < hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Plug(headphone3);
  // Devices: [headphone3* internal4] hdmi1 hdmi2
  // List: internal4 < headphone3 < hdmi2 < hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Unplug(headphone3);
  // Devices: [internal4*] hdmi1 hdmi2 headphone3
  // List: internal4 < headphone3 < hdmi2 < hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, DdDd21Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode headphone3 = NewOutputNode("HEADPHONE");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 headphone3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] headphone3
  // List: internal1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Select(internal1);
  // Devices: [internal1* hdmi2] headphone3
  // List: hdmi2 < internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(headphone3);
  // Devices: [internal1 hdmi2 headphone3*]
  // List: hdmi2 < internal1 < headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Unplug(headphone3);
  // Devices: [internal1* hdmi2] headphone3
  // List: hdmi2 < internal1 < headphone3
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, DdDd22Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode headphone3 = NewOutputNode("HEADPHONE");
  AudioNode hdmi4 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 headphone3 hdmi4
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] headphone3 hdmi4
  // List: internal1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(headphone3);
  // Devices: [internal1 hdmi2 headphone3*] hdmi4
  // List: internal1 < hdmi2 < headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Select(hdmi2);
  // Devices: [internal1 hdmi2* headphone3] hdmi4
  // List: internal1 < headphone3 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(hdmi4);
  // Devices: [internal1 hdmi2 headphone3 hdmi4*]
  // List: internal1 < headphone3 < hdmi2 < hdmi4
  EXPECT_EQ(ActiveOutputNodeId(), hdmi4.id);

  Unplug(hdmi4);
  // Devices: [internal1 hdmi2* headphone3] hdmi4
  // List: internal1 < headphone3 < hdmi2 < hdmi4
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, DdDd23Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode headphone3 = NewOutputNode("HEADPHONE");
  AudioNode hdmi4 = NewOutputNode("HDMI");
  AudioNode hdmi5 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 headphone3 hdmi4 hdmi5
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] headphone3 hdmi4 hdmi5
  // List: internal1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(headphone3);
  // Devices: [internal1 hdmi2 headphone3*] hdmi4 hdmi5
  // List: internal1 < hdmi2 < headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Select(hdmi2);
  // Devices: [internal1 hdmi2* headphone3] hdmi4 hdmi5
  // List: internal1 < headphone3 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(hdmi4);
  // Devices: [internal1 hdmi2 headphone3 hdmi4*] hdmi5
  // List: internal1 < headphone3 < hdmi2 < hdmi4
  EXPECT_EQ(ActiveOutputNodeId(), hdmi4.id);

  Select(headphone3);
  // Devices: [internal1 hdmi2 headphone3* hdmi4] hdmi5
  // List: internal1 < hdmi2 < hdmi4 < headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Plug(hdmi5);
  // Devices: [internal1 hdmi2 headphone3* hdmi4 hdmi5]
  // List: internal1 < hdmi2 < hdmi4 < headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Select(hdmi5);
  // Devices: [internal1 hdmi2 headphone3 hdmi4 hdmi5*]
  // List: internal1 < hdmi2 < hdmi4 < headphone3 < hdmi5
  EXPECT_EQ(ActiveOutputNodeId(), hdmi5.id);

  Unplug(hdmi5);
  // Devices: [internal1 hdmi2 headphone3* hdmi4] hdmi5
  // List: internal1 < hdmi2 < hdmi4 < headphone3 < hdmi5
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Unplug(hdmi4);
  // Devices: [internal1 hdmi2 headphone3*] hdmi4 hdmi5
  // List: internal1 < hdmi2 < hdmi4 < headphone3 < hdmi5
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, DdDd24Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode hdmi3 = NewOutputNode("HDMI");
  AudioNode hdmi4 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 hdmi3 hdmi4
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] hdmi3 hdmi4
  // List: internal1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(hdmi4);
  // Devices: [internal1 hdmi2 hdmi4*] hdmi3
  // List: internal1 < hdmi2 < hdmi4
  EXPECT_EQ(ActiveOutputNodeId(), hdmi4.id);

  Plug(hdmi3);
  // Devices: [internal1 hdmi2 hdmi3* hdmi4]
  // List: internal1 < hdmi2 < hdmi4 < hdmi3
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);

  Select(hdmi4);
  // Devices: [internal1 hdmi2 hdmi3 hdmi4*]
  // List: internal1 < hdmi2 < hdmi3 < hdmi4
  EXPECT_EQ(ActiveOutputNodeId(), hdmi4.id);

  Unplug(hdmi4);
  // Devices: [internal1 hdmi2 hdmi3*] hdmi4
  // List: internal1 < hdmi2 < hdmi3 < hdmi4
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);

  Select(internal1);
  // Devices: [internal1* hdmi2 hdmi3] hdmi4
  // List: hdmi2 < hdmi3 < internal1 < hdmi4
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi4);
  // Devices: [internal1 hdmi2 hdmi3 hdmi4*]
  // List: hdmi2 < hdmi3 < internal1 < hdmi4
  EXPECT_EQ(ActiveOutputNodeId(), hdmi4.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, DiscussionIssue1Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode hdmi3 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] usb2 hdmi3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] hdmi3
  // List: internal1 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(hdmi3);
  // Devices: [internal1 usb2* hdmi3]
  // List: internal1 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Select(hdmi3);
  // Devices: [internal1 usb2 hdmi3*]
  // List: internal1 < usb2 < hdmi3
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);

  Select(usb2);
  // Devices: [internal1 usb2* hdmi3]
  // List: internal1 < hdmi3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Select(internal1);
  // Devices: [internal1* usb2 hdmi3]
  // List: hdmi3 < usb2 < internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(usb2);
  // Devices: [internal1* hdmi3] usb2
  // List: hdmi3 < usb2 < internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2 hdmi3]
  // List: hdmi3 < usb2 < internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, DiscussionIssue2Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 usb3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1 usb3*] hdmi2
  // List: internal1 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Unplug(usb3);
  // Devices: [internal1*] hdmi2 usb3
  // List: internal1 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] usb3
  // List: internal1 < hdmi2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Select(internal1);
  // Devices: [internal1* hdmi2] usb3
  // List: hdmi2 < internal1 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(hdmi2);
  // Devices: [internal1*] hdmi2 usb3
  // List: hdmi2 < internal1 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1 usb3*] hdmi2
  // List: hdmi2 < internal1 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2 usb3*]
  // List: hdmi2 < internal1 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, FeedbackComment3Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode hdmi3 = NewOutputNode("HDMI");
  AudioNode usb4 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 hdmi3 usb4
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] hdmi3 usb4
  // List: internal1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(hdmi3);
  // Devices: [internal1 hdmi2 hdmi3*] usb4
  // List: internal1 < hdmi2 < hdmi3
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);

  Plug(usb4);
  // Devices: [internal1 hdmi2 hdmi3 usb4*]
  // List: internal1 < hdmi2 < hdmi3 < usb4
  EXPECT_EQ(ActiveOutputNodeId(), usb4.id);

  Unplug(hdmi2);
  // Devices: [internal1 hdmi3 usb4*] hdmi2
  // List: internal1 < hdmi2 < hdmi3 < usb4
  EXPECT_EQ(ActiveOutputNodeId(), usb4.id);

  Unplug(hdmi3);
  // Devices: [internal1 usb4*] hdmi2 hdmi3
  // List: internal1 < hdmi2 < hdmi3 < usb4
  EXPECT_EQ(ActiveOutputNodeId(), usb4.id);

  Unplug(usb4);
  // Devices: [internal1*] hdmi2 hdmi3 usb4
  // List: internal1 < hdmi2 < hdmi3 < usb4
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] hdmi3 usb4
  // List: internal1 < hdmi2 < hdmi3 < usb4
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(hdmi3);
  // Devices: [internal1 hdmi2 hdmi3*] usb4
  // List: internal1 < hdmi2 < hdmi3 < usb4
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);

  Plug(usb4);
  // Devices: [internal1 hdmi2 hdmi3 usb4*]
  // List: internal1 < hdmi2 < hdmi3 < usb4
  EXPECT_EQ(ActiveOutputNodeId(), usb4.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, FeedbackComment5Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode hdmi3 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 hdmi3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] hdmi3
  // List: internal1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(hdmi3);
  // Devices: [internal1 hdmi2 hdmi3*]
  // List: internal1 < hdmi2 < hdmi3
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);

  Select(hdmi2);
  // Devices: [internal1 hdmi2* hdmi3]
  // List: internal1 < hdmi3 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Unplug(hdmi2);
  // Devices: [internal1 hdmi3*] hdmi2
  // List: internal1 < hdmi3 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);

  Unplug(hdmi3);
  // Devices: [internal1*] hdmi2 hdmi3
  // List: internal1 < hdmi3 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] hdmi3
  // List: internal1 < hdmi3 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(hdmi3);
  // Devices: [internal1 hdmi2* hdmi3]
  // List: internal1 < hdmi3 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, FeedbackComment8Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode headphone3 = NewOutputNode("HEADPHONE");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 headphone3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] headphone3
  // List: internal1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Select(internal1);
  // Devices: [internal1* hdmi2] headphone3
  // List: hdmi2 < internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(headphone3);
  // Devices: [internal1 hdmi2 headphone3*]
  // List: hdmi2 < internal1 < headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Unplug(headphone3);
  // Devices: [internal1* hdmi2] headphone3
  // List: hdmi2 < internal1 < headphone3
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, FeedbackComment10Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode hdmi3 = NewOutputNode("HDMI");
  AudioNode usb4 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 hdmi3 usb4
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] hdmi3 usb4
  // List: internal1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(hdmi3);
  // Devices: [internal1 hdmi2 hdmi3*] usb4
  // List: internal1 < hdmi2 < hdmi3
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);

  Plug(usb4);
  // Devices: [internal1 hdmi2 hdmi3 usb4*]
  // List: internal1 < hdmi2 < hdmi3 < usb4
  EXPECT_EQ(ActiveOutputNodeId(), usb4.id);

  Unplug(usb4);
  // Devices: [internal1 hdmi2 hdmi3*] usb4
  // List: internal1 < hdmi2 < hdmi3 < usb4
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, GreendocH4Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] hdmi2
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*]
  // List: internal1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Select(internal1);
  // Devices: [internal1* hdmi2]
  // List: hdmi2 < internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(hdmi2);
  // Devices: [internal1*] hdmi2
  // List: hdmi2 < internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2]
  // List: hdmi2 < internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, GreendocH7Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode headphone3 = NewOutputNode("HEADPHONE");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 headphone3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] headphone3
  // List: internal1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Select(internal1);
  // Devices: [internal1* hdmi2] headphone3
  // List: hdmi2 < internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(headphone3);
  // Devices: [internal1 hdmi2 headphone3*]
  // List: hdmi2 < internal1 < headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Unplug(headphone3);
  // Devices: [internal1* hdmi2] headphone3
  // List: hdmi2 < internal1 < headphone3
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, GreendocM1Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode hdmi3 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 hdmi3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] hdmi3
  // List: internal1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(hdmi3);
  // Devices: [internal1 hdmi2 hdmi3*]
  // List: internal1 < hdmi2 < hdmi3
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, GreendocM3Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode hdmi3 = NewOutputNode("HDMI");
  AudioNode headphone4 = NewOutputNode("HEADPHONE");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 hdmi3 headphone4
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] hdmi3 headphone4
  // List: internal1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(hdmi3);
  // Devices: [internal1 hdmi2 hdmi3*] headphone4
  // List: internal1 < hdmi2 < hdmi3
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);

  Select(hdmi2);
  // Devices: [internal1 hdmi2* hdmi3] headphone4
  // List: internal1 < hdmi3 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(headphone4);
  // Devices: [internal1 hdmi2 hdmi3 headphone4*]
  // List: internal1 < hdmi3 < hdmi2 < headphone4
  EXPECT_EQ(ActiveOutputNodeId(), headphone4.id);

  Unplug(headphone4);
  // Devices: [internal1 hdmi2* hdmi3] headphone4
  // List: internal1 < hdmi3 < hdmi2 < headphone4
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, GreendocM4Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode hdmi3 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 hdmi3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] hdmi3
  // List: internal1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Select(internal1);
  // Devices: [internal1* hdmi2] hdmi3
  // List: hdmi2 < internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(hdmi2);
  // Devices: [internal1*] hdmi2 hdmi3
  // List: hdmi2 < internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi3);
  // Devices: [internal1 hdmi3*] hdmi2
  // List: hdmi2 < internal1 < hdmi3
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);

  Unplug(hdmi3);
  // Devices: [internal1*] hdmi2 hdmi3
  // List: hdmi2 < internal1 < hdmi3
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] hdmi3
  // List: hdmi2 < internal1 < hdmi3
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, GreendocM5Output) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode headphone3 = NewOutputNode("HEADPHONE");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 headphone3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2*] headphone3
  // List: internal1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Select(internal1);
  // Devices: [internal1* hdmi2] headphone3
  // List: hdmi2 < internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(hdmi2);
  // Devices: [internal1*] hdmi2 headphone3
  // List: hdmi2 < internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(headphone3);
  // Devices: [internal1 headphone3*] hdmi2
  // List: hdmi2 < internal1 < headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Unplug(headphone3);
  // Devices: [internal1*] hdmi2 headphone3
  // List: hdmi2 < internal1 < headphone3
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] headphone3
  // List: hdmi2 < internal1 < headphone3
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, HdmiReplugUsbOutput) {
  AudioNode usb1 = NewOutputNode("USB");
  AudioNode hdmi2 = NewOutputNode("HDMI");

  Plug(usb1);
  // Devices: [usb1*] hdmi2
  // List: usb1
  EXPECT_EQ(ActiveOutputNodeId(), usb1.id);

  Plug(hdmi2);
  // Devices: [usb1* hdmi2]
  // List: usb1
  EXPECT_EQ(ActiveOutputNodeId(), usb1.id);

  Select(hdmi2);
  // Devices: [usb1 hdmi2*]
  // List: usb1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Unplug(usb1);
  // Devices: [hdmi2*] usb1
  // List: usb1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(usb1);
  // Devices: [usb1 hdmi2*]
  // List: usb1 < hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, InternalReplugUsbInput) {
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2
  // List: internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*]
  // List: internal1 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Select(internal1);
  // Devices: [internal1* usb2]
  // List: usb2 < internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Unplug(usb2);
  // Devices: [internal1*] usb2
  // List: usb2 < internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2]
  // List: usb2 < internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, InternalReplugUsbOutput) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*]
  // List: internal1 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Select(internal1);
  // Devices: [internal1* usb2]
  // List: usb2 < internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(usb2);
  // Devices: [internal1*] usb2
  // List: usb2 < internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2]
  // List: usb2 < internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, SimpleInput) {
  AudioNode usb1 = NewInputNode("USB");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(usb1);
  // Devices: [usb1*] usb2 usb3
  // List: usb1
  EXPECT_EQ(ActiveInputNodeId(), usb1.id);

  Plug(usb2);
  // Devices: [usb1 usb2*] usb3
  // List: usb1 < usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [usb1 usb2 usb3*]
  // List: usb1 < usb2 < usb3
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Select(usb1);
  // Devices: [usb1* usb2 usb3]
  // List: usb2 < usb3 < usb1
  EXPECT_EQ(ActiveInputNodeId(), usb1.id);

  Unplug(usb3);
  // Devices: [usb1* usb2] usb3
  // List: usb2 < usb3 < usb1
  EXPECT_EQ(ActiveInputNodeId(), usb1.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest, SimpleOutput) {
  AudioNode usb1 = NewOutputNode("USB");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(usb1);
  // Devices: [usb1*] usb2 usb3
  // List: usb1
  EXPECT_EQ(ActiveOutputNodeId(), usb1.id);

  Plug(usb2);
  // Devices: [usb1 usb2*] usb3
  // List: usb1 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [usb1 usb2 usb3*]
  // List: usb1 < usb2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Select(usb1);
  // Devices: [usb1* usb2 usb3]
  // List: usb2 < usb3 < usb1
  EXPECT_EQ(ActiveOutputNodeId(), usb1.id);

  Unplug(usb3);
  // Devices: [usb1* usb2] usb3
  // List: usb2 < usb3 < usb1
  EXPECT_EQ(ActiveOutputNodeId(), usb1.id);
}

TEST_F(AudioDeviceSelectionGeneratedTest,
       PersistActiveUsbHeadphoneAcrossRebootUsbComeLater) {
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode headphone3 = NewOutputNode("HEADPHONE");

  Plug(internal1);
  // Devices: [internal1*] usb2 headphone3
  // List: internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] headphone3
  // List: internal1 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(headphone3);
  // Devices: [internal1 usb headphone3*]
  // List: internal1 < usb2 < headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Select(usb2);
  // Devices: [internal1 usb2* headphone3]
  // List: internal1 <  headphone3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1 headphone3*] usb2*
  // List: internal1 <  headphone3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Plug(usb2);
  // Devices: [internal1 usb2* headphone3]
  // List: internal1 <  headphone3 < usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);
}

}  // namespace
}  // namespace ash
