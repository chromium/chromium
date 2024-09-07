// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/enterprise_companion_status.h"

#include "base/ranges/algorithm.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom-forward.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "chrome/enterprise_companion/proto/enterprise_companion_event.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_companion {

TEST(EnterpriseCompanionStatusTest, FromDeviceManagementStatusSuccess) {
  EnterpriseCompanionStatus status =
      EnterpriseCompanionStatus::FromDeviceManagementStatus(
          policy::DM_STATUS_SUCCESS);
  EXPECT_TRUE(status.ok());
}

TEST(EnterpriseCompanionStatusTest, FromDeviceManagementStatusError) {
  EnterpriseCompanionStatus status =
      EnterpriseCompanionStatus::FromDeviceManagementStatus(
          policy::DM_STATUS_REQUEST_FAILED);
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(
      status.EqualsDeviceManagementStatus(policy::DM_STATUS_REQUEST_FAILED));
}

TEST(EnterpriseCompanionStatusTest, DeviceManagementStatusErrorsEqual) {
  EnterpriseCompanionStatus status1 =
      EnterpriseCompanionStatus::FromDeviceManagementStatus(
          policy::DM_STATUS_REQUEST_FAILED);
  EnterpriseCompanionStatus status2 =
      EnterpriseCompanionStatus::FromDeviceManagementStatus(
          policy::DM_STATUS_REQUEST_FAILED);
  EXPECT_EQ(status1, status2);
}

TEST(EnterpriseCompanionStatusTest, FromCloudPolicyValidationResultSuccess) {
  EnterpriseCompanionStatus status =
      EnterpriseCompanionStatus::FromCloudPolicyValidationResult(
          policy::CloudPolicyValidatorBase::VALIDATION_OK);
  EXPECT_TRUE(status.ok());
}

