// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/ios/device_attestation_service_ios.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/enterprise/device_attestation/common/device_attestation_types.h"
#include "components/enterprise/device_attestation/device_attestation_service_factory.h"
#include "components/enterprise/device_attestation/ios/attestation_service_ios.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace enterprise {

namespace {

constexpr char kTestAttestationBlob[] = "mock_attestation_blob_payload";

class MockAttestationServiceIOS : public AttestationServiceIOS {
 public:
  MockAttestationServiceIOS() = default;
  ~MockAttestationServiceIOS() override = default;

  base::CallbackListSubscription Initialize(
      InitializeCallback callback) override {
    std::move(callback).Run(std::nullopt);
    return base::CallbackListSubscription();
  }

  bool IsReady() override { return true; }

  base::CallbackListSubscription GetSnapshot(
      const ContentBinding& content_binding,
      SnapshotCallback callback) override {
    last_content_binding_ = content_binding;
    std::move(callback).Run(kTestAttestationBlob);
    return base::CallbackListSubscription();
  }

  const ContentBinding& last_content_binding() const {
    return last_content_binding_;
  }

 private:
  ContentBinding last_content_binding_;
};

class ErrorAttestationServiceIOS : public AttestationServiceIOS {
 public:
  explicit ErrorAttestationServiceIOS(AttestationError error) : error_(error) {}
  ~ErrorAttestationServiceIOS() override = default;

  base::CallbackListSubscription Initialize(
      InitializeCallback callback) override {
    return base::CallbackListSubscription();
  }

  bool IsReady() override { return true; }

  base::CallbackListSubscription GetSnapshot(
      const ContentBinding& content_binding,
      SnapshotCallback callback) override {
    std::move(callback).Run(base::unexpected(error_));
    return base::CallbackListSubscription();
  }

 private:
  AttestationError error_;
};

struct AttestationErrorTestCase {
  AttestationServiceIOS::AttestationError error;
  const char* expected_message;
};

constexpr AttestationErrorTestCase kAttestationErrorTestCases[] = {
    {AttestationServiceIOS::AttestationError::kServiceUnavailable,
     "Attestation service unavailable"},
    {AttestationServiceIOS::AttestationError::kNetworkError,
     "Attestation challenge fetch network error"},
    {AttestationServiceIOS::AttestationError::kTimeout,
     "Attestation challenge fetch timed out"},
    {AttestationServiceIOS::AttestationError::kClientError,
     "Attestation challenge fetch client error"},
    {AttestationServiceIOS::AttestationError::kServerError,
     "Attestation challenge fetch server error"},
    {AttestationServiceIOS::AttestationError::kResponseParsingFailed,
     "Attestation challenge response parsing failed"},
    {AttestationServiceIOS::AttestationError::kNotInitialized,
     "Attestation service not initialized"},
    {AttestationServiceIOS::AttestationError::kSnapshotGenerationFailed,
     "Attestation snapshot generation failed"},
    {AttestationServiceIOS::AttestationError::kUnknown,
     "Unknown attestation error"},
};

}  // namespace

class DeviceAttestationServiceIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    std::unique_ptr<MockAttestationServiceIOS> mock_service =
        std::make_unique<MockAttestationServiceIOS>();
    mock_service_ = mock_service.get();
    service_ =
        std::make_unique<DeviceAttestationServiceIOS>(std::move(mock_service));
  }

  void TearDown() override {
    mock_service_ = nullptr;
    service_.reset();
    PlatformTest::TearDown();
  }

  base::test::TaskEnvironment task_environment_;
  // Declare service_ BEFORE mock_service_ to ensure correct destruction order.
  std::unique_ptr<DeviceAttestationServiceIOS> service_;
  raw_ptr<MockAttestationServiceIOS> mock_service_ = nullptr;
};

