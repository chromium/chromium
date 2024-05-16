// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/management/telemetry_management_service_ash.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
constexpr uint64_t kFakeAudioOutputNodeId =
    0x100000001;  // From `FakeCrasAudioClient`.
constexpr uint64_t kFakeAudioInputNodeId =
    0x100000002;  // From `FakeCrasAudioClient`.

}  // namespace

class TelemetryManagementServiceAshTest : public testing::Test {
 public:
  void SetUp() override {}

  void TearDown() override {}

  crosapi::mojom::TelemetryManagementServiceProxy* management_service() const {
    return remote_management_service_.get();
  }

  CrasAudioHandler& cras_audio_handler() { return cras_audio_handler_.Get(); }

 protected:
  uint64_t GetFakeAudioNodeId() {
    uint64_t nonexistent_node_id = 0;
    AudioDeviceList devices;
    cras_audio_handler().GetAudioDevices(&devices);
    for (const auto& device : devices) {
      nonexistent_node_id = std::max(nonexistent_node_id, device.id) + 1;
    }
    return nonexistent_node_id;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  // ScopedCrasAudioHandlerForTesting is a helper class that initializes
  // the `CrasAudioHandler` in its constructor with cleanup in its destructor.
  ScopedCrasAudioHandlerForTesting cras_audio_handler_;

  mojo::Remote<crosapi::mojom::TelemetryManagementService>
      remote_management_service_;
  std::unique_ptr<crosapi::mojom::TelemetryManagementService>
      management_service_{TelemetryManagementServiceAsh::Factory::Create(
          remote_management_service_.BindNewPipeAndPassReceiver())};
};

// Tests that AudioSetGain forwards requests to CrasAudioHandler to set the
// audio gain correctly.
TEST_F(TelemetryManagementServiceAshTest, AudioSetGainSuccess) {
  // Set to an arbitrary value first.
  cras_audio_handler().SetVolumeGainPercentForDevice(kFakeAudioInputNodeId, 10);

  constexpr int32_t expected_gain = 60;
  base::test::TestFuture<bool> future;
  management_service()->SetAudioGain(kFakeAudioInputNodeId, expected_gain,
                                     future.GetCallback());
  EXPECT_TRUE(future.Get());
  EXPECT_EQ(
      cras_audio_handler().GetInputGainPercentForDevice(kFakeAudioInputNodeId),
      expected_gain);
}

// Tests that AudioSetGain sets gain to max (100) when |gain| exceeds max.
TEST_F(TelemetryManagementServiceAshTest, AudioSetGainInvalidGainAboveMax) {
  // Set to an arbitrary value first.
  cras_audio_handler().SetVolumeGainPercentForDevice(kFakeAudioInputNodeId, 10);

  base::test::TestFuture<bool> future;
  management_service()->SetAudioGain(kFakeAudioInputNodeId, 999,
                                     future.GetCallback());
  EXPECT_TRUE(future.Get());
  EXPECT_EQ(
      cras_audio_handler().GetInputGainPercentForDevice(kFakeAudioInputNodeId),
      100);
}

// Tests that AudioSetGain sets gain to min (0) when |gain| is below min.
TEST_F(TelemetryManagementServiceAshTest, AudioSetGainInvalidGainBelowMin) {
  // Set to an arbitrary value first.
  cras_audio_handler().SetVolumeGainPercentForDevice(kFakeAudioInputNodeId, 10);

  base::test::TestFuture<bool> future;
  management_service()->SetAudioGain(kFakeAudioInputNodeId, -100,
                                     future.GetCallback());
  EXPECT_TRUE(future.Get());
  EXPECT_EQ(
      cras_audio_handler().GetInputGainPercentForDevice(kFakeAudioInputNodeId),
      0);
}

// Tests that AudioSetGain returns false when |node_id| is invalid.
TEST_F(TelemetryManagementServiceAshTest, AudioSetGainInvalidNodeId) {
  base::test::TestFuture<bool> future;
  management_service()->SetAudioGain(GetFakeAudioNodeId(), 60,
                                     future.GetCallback());
  EXPECT_FALSE(future.Get());
}

// Tests that AudioSetGain return false if the audio node is an output node.
TEST_F(TelemetryManagementServiceAshTest, AudioSetGainWithOutputNode) {
  // Set to an arbitrary value first.
  cras_audio_handler().SetVolumeGainPercentForDevice(kFakeAudioOutputNodeId,
                                                     10);

  base::test::TestFuture<bool> future;
  management_service()->SetAudioGain(kFakeAudioOutputNodeId, 60,
                                     future.GetCallback());
  EXPECT_FALSE(future.Get());
  EXPECT_EQ(
      cras_audio_handler().GetInputGainPercentForDevice(kFakeAudioOutputNodeId),
      10);
}

// Tests that AudioSetVolume forwards requests to CrasAudioHandler to set the
// audio volume correctly.
TEST_F(TelemetryManagementServiceAshTest, AudioSetVolumeSuccess) {
  // Set to an arbitrary value first.
  cras_audio_handler().SetVolumeGainPercentForDevice(kFakeAudioOutputNodeId,
                                                     10);
  cras_audio_handler().SetMuteForDevice(kFakeAudioOutputNodeId, false);

  constexpr int32_t expected_volume = 60;
  base::test::TestFuture<bool> future;
  management_service()->SetAudioVolume(kFakeAudioOutputNodeId, expected_volume,
                                       true, future.GetCallback());
  EXPECT_TRUE(future.Get());
  EXPECT_EQ(cras_audio_handler().GetOutputVolumePercentForDevice(
                kFakeAudioOutputNodeId),
            expected_volume);
  EXPECT_TRUE(
      cras_audio_handler().IsOutputMutedForDevice(kFakeAudioOutputNodeId));
}

// Tests that AudioSetVolume sets volume to max (100) when |volume| exceeds max.
TEST_F(TelemetryManagementServiceAshTest, AudioSetVolumeInvalidVolumeAboveMax) {
  // Set to an arbitrary value first.
  cras_audio_handler().SetVolumeGainPercentForDevice(kFakeAudioOutputNodeId,
                                                     10);
  cras_audio_handler().SetMuteForDevice(kFakeAudioOutputNodeId, false);

  base::test::TestFuture<bool> future;
  management_service()->SetAudioVolume(kFakeAudioOutputNodeId, 999, true,
                                       future.GetCallback());
  EXPECT_TRUE(future.Get());
  EXPECT_EQ(cras_audio_handler().GetOutputVolumePercentForDevice(
                kFakeAudioOutputNodeId),
            100);
  EXPECT_TRUE(
      cras_audio_handler().IsOutputMutedForDevice(kFakeAudioOutputNodeId));
}

// Tests that AudioSetVolume sets volume to min (0) when |volume| is below min.
TEST_F(TelemetryManagementServiceAshTest, AudioSetVolumeInvalidVolumeBelowMin) {
  // Set to an arbitrary value first.
  cras_audio_handler().SetVolumeGainPercentForDevice(kFakeAudioOutputNodeId,
                                                     10);
  cras_audio_handler().SetMuteForDevice(kFakeAudioOutputNodeId, false);

  base::test::TestFuture<bool> future;
  management_service()->SetAudioVolume(kFakeAudioOutputNodeId, -100, true,
                                       future.GetCallback());
  EXPECT_TRUE(future.Get());
  EXPECT_EQ(cras_audio_handler().GetOutputVolumePercentForDevice(
                kFakeAudioOutputNodeId),
            0);
  EXPECT_TRUE(
      cras_audio_handler().IsOutputMutedForDevice(kFakeAudioOutputNodeId));
}

// Tests that AudioSetVolume returns false when |node_id| is invalid.
TEST_F(TelemetryManagementServiceAshTest, AudioSetVolumeInvalidNodeId) {
  base::test::TestFuture<bool> future;
  management_service()->SetAudioVolume(GetFakeAudioNodeId(), 60, false,
                                       future.GetCallback());
  EXPECT_FALSE(future.Get());
}

// Tests that AudioSetVolume return false if the audio node is an input node.
TEST_F(TelemetryManagementServiceAshTest, AudioSetVolumeNoUnmuteInput) {
  // Input mute state is only recorded in CRAS when the input node is active.
  cras_audio_handler().SetActiveInputNodes({kFakeAudioInputNodeId});
  // Set to an arbitrary value first.
  cras_audio_handler().SetVolumeGainPercentForDevice(kFakeAudioInputNodeId, 10);
  cras_audio_handler().SetMuteForDevice(kFakeAudioInputNodeId, true);

  EXPECT_TRUE(
      cras_audio_handler().IsInputMutedForDevice(kFakeAudioInputNodeId));
  base::test::TestFuture<bool> future;
  management_service()->SetAudioVolume(kFakeAudioInputNodeId, 60, false,
                                       future.GetCallback());
  EXPECT_FALSE(future.Get());
  EXPECT_EQ(cras_audio_handler().GetOutputVolumePercentForDevice(
                kFakeAudioInputNodeId),
            10);
  EXPECT_TRUE(
      cras_audio_handler().IsInputMutedForDevice(kFakeAudioInputNodeId));
}

}  // namespace ash
