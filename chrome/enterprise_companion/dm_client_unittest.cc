// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/dm_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_companion {

constexpr char kFakeDeviceId[] = "test-device-id";
constexpr char kFakeEnrollmentToken[] = "TestEnrollmentToken";
constexpr char kFakeDMToken[] = "test-dm-token";

class MockCloudPolicyClient : public policy::MockCloudPolicyClient {
 public:
  explicit MockCloudPolicyClient(policy::DeviceManagementService* service)
      : policy::MockCloudPolicyClient(service) {}

  MOCK_METHOD(void,
              RegisterBrowserWithEnrollmentToken,
              (const std::string& token,
               const std::string& client_id,
               const policy::ClientDataDelegate& client_data_delegate,
               bool is_mandatory),
              (override));
};

class TestTokenService
    : public device_management_storage::TokenServiceInterface {
 public:
  ~TestTokenService() override = default;

  // Overrides for TokenServiceInterface.
  std::string GetDeviceID() const override { return kFakeDeviceId; }

  bool IsEnrollmentMandatory() const override { return false; }

  bool StoreEnrollmentToken(const std::string& enrollment_token) override {
    enrollment_token_ = enrollment_token;
    return true;
  }

  bool DeleteEnrollmentToken() override { return StoreEnrollmentToken(""); }

  std::string GetEnrollmentToken() const override { return enrollment_token_; }

  bool StoreDmToken(const std::string& dm_token) override {
    dm_token_ = dm_token;
    return true;
  }

  bool DeleteDmToken() override {
    dm_token_.clear();
    return true;
  }

  std::string GetDmToken() const override { return dm_token_; }

 private:
  std::string enrollment_token_;
  std::string dm_token_;
};

class DMClientTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(storage_dir_.CreateUniqueTempDir());

    std::unique_ptr<TestTokenService> test_token_service =
        std::make_unique<TestTokenService>();
    test_token_service_ = test_token_service.get();
    mock_cloud_policy_client_ =
        new MockCloudPolicyClient(&fake_device_management_service_);
    dm_storage_ =
        CreateDMStorage(storage_dir_.GetPath(), std::move(test_token_service));
    dm_client_ = CreateDMClient(
        base::BindLambdaForTesting([&](policy::DeviceManagementService*) {
          return base::WrapUnique(static_cast<policy::CloudPolicyClient*>(
              mock_cloud_policy_client_));
        }),
        dm_storage_);
  }

 protected:
  base::test::TaskEnvironment environment_;
  base::ScopedTempDir storage_dir_;
  policy::MockJobCreationHandler mock_job_creation_handler_;
  policy::FakeDeviceManagementService fake_device_management_service_ =
      policy::FakeDeviceManagementService(&mock_job_creation_handler_);
  scoped_refptr<device_management_storage::DMStorage> dm_storage_;
  // |test_token_service_| and |mock_cloud_policy_client_| are pointers to
  // objects owned by |dm_client_|. They must be destructed before the client to
  // avoid raw_ptr from complaining about dangling pointers.
  std::unique_ptr<DMClient> dm_client_;
  raw_ptr<TestTokenService> test_token_service_ = nullptr;
  raw_ptr<MockCloudPolicyClient> mock_cloud_policy_client_ = nullptr;
};

TEST_F(DMClientTest, RegisterDeviceSuccess) {
  test_token_service_->StoreEnrollmentToken(kFakeEnrollmentToken);
  EXPECT_CALL(*mock_cloud_policy_client_,
              RegisterBrowserWithEnrollmentToken(
                  kFakeEnrollmentToken, kFakeDeviceId, testing::_, false))
      .Times(1);

  base::RunLoop run_loop;
  dm_client_->RegisterBrowser(
      base::BindOnce([](const EnterpriseCompanionStatus& status) {
        EXPECT_TRUE(status.ok());
      }).Then(run_loop.QuitClosure()));
  mock_cloud_policy_client_->SetDMToken(kFakeDMToken);
  mock_cloud_policy_client_->NotifyRegistrationStateChanged();
  run_loop.Run();

  EXPECT_EQ(test_token_service_->GetDmToken(), kFakeDMToken);
}

TEST_F(DMClientTest, RegisterDeviceFailure) {
  test_token_service_->StoreEnrollmentToken(kFakeEnrollmentToken);
  EXPECT_CALL(*mock_cloud_policy_client_,
              RegisterBrowserWithEnrollmentToken(
                  kFakeEnrollmentToken, kFakeDeviceId, testing::_, false))
      .Times(1);

  base::RunLoop run_loop;
  dm_client_->RegisterBrowser(
      base::BindOnce([](const EnterpriseCompanionStatus& status) {
        EXPECT_TRUE(status.EqualsDeviceManagementStatus(
            policy::DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER));
      }).Then(run_loop.QuitClosure()));
  mock_cloud_policy_client_->SetStatus(
      policy::DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER);
  mock_cloud_policy_client_->NotifyClientError();
  run_loop.Run();

  EXPECT_TRUE(test_token_service_->GetDmToken().empty());
}

