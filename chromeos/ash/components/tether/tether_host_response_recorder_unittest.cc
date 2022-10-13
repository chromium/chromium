// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_host_response_recorder.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

namespace {

class TestObserver final : public TetherHostResponseRecorder::Observer {
 public:
  TestObserver() : num_callbacks_(0) {}
  ~TestObserver() = default;

  uint32_t num_callbacks() { return num_callbacks_; }

  // TetherHostResponseRecorder::Observer:
  void OnPreviouslyConnectedHostIdsChanged() override { num_callbacks_++; }

 private:
  uint32_t num_callbacks_;
};

}  // namespace

class TetherHostResponseRecorderTest : public testing::Test {
 public:
  TetherHostResponseRecorderTest(const TetherHostResponseRecorderTest&) =
      delete;
  TetherHostResponseRecorderTest& operator=(
      const TetherHostResponseRecorderTest&) = delete;

 protected:
  TetherHostResponseRecorderTest()
      : test_devices_(multidevice::CreateRemoteDeviceRefListForTest(10)) {}

  void SetUp() override {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    TetherHostResponseRecorder::RegisterPrefs(pref_service_->registry());

    recorder_ =
        std::make_unique<TetherHostResponseRecorder>(pref_service_.get());

    test_observer_ = base::WrapUnique(new TestObserver());
    recorder_->AddObserver(test_observer_.get());
  }

  const multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<TestObserver> test_observer_;

  std::unique_ptr<TetherHostResponseRecorder> recorder_;
};

TEST_F(TetherHostResponseRecorderTest, TestTetherAvailabilityResponses) {
  // Receive TetherAvailabilityResponses from devices in the following order:
  // 0, 2, 4, 6, 8, 1, 3, 5, 7, 9
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[2]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[4]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[6]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[8]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[3]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[5]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[7]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[9]);

  // The order, from most recent to least recent, should be:
  // 9, 7, 5, 3, 1, 8, 6, 4, 2, 0
  EXPECT_EQ(
      (std::vector<std::string>{
          test_devices_[9].GetDeviceId(), test_devices_[7].GetDeviceId(),
          test_devices_[5].GetDeviceId(), test_devices_[3].GetDeviceId(),
          test_devices_[1].GetDeviceId(), test_devices_[8].GetDeviceId(),
          test_devices_[6].GetDeviceId(), test_devices_[4].GetDeviceId(),
          test_devices_[2].GetDeviceId(), test_devices_[0].GetDeviceId()}),
      recorder_->GetPreviouslyAvailableHostIds());

  EXPECT_EQ(0u, test_observer_->num_callbacks());
}

TEST_F(TetherHostResponseRecorderTest, TestConnectTetheringResponses) {
  // Receive TetherAvailabilityResponses from devices in the following order:
  // 0, 2, 4, 6, 8, 1, 3, 5, 7, 9
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[0]);
  EXPECT_EQ(1u, test_observer_->num_callbacks());
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[2]);
  EXPECT_EQ(2u, test_observer_->num_callbacks());
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[4]);
  EXPECT_EQ(3u, test_observer_->num_callbacks());
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[6]);
  EXPECT_EQ(4u, test_observer_->num_callbacks());
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[8]);
  EXPECT_EQ(5u, test_observer_->num_callbacks());
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[1]);
  EXPECT_EQ(6u, test_observer_->num_callbacks());
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[3]);
  EXPECT_EQ(7u, test_observer_->num_callbacks());
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[5]);
  EXPECT_EQ(8u, test_observer_->num_callbacks());
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[7]);
  EXPECT_EQ(9u, test_observer_->num_callbacks());
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[9]);
  EXPECT_EQ(10u, test_observer_->num_callbacks());

  // The order, from most recent to least recent, should be:
  // 9, 7, 5, 3, 1, 8, 6, 4, 2, 0
  EXPECT_EQ(
      (std::vector<std::string>{
          test_devices_[9].GetDeviceId(), test_devices_[7].GetDeviceId(),
          test_devices_[5].GetDeviceId(), test_devices_[3].GetDeviceId(),
          test_devices_[1].GetDeviceId(), test_devices_[8].GetDeviceId(),
          test_devices_[6].GetDeviceId(), test_devices_[4].GetDeviceId(),
          test_devices_[2].GetDeviceId(), test_devices_[0].GetDeviceId()}),
      recorder_->GetPreviouslyConnectedHostIds());

  EXPECT_EQ(10u, test_observer_->num_callbacks());
}

TEST_F(TetherHostResponseRecorderTest, TestBothResponseTypes) {
  // Receive TetherAvailabilityResponses from devices 0, 1, and 2.
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[2]);

  // Receive a ConnectTetheringResponse from device 2.
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[2]);
  EXPECT_EQ(1u, test_observer_->num_callbacks());

  // Receive TetherAvailabilityResponses from devices 0, 1, and 3.
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[3]);

  // Receive a ConnectTetheringResponse from device 0.
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[0]);
  EXPECT_EQ(2u, test_observer_->num_callbacks());

  // Receive another ConnectTetheringResponse from device 0. Since it was
  // already in the front of the list, this should not trigger a callback.
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[0]);
  EXPECT_EQ(2u, test_observer_->num_callbacks());

  // The order for TetherAvailabilityResponses, from most recent to least
  // recent, should be:
  // 3, 1, 0, 2
  EXPECT_EQ(
      (std::vector<std::string>{
          test_devices_[3].GetDeviceId(), test_devices_[1].GetDeviceId(),
          test_devices_[0].GetDeviceId(), test_devices_[2].GetDeviceId()}),
      recorder_->GetPreviouslyAvailableHostIds());

  // The order for ConnectTetheringResponses, from most recent to least
  // recent, should be:
  // 0, 2
  EXPECT_EQ((std::vector<std::string>{test_devices_[0].GetDeviceId(),
                                      test_devices_[2].GetDeviceId()}),
            recorder_->GetPreviouslyConnectedHostIds());

  EXPECT_EQ(2u, test_observer_->num_callbacks());
}

}  // namespace tether

}  // namespace ash