TEST_F(DeviceAttestationServiceIOSTest, GetAttestationResponse_Success) {
  enterprise_management::ChromeProfileReportRequest report;

  base::test::TestFuture<const AttestationResult&> future;
  service_->GetAttestationResponse("flow_name", report,
                                   /*legacy_request_payload=*/"", "1700000000",
                                   "nonce", future.GetCallback());

  const AttestationResult& result = future.Get();
  EXPECT_EQ(result.blob_generation_result.attestation_blob,
            kTestAttestationBlob);
  EXPECT_TRUE(result.blob_generation_result.error_message.empty());
  EXPECT_EQ(result.content_binding_version, 0);
  EXPECT_TRUE(mock_service_->last_content_binding().empty());
}

TEST_F(DeviceAttestationServiceIOSTest,
       GetAttestationResponse_PopulatesContentBinding) {
  enterprise_management::ChromeProfileReportRequest report;
  enterprise_management::BrowserReport* browser_report =
      report.mutable_browser_report();
  browser_report->set_browser_version("128.0.6613.85");
  enterprise_management::ChromeUserProfileInfo* profile_info =
      browser_report->add_chrome_user_profile_infos();
  profile_info->set_profile_id("test_profile_id");

  base::test::TestFuture<const AttestationResult&> future;
  service_->GetAttestationResponse("flow_name", report,
                                   /*legacy_request_payload=*/"", "1700000000",
                                   "nonce", future.GetCallback());

  const AttestationResult& result = future.Get();
  EXPECT_EQ(result.blob_generation_result.attestation_blob,
            kTestAttestationBlob);
  EXPECT_TRUE(result.blob_generation_result.error_message.empty());
  EXPECT_EQ(result.content_binding_version, 0);

  const AttestationServiceIOS::ContentBinding& content_binding =
      mock_service_->last_content_binding();
  EXPECT_EQ(content_binding.size(), 2u);
  EXPECT_EQ(content_binding.at("browser_version"), "128.0.6613.85");
  EXPECT_EQ(content_binding.at("profile_id"), "test_profile_id");
}

TEST_F(DeviceAttestationServiceIOSTest,
       GetAttestationResponse_ConcurrentRequests) {
  enterprise_management::ChromeProfileReportRequest report;

  base::test::TestFuture<const AttestationResult&> future1;
  base::test::TestFuture<const AttestationResult&> future2;

  service_->GetAttestationResponse("flow_name", report,
                                   /*legacy_request_payload=*/"", "1700000000",
                                   "nonce", future1.GetCallback());
  service_->GetAttestationResponse("flow_name", report,
                                   /*legacy_request_payload=*/"", "1700000000",
                                   "nonce", future2.GetCallback());

  const AttestationResult& result1 = future1.Get();
  const AttestationResult& result2 = future2.Get();

  EXPECT_EQ(result1.blob_generation_result.attestation_blob,
            kTestAttestationBlob);
  EXPECT_EQ(result2.blob_generation_result.attestation_blob,
            kTestAttestationBlob);
}

