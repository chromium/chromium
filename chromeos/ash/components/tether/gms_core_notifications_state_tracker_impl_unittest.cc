// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/gms_core_notifications_state_tracker_impl.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

namespace {

const int kNumTestDevices = 3;

// Creates test devices which have the naming scheme of "testDevice0",
// "testDevice1", etc.
multidevice::RemoteDeviceRefList CreateTestDevices() {
  multidevice::RemoteDeviceRefList test_devices =
      multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices);
  for (size_t i = 0; i < kNumTestDevices; ++i) {
    std::stringstream ss;
    ss << "testDevice" << i;
    test_devices.push_back(multidevice::RemoteDeviceRefBuilder()
                               .SetName(std::string(ss.str()))
                               .Build());
  }
  return test_devices;
}

class TestObserver final : public GmsCoreNotificationsStateTracker::Observer {
 public:
  explicit TestObserver(GmsCoreNotificationsStateTrackerImpl* tracker)
      : tracker_(tracker) {}

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() = default;

  uint32_t change_count() const { return change_count_; }

  const std::vector<std::string>& names_from_last_update() {
    return names_from_last_update_;
  }

  // GmsCoreNotificationsStateTracker::Observer:
  void OnGmsCoreNotificationStateChanged() override {
    names_from_last_update_ =
        tracker_->GetGmsCoreNotificationsDisabledDeviceNames();
    ++change_count_;
  }

 private:
  GmsCoreNotificationsStateTrackerImpl* tracker_;

  uint32_t change_count_ = 0;
  std::vector<std::string> names_from_last_update_;
};

}  // namespace

class GmsCoreNotificationsStateTrackerImplTest : public testing::Test {
 public:
  GmsCoreNotificationsStateTrackerImplTest(
      const GmsCoreNotificationsStateTrackerImplTest&) = delete;
  GmsCoreNotificationsStateTrackerImplTest& operator=(
      const GmsCoreNotificationsStateTrackerImplTest&) = delete;

 protected:
  GmsCoreNotificationsStateTrackerImplTest()
      : test_devices_(CreateTestDevices()) {}

  void SetUp() override {
    devices_to_send_.clear();
    scanned_device_infos_.clear();

    tracker_ = std::make_unique<GmsCoreNotificationsStateTrackerImpl>();

    observer_ = std::make_unique<TestObserver>(tracker_.get());
    tracker_->AddObserver(observer_.get());
  }

  void VerifyExpectedNames(const std::vector<std::string> expected_names,
                           size_t expected_change_count) {
    EXPECT_EQ(expected_names, observer_->names_from_last_update());
    EXPECT_EQ(expected_change_count, observer_->change_count());
  }

  void ReceiveTetherAvailabilityResponse(bool is_final_scan_result) {
    tracker_->OnTetherAvailabilityResponse(
        scanned_device_infos_, devices_to_send_, is_final_scan_result);
  }

  void AddScannedRemoteDevice(multidevice::RemoteDeviceRef remote_device) {
    scanned_device_infos_.emplace_back(remote_device, DeviceStatus(),
                                       false /* setup_required */);
  }

  const multidevice::RemoteDeviceRefList test_devices_;

  std::vector<HostScannerOperation::ScannedDeviceInfo> scanned_device_infos_;
  multidevice::RemoteDeviceRefList devices_to_send_;

  std::unique_ptr<GmsCoreNotificationsStateTrackerImpl> tracker_;
  std::unique_ptr<TestObserver> observer_;
};

