// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_host.h"

#include "base/uuid.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::tether {
class TetherHostTest : public testing::Test {
 public:
  nearby::presence::PresenceDevice CreatePresenceDeviceForTest() {
    nearby::internal::DeviceIdentityMetaData device_identity_metadata;
    device_identity_metadata.set_device_name(
        base::Uuid::GenerateRandomV4().AsLowercaseString());
    device_identity_metadata.set_device_id(
        multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
            base::Uuid::GenerateRandomV4().AsLowercaseString()));
    return nearby::presence::PresenceDevice(device_identity_metadata);
  }
};

TEST_F(TetherHostTest, TestAccessorsWithPresenceDevice) {
  nearby::presence::PresenceDevice presence_device =
      CreatePresenceDeviceForTest();
  TetherHost tether_host(presence_device);

  EXPECT_EQ(tether_host.presence_device().value(), presence_device);
  EXPECT_FALSE(tether_host.remote_device_ref().has_value());
  EXPECT_EQ(tether_host.GetDeviceId(),
            presence_device.GetDeviceIdentityMetadata().device_id());
  EXPECT_EQ(tether_host.GetName(),
            presence_device.GetDeviceIdentityMetadata().device_name());
}

TEST_F(TetherHostTest, TestAccessorsWithRemoteDeviceRef) {
  auto remote_device_ref = multidevice::CreateRemoteDeviceRefForTest();
  TetherHost tether_host(remote_device_ref);
  EXPECT_EQ(tether_host.remote_device_ref().value(), remote_device_ref);
  EXPECT_FALSE(tether_host.presence_device().has_value());
  EXPECT_EQ(tether_host.GetDeviceId(), remote_device_ref.GetDeviceId());
  EXPECT_EQ(tether_host.GetName(), remote_device_ref.name());
}

TEST_F(TetherHostTest, TestEqualityWithRemoteDeviceRef) {
  TetherHost first_tether_host(multidevice::CreateRemoteDeviceRefForTest());
  TetherHost second_tether_host(first_tether_host.remote_device_ref().value());

  EXPECT_EQ(first_tether_host, second_tether_host);
}

TEST_F(TetherHostTest, TestInequalityWithRemoteDeviceRef) {
  multidevice::RemoteDeviceRefList remote_device_refs =
      multidevice::CreateRemoteDeviceRefListForTest(2);
  TetherHost first_tether_host(remote_device_refs[0]);
  TetherHost second_tether_host(remote_device_refs[1]);

  EXPECT_NE(first_tether_host, second_tether_host);
}

TEST_F(TetherHostTest, TestEqualityWithPresenceDevice) {
  TetherHost first_tether_host(CreatePresenceDeviceForTest());
  TetherHost second_tether_host(first_tether_host.presence_device().value());

  EXPECT_EQ(first_tether_host, second_tether_host);
}

TEST_F(TetherHostTest, TestInequalityWithPresenceDevice) {
  TetherHost first_tether_host(CreatePresenceDeviceForTest());
  TetherHost second_tether_host(CreatePresenceDeviceForTest());

  EXPECT_NE(first_tether_host, second_tether_host);
}

TEST_F(TetherHostTest, TestInequalityWithRemoteDeviceRefAndPresenceDevice) {
  TetherHost first_tether_host(multidevice::CreateRemoteDeviceRefForTest());
  TetherHost second_tether_host(CreatePresenceDeviceForTest());

  EXPECT_NE(first_tether_host, second_tether_host);
}

}  // namespace ash::tether
