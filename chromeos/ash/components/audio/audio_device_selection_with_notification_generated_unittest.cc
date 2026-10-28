// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generated test cases from cl/633262013 with a new handler:
// simple_env_with_exception.py. DO NOT EDIT."

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/audio_device_selection_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"

namespace ash {
namespace {

class AudioDeviceSelectionWithNotificationGeneratedTest
    : public AudioDeviceSelectionTestBase {
 public:
  void SetUp() override {
    message_center::MessageCenter::Initialize();
    AudioDeviceSelectionTestBase::SetUp();
  }
  void TearDown() override {
    AudioDeviceSelectionTestBase::TearDown();
    message_center::MessageCenter::Shutdown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       BandDocScenario1Input) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb3);
  // Devices: [internal1 usb2*] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       BandDocScenario1Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb3);
  // Devices: [internal1 usb2*] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       BandDocScenario2Input) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1* usb3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       BandDocScenario2Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1* usb3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       BandDocScenario3Input) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1* usb3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       BandDocScenario3Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1* usb3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       BandDocScenario4Input) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1* usb3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       BandDocScenario4Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1* usb3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       BandDocScenario5Input) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1* usb3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       BandDocScenario5Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1* usb3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       BandDocScenario6Input) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1* usb3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb3);
  // Devices: [internal1 usb2*] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       BandDocScenario6Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1* usb3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb3);
  // Devices: [internal1 usb2*] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       BandDocScenario7Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode usb3 = NewOutputNode("USB");
  AudioNode headphone4 = NewOutputNode("HEADPHONE");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 usb3 headphone4
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] usb3 headphone4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* hdmi2 usb3] headphone4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(hdmi2);
  // Devices: [internal1 hdmi2* usb3] headphone4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, internal1, usb3 = hdmi2
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(headphone4);
  // Devices: [internal1 hdmi2 usb3 headphone4*]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, internal1, usb3 = hdmi2
  //             hdmi2, headphone4, internal1, usb3 = headphone4
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi2, headphone4
  EXPECT_EQ(ActiveOutputNodeId(), headphone4.id);

  Unplug(headphone4);
  // Devices: [internal1 hdmi2* usb3] headphone4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, internal1, usb3 = hdmi2
  //             hdmi2, headphone4, internal1, usb3 = headphone4
  // Most recent active devices (most recent in the end):
  //             internal1, headphone4, hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest, DdDd11Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode headphone2 = NewOutputNode("HEADPHONE");
  AudioNode hdmi3 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] headphone2 hdmi3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(headphone2);
  // Devices: [internal1 headphone2*] hdmi3
  // Pref state: internal1 = internal1
  //             headphone2, internal1 = headphone2
  // Most recent active devices (most recent in the end):
  //             internal1, headphone2
  EXPECT_EQ(ActiveOutputNodeId(), headphone2.id);

  Unplug(headphone2);
  // Devices: [internal1*] headphone2 hdmi3
  // Pref state: internal1 = internal1
  //             headphone2, internal1 = headphone2
  // Most recent active devices (most recent in the end):
  //             headphone2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi3);
  // Devices: [internal1* hdmi3] headphone2
  // Pref state: internal1 = internal1
  //             headphone2, internal1 = headphone2
  //             hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             headphone2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(hdmi3);
  // Devices: [internal1*] headphone2 hdmi3
  // Pref state: internal1 = internal1
  //             headphone2, internal1 = headphone2
  //             hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             headphone2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(headphone2);
  // Devices: [internal1 headphone2*] hdmi3
  // Pref state: internal1 = internal1
  //             headphone2, internal1 = headphone2
  //             hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, headphone2
  EXPECT_EQ(ActiveOutputNodeId(), headphone2.id);

  Unplug(headphone2);
  // Devices: [internal1*] headphone2 hdmi3
  // Pref state: internal1 = internal1
  //             headphone2, internal1 = headphone2
  //             hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             headphone2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi3);
  // Devices: [internal1* hdmi3] headphone2
  // Pref state: internal1 = internal1
  //             headphone2, internal1 = headphone2
  //             hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             headphone2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(hdmi3);
  // Devices: [internal1*] headphone2 hdmi3
  // Pref state: internal1 = internal1
  //             headphone2, internal1 = headphone2
  //             hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             headphone2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest, DdDd12Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode hdmi1 = NewOutputNode("HDMI");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode headphone3 = NewOutputNode("HEADPHONE");
  AudioNode internal4 = NewOutputNode("INTERNAL_SPEAKER");

  Plug(internal4);
  // Devices: [internal4*] hdmi1 hdmi2 headphone3
  // Pref state: internal4 = internal4
  // Most recent active devices (most recent in the end):
  //             internal4
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Plug(hdmi1);
  // Devices: [hdmi1 internal4*] hdmi2 headphone3
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  // Most recent active devices (most recent in the end):
  //             internal4
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Plug(hdmi2);
  // Devices: [hdmi1 hdmi2 internal4*] headphone3
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  //             hdmi1, hdmi2, internal4 = internal4
  // Most recent active devices (most recent in the end):
  //             internal4
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Select(hdmi1);
  // Devices: [hdmi1* hdmi2 internal4] headphone3
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  //             hdmi1, hdmi2, internal4 = hdmi1
  // Most recent active devices (most recent in the end):
  //             internal4, hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), hdmi1.id);

  Unplug(hdmi1);
  // Devices: [hdmi2 internal4*] hdmi1 headphone3
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  //             hdmi1, hdmi2, internal4 = hdmi1
  //             hdmi2, internal4 = internal4
  // Most recent active devices (most recent in the end):
  //             hdmi1, internal4
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Unplug(hdmi2);
  // Devices: [internal4*] hdmi1 hdmi2 headphone3
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  //             hdmi1, hdmi2, internal4 = hdmi1
  //             hdmi2, internal4 = internal4
  // Most recent active devices (most recent in the end):
  //             hdmi1, internal4
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Plug(headphone3);
  // Devices: [headphone3* internal4] hdmi1 hdmi2
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  //             hdmi1, hdmi2, internal4 = hdmi1
  //             hdmi2, internal4 = internal4
  //             headphone3, internal4 = headphone3
  // Most recent active devices (most recent in the end):
  //             hdmi1, internal4, headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Unplug(headphone3);
  // Devices: [internal4*] hdmi1 hdmi2 headphone3
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  //             hdmi1, hdmi2, internal4 = hdmi1
  //             hdmi2, internal4 = internal4
  //             headphone3, internal4 = headphone3
  // Most recent active devices (most recent in the end):
  //             hdmi1, headphone3, internal4
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Plug(hdmi2);
  // Devices: [hdmi2 internal4*] hdmi1 headphone3
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  //             hdmi1, hdmi2, internal4 = hdmi1
  //             hdmi2, internal4 = internal4
  //             headphone3, internal4 = headphone3
  // Most recent active devices (most recent in the end):
  //             hdmi1, headphone3, internal4
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Plug(hdmi1);
  // Devices: [hdmi1* hdmi2 internal4] headphone3
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  //             hdmi1, hdmi2, internal4 = hdmi1
  //             hdmi2, internal4 = internal4
  //             headphone3, internal4 = headphone3
  // Most recent active devices (most recent in the end):
  //             headphone3, internal4, hdmi1
  EXPECT_EQ(ActiveOutputNodeId(), hdmi1.id);

  Unplug(hdmi1);
  // Devices: [hdmi2 internal4*] hdmi1 headphone3
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  //             hdmi1, hdmi2, internal4 = hdmi1
  //             hdmi2, internal4 = internal4
  //             headphone3, internal4 = headphone3
  // Most recent active devices (most recent in the end):
  //             headphone3, hdmi1, internal4
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Unplug(hdmi2);
  // Devices: [internal4*] hdmi1 hdmi2 headphone3
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  //             hdmi1, hdmi2, internal4 = hdmi1
  //             hdmi2, internal4 = internal4
  //             headphone3, internal4 = headphone3
  // Most recent active devices (most recent in the end):
  //             headphone3, hdmi1, internal4
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Plug(hdmi1);
  // Devices: [hdmi1 internal4*] hdmi2 headphone3
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  //             hdmi1, hdmi2, internal4 = hdmi1
  //             hdmi2, internal4 = internal4
  //             headphone3, internal4 = headphone3
  // Most recent active devices (most recent in the end):
  //             headphone3, hdmi1, internal4
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Plug(hdmi2);
  // Devices: [hdmi1 hdmi2 internal4*] headphone3
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  //             hdmi1, hdmi2, internal4 = internal4
  //             hdmi2, internal4 = internal4
  //             headphone3, internal4 = headphone3
  // Most recent active devices (most recent in the end):
  //             headphone3, hdmi1, internal4
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Unplug(hdmi1);
  // Devices: [hdmi2 internal4*] hdmi1 headphone3
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  //             hdmi1, hdmi2, internal4 = internal4
  //             hdmi2, internal4 = internal4
  //             headphone3, internal4 = headphone3
  // Most recent active devices (most recent in the end):
  //             headphone3, hdmi1, internal4
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Unplug(hdmi2);
  // Devices: [internal4*] hdmi1 hdmi2 headphone3
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  //             hdmi1, hdmi2, internal4 = internal4
  //             hdmi2, internal4 = internal4
  //             headphone3, internal4 = headphone3
  // Most recent active devices (most recent in the end):
  //             headphone3, hdmi1, internal4
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);

  Plug(headphone3);
  // Devices: [headphone3* internal4] hdmi1 hdmi2
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  //             hdmi1, hdmi2, internal4 = internal4
  //             hdmi2, internal4 = internal4
  //             headphone3, internal4 = headphone3
  // Most recent active devices (most recent in the end):
  //             hdmi1, internal4, headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Unplug(headphone3);
  // Devices: [internal4*] hdmi1 hdmi2 headphone3
  // Pref state: internal4 = internal4
  //             hdmi1, internal4 = internal4
  //             hdmi1, hdmi2, internal4 = internal4
  //             hdmi2, internal4 = internal4
  //             headphone3, internal4 = headphone3
  // Most recent active devices (most recent in the end):
  //             hdmi1, headphone3, internal4
  EXPECT_EQ(ActiveOutputNodeId(), internal4.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest, DdDd21Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode headphone3 = NewOutputNode("HEADPHONE");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 headphone3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] headphone3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(headphone3);
  // Devices: [internal1 hdmi2 headphone3*]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = headphone3
  // Most recent active devices (most recent in the end):
  //             internal1, headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Unplug(headphone3);
  // Devices: [internal1* hdmi2] headphone3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = headphone3
  // Most recent active devices (most recent in the end):
  //             headphone3, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest, DdDd22Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode headphone3 = NewOutputNode("HEADPHONE");
  AudioNode hdmi4 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 headphone3 hdmi4
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] headphone3 hdmi4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(headphone3);
  // Devices: [internal1 hdmi2 headphone3*] hdmi4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = headphone3
  // Most recent active devices (most recent in the end):
  //             internal1, headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Select(hdmi2);
  // Devices: [internal1 hdmi2* headphone3] hdmi4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = hdmi2
  // Most recent active devices (most recent in the end):
  //             internal1, headphone3, hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(hdmi4);
  // Devices: [internal1 hdmi2* headphone3 hdmi4]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = hdmi2
  //             hdmi2, hdmi4, headphone3, internal1 = hdmi2
  // Most recent active devices (most recent in the end):
  //             internal1, headphone3, hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Select(hdmi4);
  // Devices: [internal1 hdmi2 headphone3 hdmi4*]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = hdmi2
  //             hdmi2, hdmi4, headphone3, internal1 = hdmi4
  // Most recent active devices (most recent in the end):
  //             internal1, headphone3, hdmi2, hdmi4
  EXPECT_EQ(ActiveOutputNodeId(), hdmi4.id);

  Unplug(hdmi4);
  // Devices: [internal1 hdmi2* headphone3] hdmi4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = hdmi2
  //             hdmi2, hdmi4, headphone3, internal1 = hdmi4
  // Most recent active devices (most recent in the end):
  //             internal1, headphone3, hdmi4, hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest, DdDd23Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode headphone3 = NewOutputNode("HEADPHONE");
  AudioNode hdmi4 = NewOutputNode("HDMI");
  AudioNode hdmi5 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 headphone3 hdmi4 hdmi5
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] headphone3 hdmi4 hdmi5
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(headphone3);
  // Devices: [internal1 hdmi2 headphone3*] hdmi4 hdmi5
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = headphone3
  // Most recent active devices (most recent in the end):
  //             internal1, headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Select(hdmi2);
  // Devices: [internal1 hdmi2* headphone3] hdmi4 hdmi5
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = hdmi2
  // Most recent active devices (most recent in the end):
  //             internal1, headphone3, hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(hdmi4);
  // Devices: [internal1 hdmi2* headphone3 hdmi4] hdmi5
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = hdmi2
  //             hdmi2, hdmi4, headphone3, internal1 = hdmi2
  // Most recent active devices (most recent in the end):
  //             internal1, headphone3, hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Select(headphone3);
  // Devices: [internal1 hdmi2 headphone3* hdmi4] hdmi5
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = hdmi2
  //             hdmi2, hdmi4, headphone3, internal1 = headphone3
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi2, headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Plug(hdmi5);
  // Devices: [internal1 hdmi2 headphone3* hdmi4 hdmi5]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = hdmi2
  //             hdmi2, hdmi4, headphone3, internal1 = headphone3
  //             hdmi2, hdmi4, hdmi5, headphone3, internal1 = headphone3
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi2, headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Select(hdmi5);
  // Devices: [internal1 hdmi2 headphone3 hdmi4 hdmi5*]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = hdmi2
  //             hdmi2, hdmi4, headphone3, internal1 = headphone3
  //             hdmi2, hdmi4, hdmi5, headphone3, internal1 = hdmi5
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi2, headphone3, hdmi5
  EXPECT_EQ(ActiveOutputNodeId(), hdmi5.id);

  Unplug(hdmi5);
  // Devices: [internal1 hdmi2 headphone3* hdmi4] hdmi5
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = hdmi2
  //             hdmi2, hdmi4, headphone3, internal1 = headphone3
  //             hdmi2, hdmi4, hdmi5, headphone3, internal1 = hdmi5
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi2, hdmi5, headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Unplug(hdmi4);
  // Devices: [internal1 hdmi2 headphone3*] hdmi4 hdmi5
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = headphone3
  //             hdmi2, hdmi4, headphone3, internal1 = headphone3
  //             hdmi2, hdmi4, hdmi5, headphone3, internal1 = hdmi5
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi2, hdmi5, headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest, DdDd24Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode hdmi3 = NewOutputNode("HDMI");
  AudioNode hdmi4 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 hdmi3 hdmi4
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] hdmi3 hdmi4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi4);
  // Devices: [internal1* hdmi2 hdmi4] hdmi3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi4, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi3);
  // Devices: [internal1* hdmi2 hdmi3 hdmi4]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi4, internal1 = internal1
  //             hdmi2, hdmi3, hdmi4, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(hdmi4);
  // Devices: [internal1 hdmi2 hdmi3 hdmi4*]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi4, internal1 = internal1
  //             hdmi2, hdmi3, hdmi4, internal1 = hdmi4
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi4
  EXPECT_EQ(ActiveOutputNodeId(), hdmi4.id);

  Unplug(hdmi4);
  // Devices: [internal1* hdmi2 hdmi3] hdmi4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi4, internal1 = internal1
  //             hdmi2, hdmi3, hdmi4, internal1 = hdmi4
  //             hdmi2, hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             hdmi4, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi4);
  // Devices: [internal1 hdmi2 hdmi3 hdmi4*]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi4, internal1 = internal1
  //             hdmi2, hdmi3, hdmi4, internal1 = hdmi4
  //             hdmi2, hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi4
  EXPECT_EQ(ActiveOutputNodeId(), hdmi4.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       DiscussionIssue1Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode hdmi3 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] usb2 hdmi3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] hdmi3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi3);
  // Devices: [internal1* usb2 hdmi3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             hdmi3, internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(hdmi3);
  // Devices: [internal1 usb2 hdmi3*]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             hdmi3, internal1, usb2 = hdmi3
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi3
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);

  Select(usb2);
  // Devices: [internal1 usb2* hdmi3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             hdmi3, internal1, usb2 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi3, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Select(internal1);
  // Devices: [internal1* usb2 hdmi3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             hdmi3, internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             hdmi3, usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(usb2);
  // Devices: [internal1* hdmi3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             hdmi3, internal1, usb2 = internal1
  //             hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             hdmi3, usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2 hdmi3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             hdmi3, internal1, usb2 = internal1
  //             hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             hdmi3, usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       DiscussionIssue2Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb3] hdmi2
  // Pref state: internal1 = internal1
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(usb3);
  // Devices: [internal1 usb3*] hdmi2
  // Pref state: internal1 = internal1
  //             internal1, usb3 = usb3
  // Most recent active devices (most recent in the end):
  //             internal1, usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Unplug(usb3);
  // Devices: [internal1*] hdmi2 usb3
  // Pref state: internal1 = internal1
  //             internal1, usb3 = usb3
  // Most recent active devices (most recent in the end):
  //             usb3, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb3 = usb3
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             usb3, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(hdmi2);
  // Devices: [internal1*] hdmi2 usb3
  // Pref state: internal1 = internal1
  //             internal1, usb3 = usb3
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             usb3, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1 usb3*] hdmi2
  // Pref state: internal1 = internal1
  //             internal1, usb3 = usb3
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Plug(hdmi2);
  // Devices: [internal1 hdmi2 usb3*]
  // Pref state: internal1 = internal1
  //             internal1, usb3 = usb3
  //             hdmi2, internal1 = internal1
  //             hdmi2, internal1, usb3 = usb3
  // Most recent active devices (most recent in the end):
  //             internal1, usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       ExceptionalRulesRule1Input) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode mic2 = NewInputNode("MIC");
  AudioNode bluetooth3 = NewInputNode("BLUETOOTH");

  Plug(internal1);
  // Devices: [internal1*] mic2 bluetooth3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(mic2);
  // Devices: [internal1 mic2*] bluetooth3
  // Pref state: internal1 = internal1
  //             internal1, mic2 = mic2
  // Most recent active devices (most recent in the end):
  //             internal1, mic2
  EXPECT_EQ(ActiveInputNodeId(), mic2.id);

  Plug(bluetooth3);
  // Devices: [internal1 mic2 bluetooth3*]
  // Pref state: internal1 = internal1
  //             internal1, mic2 = mic2
  //             bluetooth3, internal1, mic2 = bluetooth3
  // Most recent active devices (most recent in the end):
  //             internal1, mic2, bluetooth3
  EXPECT_EQ(ActiveInputNodeId(), bluetooth3.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       ExceptionalRulesRule1Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode headphone2 = NewOutputNode("HEADPHONE");
  AudioNode bluetooth3 = NewOutputNode("BLUETOOTH");

  Plug(internal1);
  // Devices: [internal1*] headphone2 bluetooth3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(headphone2);
  // Devices: [internal1 headphone2*] bluetooth3
  // Pref state: internal1 = internal1
  //             headphone2, internal1 = headphone2
  // Most recent active devices (most recent in the end):
  //             internal1, headphone2
  EXPECT_EQ(ActiveOutputNodeId(), headphone2.id);

  Plug(bluetooth3);
  // Devices: [internal1 headphone2 bluetooth3*]
  // Pref state: internal1 = internal1
  //             headphone2, internal1 = headphone2
  //             bluetooth3, headphone2, internal1 = bluetooth3
  // Most recent active devices (most recent in the end):
  //             internal1, headphone2, bluetooth3
  EXPECT_EQ(ActiveOutputNodeId(), bluetooth3.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       ExceptionalRulesRule2Input) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb3);
  // Devices: [internal1 usb2*] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       ExceptionalRulesRule2Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb3);
  // Devices: [internal1 usb2*] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       ExceptionalRulesRule3Input) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Unplug(usb3);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2*] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       ExceptionalRulesRule3Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(usb3);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2*] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = usb2
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       ExceptionalRulesRule4Input) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1* usb3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       ExceptionalRulesRule4Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2 usb3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] usb3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1* usb2 usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(usb2);
  // Devices: [internal1 usb2* usb3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1* usb3] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, usb3 = usb2
  //             internal1, usb3 = internal1
  // Most recent active devices (most recent in the end):
  //             usb2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       FeedbackComment10Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode hdmi3 = NewOutputNode("HDMI");
  AudioNode usb4 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 hdmi3 usb4
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] hdmi3 usb4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi3);
  // Devices: [internal1* hdmi2 hdmi3] usb4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(hdmi3);
  // Devices: [internal1 hdmi2 hdmi3*] usb4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = hdmi3
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi3
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);

  Plug(usb4);
  // Devices: [internal1 hdmi2 hdmi3* usb4]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = hdmi3
  //             hdmi2, hdmi3, internal1, usb4 = hdmi3
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi3
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);

  Unplug(usb4);
  // Devices: [internal1 hdmi2 hdmi3*] usb4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = hdmi3
  //             hdmi2, hdmi3, internal1, usb4 = hdmi3
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi3
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       FeedbackComment3Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode hdmi3 = NewOutputNode("HDMI");
  AudioNode usb4 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 hdmi3 usb4
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] hdmi3 usb4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi3);
  // Devices: [internal1* hdmi2 hdmi3] usb4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb4);
  // Devices: [internal1* hdmi2 hdmi3 usb4]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = internal1
  //             hdmi2, hdmi3, internal1, usb4 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(usb4);
  // Devices: [internal1 hdmi2 hdmi3 usb4*]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = internal1
  //             hdmi2, hdmi3, internal1, usb4 = usb4
  // Most recent active devices (most recent in the end):
  //             internal1, usb4
  EXPECT_EQ(ActiveOutputNodeId(), usb4.id);

  Unplug(hdmi2);
  // Devices: [internal1 hdmi3 usb4*] hdmi2
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = internal1
  //             hdmi2, hdmi3, internal1, usb4 = usb4
  //             hdmi3, internal1, usb4 = usb4
  // Most recent active devices (most recent in the end):
  //             internal1, usb4
  EXPECT_EQ(ActiveOutputNodeId(), usb4.id);

  Unplug(hdmi3);
  // Devices: [internal1 usb4*] hdmi2 hdmi3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = internal1
  //             hdmi2, hdmi3, internal1, usb4 = usb4
  //             hdmi3, internal1, usb4 = usb4
  //             internal1, usb4 = usb4
  // Most recent active devices (most recent in the end):
  //             internal1, usb4
  EXPECT_EQ(ActiveOutputNodeId(), usb4.id);

  Unplug(usb4);
  // Devices: [internal1*] hdmi2 hdmi3 usb4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = internal1
  //             hdmi2, hdmi3, internal1, usb4 = usb4
  //             hdmi3, internal1, usb4 = usb4
  //             internal1, usb4 = usb4
  // Most recent active devices (most recent in the end):
  //             usb4, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] hdmi3 usb4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = internal1
  //             hdmi2, hdmi3, internal1, usb4 = usb4
  //             hdmi3, internal1, usb4 = usb4
  //             internal1, usb4 = usb4
  // Most recent active devices (most recent in the end):
  //             usb4, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi3);
  // Devices: [internal1* hdmi2 hdmi3] usb4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = internal1
  //             hdmi2, hdmi3, internal1, usb4 = usb4
  //             hdmi3, internal1, usb4 = usb4
  //             internal1, usb4 = usb4
  // Most recent active devices (most recent in the end):
  //             usb4, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb4);
  // Devices: [internal1 hdmi2 hdmi3 usb4*]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = internal1
  //             hdmi2, hdmi3, internal1, usb4 = usb4
  //             hdmi3, internal1, usb4 = usb4
  //             internal1, usb4 = usb4
  // Most recent active devices (most recent in the end):
  //             internal1, usb4
  EXPECT_EQ(ActiveOutputNodeId(), usb4.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       FeedbackComment5Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode hdmi3 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 hdmi3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] hdmi3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi3);
  // Devices: [internal1* hdmi2 hdmi3]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(hdmi2);
  // Devices: [internal1 hdmi2* hdmi3]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = hdmi2
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Unplug(hdmi2);
  // Devices: [internal1* hdmi3] hdmi2
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = hdmi2
  //             hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             hdmi2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(hdmi3);
  // Devices: [internal1*] hdmi2 hdmi3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = hdmi2
  //             hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             hdmi2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] hdmi3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = hdmi2
  //             hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             hdmi2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi3);
  // Devices: [internal1* hdmi2 hdmi3]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = internal1
  //             hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             hdmi2, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       FeedbackComment8Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode headphone3 = NewOutputNode("HEADPHONE");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 headphone3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] headphone3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(headphone3);
  // Devices: [internal1 hdmi2 headphone3*]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = headphone3
  // Most recent active devices (most recent in the end):
  //             internal1, headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Unplug(headphone3);
  // Devices: [internal1* hdmi2] headphone3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = headphone3
  // Most recent active devices (most recent in the end):
  //             headphone3, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest, GreendocH4Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] hdmi2
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(hdmi2);
  // Devices: [internal1*] hdmi2
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest, GreendocH7Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode headphone3 = NewOutputNode("HEADPHONE");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 headphone3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] headphone3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(headphone3);
  // Devices: [internal1 hdmi2 headphone3*]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = headphone3
  // Most recent active devices (most recent in the end):
  //             internal1, headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Unplug(headphone3);
  // Devices: [internal1* hdmi2] headphone3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, headphone3, internal1 = headphone3
  // Most recent active devices (most recent in the end):
  //             headphone3, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest, GreendocM1Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode hdmi3 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 hdmi3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] hdmi3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi3);
  // Devices: [internal1* hdmi2 hdmi3]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest, GreendocM3Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode hdmi3 = NewOutputNode("HDMI");
  AudioNode headphone4 = NewOutputNode("HEADPHONE");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 hdmi3 headphone4
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] hdmi3 headphone4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi3);
  // Devices: [internal1* hdmi2 hdmi3] headphone4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(hdmi2);
  // Devices: [internal1 hdmi2* hdmi3] headphone4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = hdmi2
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(headphone4);
  // Devices: [internal1 hdmi2 hdmi3 headphone4*]
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = hdmi2
  //             hdmi2, hdmi3, headphone4, internal1 = headphone4
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi2, headphone4
  EXPECT_EQ(ActiveOutputNodeId(), headphone4.id);

  Unplug(headphone4);
  // Devices: [internal1 hdmi2* hdmi3] headphone4
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi2, hdmi3, internal1 = hdmi2
  //             hdmi2, hdmi3, headphone4, internal1 = headphone4
  // Most recent active devices (most recent in the end):
  //             internal1, headphone4, hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest, GreendocM4Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode hdmi3 = NewOutputNode("HDMI");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 hdmi3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] hdmi3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(hdmi2);
  // Devices: [internal1*] hdmi2 hdmi3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi3);
  // Devices: [internal1* hdmi3] hdmi2
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi3, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Select(hdmi3);
  // Devices: [internal1 hdmi3*] hdmi2
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi3, internal1 = hdmi3
  // Most recent active devices (most recent in the end):
  //             internal1, hdmi3
  EXPECT_EQ(ActiveOutputNodeId(), hdmi3.id);

  Unplug(hdmi3);
  // Devices: [internal1*] hdmi2 hdmi3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi3, internal1 = hdmi3
  // Most recent active devices (most recent in the end):
  //             hdmi3, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] hdmi3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             hdmi3, internal1 = hdmi3
  // Most recent active devices (most recent in the end):
  //             hdmi3, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest, GreendocM5Output) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode hdmi2 = NewOutputNode("HDMI");
  AudioNode headphone3 = NewOutputNode("HEADPHONE");

  Plug(internal1);
  // Devices: [internal1*] hdmi2 headphone3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] headphone3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(hdmi2);
  // Devices: [internal1*] hdmi2 headphone3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(headphone3);
  // Devices: [internal1 headphone3*] hdmi2
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             headphone3, internal1 = headphone3
  // Most recent active devices (most recent in the end):
  //             internal1, headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Unplug(headphone3);
  // Devices: [internal1*] hdmi2 headphone3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             headphone3, internal1 = headphone3
  // Most recent active devices (most recent in the end):
  //             headphone3, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(hdmi2);
  // Devices: [internal1* hdmi2] headphone3
  // Pref state: internal1 = internal1
  //             hdmi2, internal1 = internal1
  //             headphone3, internal1 = headphone3
  // Most recent active devices (most recent in the end):
  //             headphone3, internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest, HdmiReplugUsbOutput) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode usb1 = NewOutputNode("USB");
  AudioNode hdmi2 = NewOutputNode("HDMI");

  Plug(usb1);
  // Devices: [usb1*] hdmi2
  // Pref state: usb1 = usb1
  // Most recent active devices (most recent in the end):
  //             usb1
  EXPECT_EQ(ActiveOutputNodeId(), usb1.id);

  Plug(hdmi2);
  // Devices: [usb1* hdmi2]
  // Pref state: usb1 = usb1
  //             hdmi2, usb1 = usb1
  // Most recent active devices (most recent in the end):
  //             usb1
  EXPECT_EQ(ActiveOutputNodeId(), usb1.id);

  Select(hdmi2);
  // Devices: [usb1 hdmi2*]
  // Pref state: usb1 = usb1
  //             hdmi2, usb1 = hdmi2
  // Most recent active devices (most recent in the end):
  //             usb1, hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Unplug(usb1);
  // Devices: [hdmi2*] usb1
  // Pref state: usb1 = usb1
  //             hdmi2, usb1 = hdmi2
  //             hdmi2 = hdmi2
  // Most recent active devices (most recent in the end):
  //             usb1, hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);

  Plug(usb1);
  // Devices: [usb1 hdmi2*]
  // Pref state: usb1 = usb1
  //             hdmi2, usb1 = hdmi2
  //             hdmi2 = hdmi2
  // Most recent active devices (most recent in the end):
  //             usb1, hdmi2
  EXPECT_EQ(ActiveOutputNodeId(), hdmi2.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       InternalReplugUsbInput) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewInputNode("INTERNAL_MIC");
  AudioNode usb2 = NewInputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Unplug(usb2);
  // Devices: [internal1*] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       InternalReplugUsbOutput) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");

  Plug(internal1);
  // Devices: [internal1*] usb2
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Unplug(usb2);
  // Devices: [internal1*] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest, SimpleInput) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode usb1 = NewInputNode("USB");
  AudioNode usb2 = NewInputNode("USB");
  AudioNode usb3 = NewInputNode("USB");

  Plug(usb1);
  // Devices: [usb1*] usb2 usb3
  // Pref state: usb1 = usb1
  // Most recent active devices (most recent in the end):
  //             usb1
  EXPECT_EQ(ActiveInputNodeId(), usb1.id);

  Plug(usb2);
  // Devices: [usb1* usb2] usb3
  // Pref state: usb1 = usb1
  //             usb1, usb2 = usb1
  // Most recent active devices (most recent in the end):
  //             usb1
  EXPECT_EQ(ActiveInputNodeId(), usb1.id);

  Select(usb2);
  // Devices: [usb1 usb2*] usb3
  // Pref state: usb1 = usb1
  //             usb1, usb2 = usb2
  // Most recent active devices (most recent in the end):
  //             usb1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [usb1 usb2* usb3]
  // Pref state: usb1 = usb1
  //             usb1, usb2 = usb2
  //             usb1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             usb1, usb2
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Select(usb1);
  // Devices: [usb1* usb2 usb3]
  // Pref state: usb1 = usb1
  //             usb1, usb2 = usb2
  //             usb1, usb2, usb3 = usb1
  // Most recent active devices (most recent in the end):
  //             usb2, usb1
  EXPECT_EQ(ActiveInputNodeId(), usb1.id);

  Unplug(usb3);
  // Devices: [usb1* usb2] usb3
  // Pref state: usb1 = usb1
  //             usb1, usb2 = usb1
  //             usb1, usb2, usb3 = usb1
  // Most recent active devices (most recent in the end):
  //             usb2, usb1
  EXPECT_EQ(ActiveInputNodeId(), usb1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest, SimpleOutput) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode usb1 = NewOutputNode("USB");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode usb3 = NewOutputNode("USB");

  Plug(usb1);
  // Devices: [usb1*] usb2 usb3
  // Pref state: usb1 = usb1
  // Most recent active devices (most recent in the end):
  //             usb1
  EXPECT_EQ(ActiveOutputNodeId(), usb1.id);

  Plug(usb2);
  // Devices: [usb1* usb2] usb3
  // Pref state: usb1 = usb1
  //             usb1, usb2 = usb1
  // Most recent active devices (most recent in the end):
  //             usb1
  EXPECT_EQ(ActiveOutputNodeId(), usb1.id);

  Select(usb2);
  // Devices: [usb1 usb2*] usb3
  // Pref state: usb1 = usb1
  //             usb1, usb2 = usb2
  // Most recent active devices (most recent in the end):
  //             usb1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [usb1 usb2* usb3]
  // Pref state: usb1 = usb1
  //             usb1, usb2 = usb2
  //             usb1, usb2, usb3 = usb2
  // Most recent active devices (most recent in the end):
  //             usb1, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Select(usb1);
  // Devices: [usb1* usb2 usb3]
  // Pref state: usb1 = usb1
  //             usb1, usb2 = usb2
  //             usb1, usb2, usb3 = usb1
  // Most recent active devices (most recent in the end):
  //             usb2, usb1
  EXPECT_EQ(ActiveOutputNodeId(), usb1.id);

  Unplug(usb3);
  // Devices: [usb1* usb2] usb3
  // Pref state: usb1 = usb1
  //             usb1, usb2 = usb1
  //             usb1, usb2, usb3 = usb1
  // Most recent active devices (most recent in the end):
  //             usb2, usb1
  EXPECT_EQ(ActiveOutputNodeId(), usb1.id);
}