TEST_F(GmsCoreNotificationsStateTrackerImplTest, TestTracking) {
  ReceiveTetherAvailabilityResponse(false /* is_final_scan_result */);
  VerifyExpectedNames(std::vector<std::string>(),
                      0 /* expected_change_count */);

  // Add two devices and verify that they are now tracked.
  devices_to_send_.push_back(test_devices_[0]);
  ReceiveTetherAvailabilityResponse(false /* is_final_scan_result */);
  VerifyExpectedNames({test_devices_[0].name()} /* expected_names */,
                      1 /* expected_change_count */);
  devices_to_send_.push_back(test_devices_[1]);
  ReceiveTetherAvailabilityResponse(false /* is_final_scan_result */);
  VerifyExpectedNames(
      {test_devices_[0].name(), test_devices_[1].name()} /* expected_names */,
      2 /* expected_change_count */);

  // Receive another response with the same list; this should not result in an
  // additional "state changed" event.
  ReceiveTetherAvailabilityResponse(false /* is_final_scan_result */);
  VerifyExpectedNames(
      {test_devices_[0].name(), test_devices_[1].name()} /* expected_names */,
      2 /* expected_change_count */);

  // End the scan session.
  ReceiveTetherAvailabilityResponse(true /* is_final_scan_result */);
  VerifyExpectedNames(
      {test_devices_[0].name(), test_devices_[1].name()} /* expected_names */,
      2 /* expected_change_count */);

  // Start a new session; since the previous session contains device 0 and
  // device 1, these devices should remain at least until the scan session ends.
  devices_to_send_.clear();
  ReceiveTetherAvailabilityResponse(false /* is_final_scan_result */);
  VerifyExpectedNames(
      {test_devices_[0].name(), test_devices_[1].name()} /* expected_names */,
      2 /* expected_change_count */);

  // Add two devices (one new and one from a previous session).
  devices_to_send_.push_back(test_devices_[0]);
  ReceiveTetherAvailabilityResponse(false /* is_final_scan_result */);
  VerifyExpectedNames(
      {test_devices_[0].name(), test_devices_[1].name()} /* expected_names */,
      2 /* expected_change_count */);
  devices_to_send_.push_back(test_devices_[2]);
  ReceiveTetherAvailabilityResponse(false /* is_final_scan_result */);
  VerifyExpectedNames({test_devices_[0].name(), test_devices_[1].name(),
                       test_devices_[2].name()} /* expected_names */,
                      3 /* expected_change_count */);

  // End the scan session; since "session2" was not present in this session, it
  // should be removed.
  ReceiveTetherAvailabilityResponse(true /* is_final_scan_result */);
  VerifyExpectedNames(
      {test_devices_[0].name(), test_devices_[2].name()} /* expected_names */,
      4 /* expected_change_count */);

  // Start another session (devices 0 and 2 should still be present).
  devices_to_send_.clear();
  ReceiveTetherAvailabilityResponse(false /* is_final_scan_result */);
  VerifyExpectedNames(
      {test_devices_[0].name(), test_devices_[2].name()} /* expected_names */,
      4 /* expected_change_count */);

  // Add device 0 as a potential tether host. This should cause it to be removed
  // from the list without notifications, even though the scan session has not
  // yet ended.
  AddScannedRemoteDevice(test_devices_[0]);
  ReceiveTetherAvailabilityResponse(false /* is_final_scan_result */);
  VerifyExpectedNames({test_devices_[2].name()} /* expected_names */,
                      5 /* expected_change_count */);

  // Keep device 2 in the list; since it already existed from the previous scan
  // session, no new event should have occurred.
  devices_to_send_.push_back(test_devices_[2]);
  ReceiveTetherAvailabilityResponse(false /* is_final_scan_result */);
  VerifyExpectedNames({test_devices_[2].name()} /* expected_names */,
                      5 /* expected_change_count */);

  // End the scan session.
  ReceiveTetherAvailabilityResponse(true /* is_final_scan_result */);
  VerifyExpectedNames({test_devices_[2].name()} /* expected_names */,
                      5 /* expected_change_count */);

  // Now, destroy |tracker_|; this should result in one more change event.
  tracker_.reset();
  VerifyExpectedNames({} /* expected_names */, 6 /* expected_change_count */);
}

TEST_F(GmsCoreNotificationsStateTrackerImplTest, TestTracking_SameName) {
  ReceiveTetherAvailabilityResponse(false /* is_final_scan_result */);
  VerifyExpectedNames(std::vector<std::string>(),
                      0 /* expected_change_count */);

  // Add device 0.
  devices_to_send_.push_back(test_devices_[0]);
  ReceiveTetherAvailabilityResponse(false /* is_final_scan_result */);
  VerifyExpectedNames({test_devices_[0].name()} /* expected_names */,
                      1 /* expected_change_count */);

  // Make a copy of device 1, and change its name to be the same as device 0's
  // while keeping its public key (and, thus, ID) the same.
  multidevice::RemoteDeviceRef device_1_copy =
      multidevice::RemoteDeviceRefBuilder()
          .SetPublicKey(test_devices_[1].public_key())
          .SetName(test_devices_[0].name())
          .Build();

  // Add the updated device 1. Both names should be sent in the event, even
  // though they are the same name.
  devices_to_send_.push_back(device_1_copy);
  ReceiveTetherAvailabilityResponse(false /* is_final_scan_result */);
  VerifyExpectedNames(
      {test_devices_[0].name(), device_1_copy.name()} /* expected_names */,
      2 /* expected_change_count */);

  tracker_.reset();
  VerifyExpectedNames({} /* expected_names */, 3 /* expected_change_count */);
}

}  // namespace tether

}  // namespace ash
