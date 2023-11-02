// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/host_scan_device_prioritizer_impl.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/ash/components/tether/tether_host_response_recorder.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

namespace {

multidevice::RemoteDeviceRef CreateRemoteDeviceRef(
    int id,
    int64_t last_update_time_millis) {
  return multidevice::RemoteDeviceRefBuilder()
      .SetPublicKey("publicKey" + base::NumberToString(id))
      .SetLastUpdateTimeMillis(last_update_time_millis)
      .Build();
}

}  // namespace

class HostScanDevicePrioritizerImplTest : public testing::Test {
 public:
  HostScanDevicePrioritizerImplTest(const HostScanDevicePrioritizerImplTest&) =
      delete;
  HostScanDevicePrioritizerImplTest& operator=(
      const HostScanDevicePrioritizerImplTest&) = delete;

 protected:
  HostScanDevicePrioritizerImplTest()
      : test_devices_(multidevice::CreateRemoteDeviceRefListForTest(10)) {}

  void SetUp() override {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    TetherHostResponseRecorder::RegisterPrefs(pref_service_->registry());
    recorder_ =
        std::make_unique<TetherHostResponseRecorder>(pref_service_.get());

    prioritizer_ =
        std::make_unique<HostScanDevicePrioritizerImpl>(recorder_.get());
  }

  multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<TetherHostResponseRecorder> recorder_;

  std::unique_ptr<HostScanDevicePrioritizerImpl> prioritizer_;
};

TEST_F(HostScanDevicePrioritizerImplTest,
       TestOnlyLastUpdateTime_RemoteDevices) {
  test_devices_[0] = CreateRemoteDeviceRef(0, 15000L);
  test_devices_[1] = CreateRemoteDeviceRef(1, 20000L);
  test_devices_[2] = CreateRemoteDeviceRef(2, 300L);
  test_devices_[3] = CreateRemoteDeviceRef(3, 10000L);
  test_devices_[4] = CreateRemoteDeviceRef(4, 5000L);
  test_devices_[5] = CreateRemoteDeviceRef(5, 30000L);
  test_devices_[6] = CreateRemoteDeviceRef(6, 600L);

  // Do not receive a TetherAvailabilityResponse or ConnectTetheringResponse.

  multidevice::RemoteDeviceRefList test_vector =
      multidevice::RemoteDeviceRefList{test_devices_[6], test_devices_[5],
                                       test_devices_[4], test_devices_[3],
                                       test_devices_[2], test_devices_[1],
                                       test_devices_[0]};

  prioritizer_->SortByHostScanOrder(&test_vector);
  EXPECT_EQ((multidevice::RemoteDeviceRefList{
                test_devices_[5], test_devices_[1], test_devices_[0],
                test_devices_[3], test_devices_[4], test_devices_[6],
                test_devices_[2]}),
            test_vector);
}

TEST_F(HostScanDevicePrioritizerImplTest,
       TestOnlyTetherAvailabilityResponses_RemoteDevices) {
  // Receive TetherAvailabilityResponses from devices 0-4.
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[2]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[3]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[4]);

  // Do not receive a ConnectTetheringResponse.

  multidevice::RemoteDeviceRefList test_vector =
      multidevice::RemoteDeviceRefList{test_devices_[6], test_devices_[5],
                                       test_devices_[4], test_devices_[3],
                                       test_devices_[2], test_devices_[1],
                                       test_devices_[0]};

  prioritizer_->SortByHostScanOrder(&test_vector);
  EXPECT_EQ((multidevice::RemoteDeviceRefList{
                test_devices_[4], test_devices_[3], test_devices_[2],
                test_devices_[1], test_devices_[0], test_devices_[6],
                test_devices_[5]}),
            test_vector);
}

TEST_F(HostScanDevicePrioritizerImplTest,
       TestBothTypesOfResponses_RemoteDevices) {
  // Receive TetherAvailabilityResponses from devices 0-4.
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[2]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[3]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[4]);

  // Receive ConnectTetheringResponse from device 0.
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[0]);

  multidevice::RemoteDeviceRefList test_vector =
      multidevice::RemoteDeviceRefList{test_devices_[6], test_devices_[5],
                                       test_devices_[4], test_devices_[3],
                                       test_devices_[2], test_devices_[1],
                                       test_devices_[0]};

  prioritizer_->SortByHostScanOrder(&test_vector);
  EXPECT_EQ((multidevice::RemoteDeviceRefList{
                test_devices_[0], test_devices_[4], test_devices_[3],
                test_devices_[2], test_devices_[1], test_devices_[6],
                test_devices_[5]}),
            test_vector);
}

TEST_F(HostScanDevicePrioritizerImplTest,
       TestBothTypesOfResponses_DifferentOrder_RemoteDevices) {
  // Receive different order.
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[2]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[4]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[3]);

  // Receive ConnectTetheringResponse from device 1.
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[1]);

  multidevice::RemoteDeviceRefList test_vector =
      multidevice::RemoteDeviceRefList{test_devices_[9], test_devices_[8],
                                       test_devices_[7], test_devices_[6],
                                       test_devices_[5], test_devices_[4],
                                       test_devices_[3], test_devices_[2],
                                       test_devices_[1], test_devices_[0]};

  prioritizer_->SortByHostScanOrder(&test_vector);
  EXPECT_EQ((multidevice::RemoteDeviceRefList{
                test_devices_[1], test_devices_[3], test_devices_[4],
                test_devices_[2], test_devices_[0], test_devices_[9],
                test_devices_[8], test_devices_[7], test_devices_[6],
                test_devices_[5]}),
            test_vector);
}

TEST_F(HostScanDevicePrioritizerImplTest,
       TestLastUpdateTimeAndBothTypesOfResponses_RemoteDevices) {
  test_devices_[0] = CreateRemoteDeviceRef(0, 2000L);
  test_devices_[1] = CreateRemoteDeviceRef(1, 9000000L);
  test_devices_[2] = CreateRemoteDeviceRef(2, 3000L);
  test_devices_[3] = CreateRemoteDeviceRef(3, 7000L);
  test_devices_[4] = CreateRemoteDeviceRef(4, 5000L);
  test_devices_[5] = CreateRemoteDeviceRef(5, 4000L);
  test_devices_[6] = CreateRemoteDeviceRef(6, 10L);
  test_devices_[7] = CreateRemoteDeviceRef(7, 40L);
  test_devices_[8] = CreateRemoteDeviceRef(8, 80L);
  test_devices_[9] = CreateRemoteDeviceRef(9, 20L);

  // Receive different order.
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[2]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[4]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[3]);

  // Receive ConnectTetheringResponse from device 1.
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[1]);

  multidevice::RemoteDeviceRefList test_vector =
      multidevice::RemoteDeviceRefList{test_devices_[9], test_devices_[8],
                                       test_devices_[7], test_devices_[6],
                                       test_devices_[5], test_devices_[4],
                                       test_devices_[3], test_devices_[2],
                                       test_devices_[1], test_devices_[0]};

  prioritizer_->SortByHostScanOrder(&test_vector);
  EXPECT_EQ((multidevice::RemoteDeviceRefList{
                test_devices_[1], test_devices_[3], test_devices_[4],
                test_devices_[2], test_devices_[0], test_devices_[5],
                test_devices_[8], test_devices_[7], test_devices_[9],
                test_devices_[6]}),
            test_vector);
}

}  // namespace tether

}  // namespace ash