TEST_F(DMClientTest, RegistrationRemovesPolicies) {
  // Store some policies, a DM token must be preset to serialize the data.
  test_token_service_->StoreDmToken(kFakeDMToken);
  ::enterprise_management::PolicyFetchResponse fake_response;
  ::enterprise_management::PolicyData fake_policy_data;
  fake_policy_data.set_policy_value("fake policy value");
  fake_response.set_policy_data(fake_policy_data.SerializeAsString());
  ASSERT_TRUE(dm_storage_->CanPersistPolicies());
  ASSERT_TRUE(dm_storage_->PersistPolicies(
      {{"google/test-policy-type", fake_response.SerializeAsString()}}));
  ASSERT_TRUE(dm_storage_->ReadPolicyData("google/test-policy-type"));

  // Delete the DM token to ensure registration is not skipped.
  test_token_service_->DeleteDmToken();
  test_token_service_->StoreEnrollmentToken(kFakeEnrollmentToken);
  EXPECT_CALL(*mock_cloud_policy_client_,
              RegisterBrowserWithEnrollmentToken(
                  kFakeEnrollmentToken, kFakeDeviceId, testing::_, false))
      .Times(1);

  // Register the device. All policies should be removed as a side effect.
  base::RunLoop run_loop;
  dm_client_->RegisterBrowser(
      base::BindOnce([](const EnterpriseCompanionStatus& status) {
        EXPECT_TRUE(status.ok());
      }).Then(run_loop.QuitClosure()));
  mock_cloud_policy_client_->SetDMToken(kFakeDMToken);
  mock_cloud_policy_client_->NotifyRegistrationStateChanged();
  run_loop.Run();

  EXPECT_FALSE(dm_storage_->ReadPolicyData("google/test-policy-type"));
}

TEST_F(DMClientTest, RegistrationSkippedNoEnrollmentToken) {
  EXPECT_CALL(*mock_cloud_policy_client_, RegisterBrowserWithEnrollmentToken)
      .Times(0);

  base::RunLoop run_loop;
  dm_client_->RegisterBrowser(
      base::BindOnce([](const EnterpriseCompanionStatus& status) {
        EXPECT_TRUE(status.ok());
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(test_token_service_->GetDmToken().empty());
}

TEST_F(DMClientTest, RegistrationSkippedAlreadyManaged) {
  test_token_service_->StoreDmToken(kFakeDMToken);
  EXPECT_CALL(*mock_cloud_policy_client_, RegisterBrowserWithEnrollmentToken)
      .Times(0);

  base::RunLoop run_loop;
  dm_client_->RegisterBrowser(
      base::BindOnce([](const EnterpriseCompanionStatus& status) {
        EXPECT_TRUE(status.ok());
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(test_token_service_->GetDmToken(), kFakeDMToken);
}

TEST_F(DMClientTest, PoliciesPersistedThroughSkippedRegistration) {
  // Store some policies, a DM token must be preset to serialize the data.
  test_token_service_->StoreEnrollmentToken(kFakeEnrollmentToken);
  test_token_service_->StoreDmToken(kFakeDMToken);
  ::enterprise_management::PolicyFetchResponse fake_response;
  ::enterprise_management::PolicyData fake_policy_data;
  fake_policy_data.set_policy_value("fake policy value");
  fake_response.set_policy_data(fake_policy_data.SerializeAsString());
  ASSERT_TRUE(dm_storage_->CanPersistPolicies());
  ASSERT_TRUE(dm_storage_->PersistPolicies(
      {{"google/test-policy-type", fake_response.SerializeAsString()}}));
  ASSERT_TRUE(dm_storage_->ReadPolicyData("google/test-policy-type"));

  // Delete the DM token to ensure registration is not skipped.
  EXPECT_CALL(*mock_cloud_policy_client_, RegisterBrowserWithEnrollmentToken)
      .Times(0);

  // Registration should be skipped as DM token is still present.
  base::RunLoop run_loop;
  dm_client_->RegisterBrowser(
      base::BindOnce([](const EnterpriseCompanionStatus& status) {
        EXPECT_TRUE(status.ok());
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(test_token_service_->GetDmToken(), kFakeDMToken);
  EXPECT_TRUE(dm_storage_->ReadPolicyData("google/test-policy-type"));
}

}  // namespace enterprise_companion