TEST(EnterpriseCompanionStatusTest, FromCloudPolicyValidationResultError) {
  EnterpriseCompanionStatus status =
      EnterpriseCompanionStatus::FromCloudPolicyValidationResult(
          policy::CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE);
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(status.EqualsCloudPolicyValidationResult(
      policy::CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST(EnterpriseCompanionStatusTest, CloudPolicyValidationResultErrorsEqual) {
  EnterpriseCompanionStatus status1 =
      EnterpriseCompanionStatus::FromCloudPolicyValidationResult(
          policy::CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE);
  EnterpriseCompanionStatus status2 =
      EnterpriseCompanionStatus::FromCloudPolicyValidationResult(
          policy::CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE);
  EXPECT_EQ(status1, status2);
}

TEST(EnterpriseCompanionStatusTest, FromMojomStatusSuccess) {
  EnterpriseCompanionStatus status = EnterpriseCompanionStatus::FromMojomStatus(
      mojom::Status::New(/*space=*/0, /*code=*/0, /*description=*/"Success"));
  EXPECT_TRUE(status.ok());
}

TEST(EnterpriseCompanionStatusTest, FromMojomStatusError) {
  EnterpriseCompanionStatus status =
      EnterpriseCompanionStatus::FromMojomStatus(mojom::Status::New(
          /*space=*/2, /*code=*/0, /*description=*/
          "RegistrationPreconditionFailed"));
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.space(), 2);
  EXPECT_EQ(status.code(), 0);
  EXPECT_EQ(status.description(), "RegistrationPreconditionFailed");
}

TEST(EnterpriseCompanionStatusTest, FromMojomStatusEqual) {
  EnterpriseCompanionStatus status1 =
      EnterpriseCompanionStatus::FromMojomStatus(mojom::Status::New(
          /*space=*/777, /*code=*/42, /*description=*/"description1"));
  EnterpriseCompanionStatus status2 =
      EnterpriseCompanionStatus::FromMojomStatus(mojom::Status::New(
          /*space=*/777, /*code=*/42, /*description=*/"description1"));
  EXPECT_EQ(status1, status2);
}

TEST(EnterpriseCompanionStatusTest, FromMojomStatusEqualsOtherType) {
  EnterpriseCompanionStatus status1 =
      EnterpriseCompanionStatus::FromMojomStatus(mojom::Status::New(
          /*space=*/3,
          /*code=*/static_cast<int>(ApplicationError::kCannotAcquireLock),
          /*description=*/"description1"));
  EnterpriseCompanionStatus status2 =
      EnterpriseCompanionStatus(ApplicationError::kCannotAcquireLock);
  EXPECT_EQ(status1, status2);
}

TEST(EnterpriseCompanionStatusTest, FromProtoStatusSuccess) {
  proto::Status proto_status;
  proto_status.set_space(0);
  proto_status.set_code(0);
  EnterpriseCompanionStatus status =
      EnterpriseCompanionStatus::FromProtoStatus(proto_status);
  EXPECT_TRUE(status.ok());
}

TEST(EnterpriseCompanionStatusTest, FromProtoStatusError) {
  proto::Status proto_status;
  proto_status.set_space(2);
  proto_status.set_code(0);
  EnterpriseCompanionStatus status =
      EnterpriseCompanionStatus::FromProtoStatus(proto_status);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.space(), 2);
  EXPECT_EQ(status.code(), 0);
}

TEST(EnterpriseCompanionStatusTest, FromProtoStatusEqual) {
  proto::Status proto_status_1;
  proto_status_1.set_space(777);
  proto_status_1.set_code(42);
  EnterpriseCompanionStatus status1 =
      EnterpriseCompanionStatus::FromProtoStatus(proto_status_1);

  proto::Status proto_status_2;
  proto_status_2.set_space(777);
  proto_status_2.set_code(42);
  EnterpriseCompanionStatus status2 =
      EnterpriseCompanionStatus::FromProtoStatus(proto_status_2);

  EXPECT_EQ(status1, status2);
}

TEST(EnterpriseCompanionStatusTest, FromProtoStatusEqualsOtherType) {
  proto::Status proto_status_1;
  proto_status_1.set_space(3);
  proto_status_1.set_code(
      static_cast<int>(ApplicationError::kCannotAcquireLock));
  EnterpriseCompanionStatus status1 =
      EnterpriseCompanionStatus::FromProtoStatus(proto_status_1);

  EnterpriseCompanionStatus status2 =
      EnterpriseCompanionStatus(ApplicationError::kCannotAcquireLock);

  EXPECT_EQ(status1, status2);
}

TEST(EnterpriseCompanionStatusTest, FromPosixErrno) {
  EnterpriseCompanionStatus status =
      EnterpriseCompanionStatus::FromPosixErrno(4);
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(status.EqualsPosixErrno(4));
}

TEST(EnterpriseCompanionStatusTest, PosixErrnosEqual) {
  EnterpriseCompanionStatus status1 =
      EnterpriseCompanionStatus::FromPosixErrno(7);
  EnterpriseCompanionStatus status2 =
      EnterpriseCompanionStatus::FromPosixErrno(7);
  EXPECT_EQ(status1, status2);
}

TEST(EnterpriseCompanionStatusTest, DifferentSuccessesEqual) {
  std::vector<EnterpriseCompanionStatus> successes = {
      EnterpriseCompanionStatus::Success(),
      EnterpriseCompanionStatus::FromDeviceManagementStatus(
          policy::DM_STATUS_SUCCESS),
      EnterpriseCompanionStatus::FromCloudPolicyValidationResult(
          policy::CloudPolicyValidatorBase::VALIDATION_OK),
      EnterpriseCompanionStatus::FromMojomStatus(mojom::Status::New(
          /*space=*/0, /*code=*/0, /*description=*/"Success"))};

  ASSERT_TRUE(base::ranges::all_of(
      successes,
      [](const EnterpriseCompanionStatus& status) { return status.ok(); }));
  for (const auto& lhs : successes) {
    for (const auto& rhs : successes) {
      EXPECT_EQ(lhs, rhs);
    }
  }
}

TEST(EnterpriseCompanionStatusTest, DifferentErrorsNotEqual) {
  EnterpriseCompanionStatus status1 =
      EnterpriseCompanionStatus::FromDeviceManagementStatus(
          policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND);
  EnterpriseCompanionStatus status2 =
      EnterpriseCompanionStatus::FromCloudPolicyValidationResult(
          policy::CloudPolicyValidatorBase::VALIDATION_BAD_DM_TOKEN);

  EXPECT_NE(status1, status2);
}

TEST(EnterpriseCompanionStatusTest, SuccessAndErrorNotEqual) {
  EnterpriseCompanionStatus success = EnterpriseCompanionStatus::Success();
  EnterpriseCompanionStatus error1 =
      EnterpriseCompanionStatus::FromCloudPolicyValidationResult(
          policy::CloudPolicyValidatorBase::VALIDATION_BAD_DM_TOKEN);
  EnterpriseCompanionStatus error2 =
      EnterpriseCompanionStatus::FromDeviceManagementStatus(
          policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND);

  EXPECT_NE(success, error1);
  EXPECT_NE(success, error2);
}

}  // namespace enterprise_companion