TEST_F(DeviceAttestationServiceIOSTest, NullServiceReturnsError) {
  std::unique_ptr<DeviceAttestationServiceIOS> null_service =
      std::make_unique<DeviceAttestationServiceIOS>(nullptr);
  enterprise_management::ChromeProfileReportRequest report;

  base::test::TestFuture<const AttestationResult&> future;
  null_service->GetAttestationResponse(
      "flow_name", report, /*legacy_request_payload=*/"", "1700000000", "nonce",
      future.GetCallback());

  const AttestationResult& result = future.Get();
  EXPECT_TRUE(result.blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(result.blob_generation_result.error_message,
            "Attestation service unavailable");
}

TEST_F(DeviceAttestationServiceIOSTest, SnapshotGenerationFailureReturnsError) {
  std::unique_ptr<DeviceAttestationServiceIOS> failing_service =
      std::make_unique<DeviceAttestationServiceIOS>(
          std::make_unique<ErrorAttestationServiceIOS>(
              AttestationServiceIOS::AttestationError::
                  kSnapshotGenerationFailed));
  enterprise_management::ChromeProfileReportRequest report;

  base::test::TestFuture<const AttestationResult&> future;
  failing_service->GetAttestationResponse(
      "flow_name", report, /*legacy_request_payload=*/"", "1700000000", "nonce",
      future.GetCallback());

  const AttestationResult& result = future.Get();
  EXPECT_TRUE(result.blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(result.blob_generation_result.error_message,
            "Attestation snapshot generation failed");
}

class DeviceAttestationServiceIOSErrorTest
    : public DeviceAttestationServiceIOSTest,
      public testing::WithParamInterface<AttestationErrorTestCase> {};

TEST_P(DeviceAttestationServiceIOSErrorTest,
       AttestationErrorsReturnExpectedMessages) {
  const AttestationErrorTestCase& test_case = GetParam();
  auto service = std::make_unique<DeviceAttestationServiceIOS>(
      std::make_unique<ErrorAttestationServiceIOS>(test_case.error));
  enterprise_management::ChromeProfileReportRequest report;
  base::test::TestFuture<const AttestationResult&> future;
  service->GetAttestationResponse("flow_name", report,
                                  /*legacy_request_payload=*/"", "1700000000",
                                  "nonce", future.GetCallback());
  const AttestationResult& result = future.Get();
  EXPECT_TRUE(result.blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(result.blob_generation_result.error_message,
            test_case.expected_message);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceAttestationServiceIOSErrorTest,
    testing::ValuesIn(kAttestationErrorTestCases));

TEST_F(DeviceAttestationServiceIOSTest,
       SynchronousSnapshotWithSubscriptionDoesNotLeak) {
  class SyncWithSubscriptionAttestationServiceIOS
      : public AttestationServiceIOS {
   public:
    base::CallbackListSubscription Initialize(
        InitializeCallback callback) override {
      return base::CallbackListSubscription();
    }
    bool IsReady() override { return true; }
    base::CallbackListSubscription GetSnapshot(
        const ContentBinding& content_binding,
        SnapshotCallback callback) override {
      std::move(callback).Run(kTestAttestationBlob);
      return callback_list_.Add(base::DoNothing());
    }

   private:
    base::RepeatingClosureList callback_list_;
  };

  std::unique_ptr<DeviceAttestationServiceIOS> sync_service =
      std::make_unique<DeviceAttestationServiceIOS>(
          std::make_unique<SyncWithSubscriptionAttestationServiceIOS>());
  enterprise_management::ChromeProfileReportRequest report;

  base::test::TestFuture<const AttestationResult&> future;
  sync_service->GetAttestationResponse(
      "flow_name", report, /*legacy_request_payload=*/"", "1700000000", "nonce",
      future.GetCallback());

  const AttestationResult& result = future.Get();
  EXPECT_EQ(result.blob_generation_result.attestation_blob,
            kTestAttestationBlob);
  EXPECT_TRUE(result.blob_generation_result.error_message.empty());
  EXPECT_EQ(sync_service->GetNumberOfActiveSubscriptionsForTesting(), 0u);
}

TEST_F(DeviceAttestationServiceIOSTest,
       AsynchronousSnapshotCleansUpSubscription) {
  class AsyncAttestationServiceIOS : public AttestationServiceIOS {
   public:
    base::CallbackListSubscription Initialize(
        InitializeCallback callback) override {
      return base::CallbackListSubscription();
    }
    bool IsReady() override { return true; }
    base::CallbackListSubscription GetSnapshot(
        const ContentBinding& content_binding,
        SnapshotCallback callback) override {
      saved_callback_ = std::move(callback);
      return callback_list_.Add(base::DoNothing());
    }

    void CompleteSnapshot(
        base::expected<std::string, AttestationError> result) {
      std::move(saved_callback_).Run(std::move(result));
    }

   private:
    SnapshotCallback saved_callback_;
    base::RepeatingClosureList callback_list_;
  };

  auto async_mock = std::make_unique<AsyncAttestationServiceIOS>();
  AsyncAttestationServiceIOS* raw_async_mock = async_mock.get();
  std::unique_ptr<DeviceAttestationServiceIOS> async_service =
      std::make_unique<DeviceAttestationServiceIOS>(std::move(async_mock));
  enterprise_management::ChromeProfileReportRequest report;

  base::test::TestFuture<const AttestationResult&> future;
  async_service->GetAttestationResponse(
      "flow_name", report, /*legacy_request_payload=*/"", "1700000000", "nonce",
      future.GetCallback());

  EXPECT_EQ(async_service->GetNumberOfActiveSubscriptionsForTesting(), 1u);
  EXPECT_FALSE(future.IsReady());

  raw_async_mock->CompleteSnapshot(kTestAttestationBlob);

  const AttestationResult& result = future.Get();
  EXPECT_EQ(result.blob_generation_result.attestation_blob,
            kTestAttestationBlob);
  EXPECT_TRUE(result.blob_generation_result.error_message.empty());
  EXPECT_EQ(async_service->GetNumberOfActiveSubscriptionsForTesting(), 0u);
}

TEST_F(DeviceAttestationServiceIOSTest, ServiceDestroyedInsideSyncCallback) {
  enterprise_management::ChromeProfileReportRequest report;

  auto local_mock = std::make_unique<MockAttestationServiceIOS>();
  auto local_service =
      std::make_unique<DeviceAttestationServiceIOS>(std::move(local_mock));

  local_service->GetAttestationResponse(
      "flow_name", report, /*legacy_request_payload=*/"", "1700000000", "nonce",
      base::BindLambdaForTesting(
          [&](const AttestationResult& result) { local_service.reset(); }));

  EXPECT_EQ(local_service, nullptr);
}

TEST_F(DeviceAttestationServiceIOSTest, FactoryCreatesServiceWithProvider) {
  enterprise_management::ChromeProfileReportRequest report;

  DeviceAttestationServiceFactory::SetAttestationServiceIOSProvider(
      base::BindRepeating(
          []() -> std::unique_ptr<AttestationServiceIOS> {
            return std::make_unique<MockAttestationServiceIOS>();
          }));

  std::unique_ptr<DeviceAttestationService> service =
      DeviceAttestationServiceFactory::GetInstance()
          ->CreateDeviceAttestationService();
  ASSERT_NE(service, nullptr);

  base::test::TestFuture<const AttestationResult&> future;
  service->GetAttestationResponse("flow_name", report,
                                   /*legacy_request_payload=*/"", "1700000000",
                                   "nonce", future.GetCallback());

  const AttestationResult& result = future.Get();
  EXPECT_EQ(result.blob_generation_result.attestation_blob,
            kTestAttestationBlob);
  EXPECT_TRUE(result.blob_generation_result.error_message.empty());

  DeviceAttestationServiceFactory::ClearAttestationServiceIOSProvider();
}

TEST_F(DeviceAttestationServiceIOSTest, FactoryCreatesServiceWithoutProvider) {
  DeviceAttestationServiceFactory::ClearAttestationServiceIOSProvider();

  std::unique_ptr<DeviceAttestationService> service =
      DeviceAttestationServiceFactory::GetInstance()
          ->CreateDeviceAttestationService();
  ASSERT_NE(service, nullptr);

  enterprise_management::ChromeProfileReportRequest report;
  base::test::TestFuture<const AttestationResult&> future;
  service->GetAttestationResponse("flow_name", report,
                                   /*legacy_request_payload=*/"", "1700000000",
                                   "nonce", future.GetCallback());

  const AttestationResult& result = future.Get();
  EXPECT_TRUE(result.blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(result.blob_generation_result.error_message,
            "Attestation service unavailable");
}

}  // namespace enterprise