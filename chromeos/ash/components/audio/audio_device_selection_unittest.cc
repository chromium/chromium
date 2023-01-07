// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/cras_audio_handler.h"

#include <inttypes.h>

#include "ash/constants/ash_features.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler_impl.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class ActiveNodeObserver : public CrasAudioClient::Observer {
 protected:
  void ActiveInputNodeChanged(uint64_t node_id) override {
    active_input_node_id_ = node_id;
  }
  void ActiveOutputNodeChanged(uint64_t node_id) override {
    active_output_node_id_ = node_id;
  }

 public:
  uint64_t GetActiveInputNodeId() { return active_input_node_id_; }
  uint64_t GetActiveOutputNodeId() { return active_output_node_id_; }
  void Reset() {
    active_input_node_id_ = 0;
    active_output_node_id_ = 0;
  }

 private:
  uint64_t active_input_node_id_ = 0;
  uint64_t active_output_node_id_ = 0;
};

class AudioDeviceSelectionTest : public testing::Test {
 public:
  AudioDeviceSelectionTest()
      : feature_list_(chromeos::features::kRobustAudioDeviceSelectLogic) {}

 protected:
  void SetUp() override {
    node_count_ = 0;
    plugged_time_ = 0;

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    AudioDevicesPrefHandlerImpl::RegisterPrefs(pref_service_->registry());
    audio_pref_handler_ = audio_pref_handler_ =
        new AudioDevicesPrefHandlerImpl(pref_service_.get());

    CrasAudioClient::InitializeFake();
    fake_cras_audio_client_ = FakeCrasAudioClient::Get();
    // Delete audio nodes created in FakeCrasAudioClient::FakeCrasAudioClient()
    fake_cras_audio_client_->SetAudioNodesForTesting({});
    active_node_observer_.Reset();
    fake_cras_audio_client_->AddObserver(&active_node_observer_);

    CrasAudioHandler::Initialize(mojo::NullRemote(), audio_pref_handler_);
    cras_audio_handler_ = CrasAudioHandler::Get();

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    CrasAudioHandler::Shutdown();
    audio_pref_handler_ = nullptr;
    CrasAudioClient::Shutdown();
    pref_service_.reset();
  }

  // Register audio devices available in each test case.
  // Each call to NewInputNode or NewOutputNode returns an AudioNode
  // with an auto-incremented node ID, starting at 1.
  AudioNode NewInputNode(const std::string& type) {
    return NewNode(true, type);
  }
  AudioNode NewOutputNode(const std::string& type) {
    return NewNode(false, type);
  }

  void Plug(AudioNode node) {
    node.plugged_time = ++plugged_time_;
    fake_cras_audio_client_->InsertAudioNodeToList(node);
  }
  void Unplug(const AudioNode& node) {
    fake_cras_audio_client_->RemoveAudioNodeFromList(node.id);
  }
  void Select(const AudioNode& node) {
    if (node.is_input) {
      ASSERT_TRUE(cras_audio_handler_->SetActiveInputNodes({node.id}));
    } else {
      ASSERT_TRUE(cras_audio_handler_->SetActiveOutputNodes({node.id}));
    }
  }

  uint64_t ActiveInputNodeId() {
    return active_node_observer_.GetActiveInputNodeId();
  }
  uint64_t ActiveOutputNodeId() {
    return active_node_observer_.GetActiveOutputNodeId();
  }

 private:
  AudioNode NewNode(bool is_input, const std::string& type) {
    ++node_count_;
    std::string name =
        base::StringPrintf("%s-%" PRIu64, type.c_str(), node_count_);
    return AudioNode(is_input, node_count_, true, node_count_, node_count_,
                     name, type, name, false, 0, 2, 0, 0);
  }

 private:
  // Test services
  ActiveNodeObserver active_node_observer_;
  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  CrasAudioHandler* cras_audio_handler_ = nullptr;         // Not owned.
  FakeCrasAudioClient* fake_cras_audio_client_ = nullptr;  // Not owned.
  scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

  // Counters
  uint64_t node_count_ = 0;
  uint64_t plugged_time_ = 0;
};

TEST_F(AudioDeviceSelectionTest, GeneratedBandDocScenario1Input) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedBandDocScenario1Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedBandDocScenario2Input) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedBandDocScenario2Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedBandDocScenario3Input) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedBandDocScenario3Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedBandDocScenario4Input) {
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

  Unplug(usb2);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Plug(usb2);
  // Devices: [internal1 usb2 usb3*]
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);
}

TEST_F(AudioDeviceSelectionTest, GeneratedBandDocScenario4Output) {
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

  Unplug(usb2);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb3);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Plug(usb2);
  // Devices: [internal1 usb2 usb3*]
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);
}

TEST_F(AudioDeviceSelectionTest, GeneratedBandDocScenario5Input) {
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

  Unplug(usb2);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveInputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveInputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2 usb3*]
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveInputNodeId(), usb3.id);
}

TEST_F(AudioDeviceSelectionTest, GeneratedBandDocScenario5Output) {
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

  Unplug(usb2);
  // Devices: [internal1 usb3*] usb2
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);

  Unplug(usb3);
  // Devices: [internal1*] usb2 usb3
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), internal1.id);

  Plug(usb2);
  // Devices: [internal1 usb2*] usb3
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb2.id);

  Plug(usb3);
  // Devices: [internal1 usb2 usb3*]
  // List: internal1 < usb2 < usb3
  EXPECT_EQ(ActiveOutputNodeId(), usb3.id);
}

TEST_F(AudioDeviceSelectionTest, GeneratedBandDocScenario6Input) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedBandDocScenario6Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedBandDocScenario7Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedDdDd11Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedDdDd12Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedDdDd21Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedDdDd22Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedDdDd23Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedDdDd24Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedDiscussionIssue1Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedDiscussionIssue2Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedFeedbackComment3Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedFeedbackComment5Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedFeedbackComment8Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedFeedbackComment10Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedGreendocH4Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedGreendocH7Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedGreendocM1Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedGreendocM3Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedGreendocM4Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedGreendocM5Output) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedSimpleInput) {
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

TEST_F(AudioDeviceSelectionTest, GeneratedSimpleOutput) {
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

}  // namespace
}  // namespace ash