TEST_F(AudioDeviceSelectionWithNotificationGeneratedTest,
       PersistActiveUsbHeadphoneAcrossRebootUsbComeLater) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  AudioNode internal1 = NewOutputNode("INTERNAL_SPEAKER");
  AudioNode usb2 = NewOutputNode("USB");
  AudioNode headphone3 = NewOutputNode("HEADPHONE");

  Plug(internal1);
  // Devices: [internal1*] usb2 headphone3
  // Pref state: internal1 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1* usb2] headphone3
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  // Most recent active devices (most recent in the end):
  //             internal1
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(headphone3);
  // Devices: [internal1 usb headphone3*]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, headphone3 = headphone3
  // Most recent active devices (most recent in the end):
  //             internal1, headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Select(usb2);
  // Devices: [internal1 usb2* headphone3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, headphone3 = usb2
  // Most recent active devices (most recent in the end):
  //             internal1, headphone3, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Unplug(usb2);
  // Devices: [internal1 headphone3*] usb2
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, headphone3 = usb2
  //             internal1, headphone3 = headphone3
  // Most recent active devices (most recent in the end):
  //             internal1, usb2, headphone3
  EXPECT_EQ(ActiveOutputNodeId(), headphone3.id);

  Plug(usb2);
  // Devices: [internal1 usb2* headphone3]
  // Pref state: internal1 = internal1
  //             internal1, usb2 = internal1
  //             internal1, usb2, headphone3 = usb2
  //             internal1, headphone3 = headphone3
  // Most recent active devices (most recent in the end):
  //             internal1, headphone3, usb2
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);
}

}  // namespace
}  // namespace ash
