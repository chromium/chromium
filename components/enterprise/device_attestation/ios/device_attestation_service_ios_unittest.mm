// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/enterprise/device_attestation/ios/device_attestation_service_ios.h"

#import <cstddef>
#import <memory>
#import <string>
#import <string_view>
#import <utility>

#import "base/base64.h"
#import "base/callback_list.h"
#import "base/memory/raw_ptr.h"
#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/types/expected.h"
#import "components/device_signals/core/common/signals_features.h"
#import "components/enterprise/device_attestation/common/device_attestation_types.h"
#import "components/enterprise/device_attestation/device_attestation_metrics.h"
#import "components/enterprise/device_attestation/device_attestation_service_factory.h"
#import "components/enterprise/device_attestation/ios/attestation_service_ios.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "crypto/hash.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise {

namespace {

constexpr char kTestAttestationBlob[] = "mock_attestation_blob_payload";
constexpr char kFlowName[] = "flow_name";
constexpr char kTimestamp[] = "1713895415";
constexpr char kNonce[] = "some_nonce";
constexpr char kLegacyPayload[] = "legacy_payload_2";
constexpr char kDefaultTimestamp[] = "1700000000";
constexpr char kDefaultNonce[] = "nonce";
constexpr size_t kLargePayloadSize = 1024 * 1024;
constexpr int kContentBindingVersionV1 = 1;
constexpr int kContentBindingVersionV0 = 0;

std::string Sha256Base64(std::string_view payload) {
  return base::Base64Encode(crypto::hash::Sha256(payload));
}

// Test signals constants. These mirror what the iOS signals collector actually
// reports; fields that are never populated on iOS (device name, serial number,
// host name, MAC addresses, DNS servers, and the content analysis connector
// provider lists) are intentionally absent.
constexpr char kDeviceManufacturer[] = "Apple Inc.";
constexpr char kDeviceModel[] = "iPhone14,2";
constexpr char kOSName[] = "iOS";
constexpr char kOSVersion[] = "17.4";
constexpr char kVendorId[] = "fake_vendor_id";
constexpr char kBrowserVersion[] = "149.0.7797.0";
constexpr char kProfileId[] = "profile_id";
constexpr char kProfileEnrollmentDomain[] = "test2.com";
constexpr char kSecurityEventProvider[] = "provider9";

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
  base::HistogramTester histogram_tester_;
  // Declare service_ BEFORE mock_service_ to ensure correct destruction order.
  std::unique_ptr<DeviceAttestationServiceIOS> service_;
  raw_ptr<MockAttestationServiceIOS> mock_service_ = nullptr;
};

// Tests that GetAttestationResponse safely copies `std::string_view` arguments,
// preventing a use-after-free crash when the original strings are destroyed
// before the background task executes.
TEST_F(DeviceAttestationServiceIOSTest,
       GetAttestationResponse_DoesNotCrashOnTemporaryStrings) {
  enterprise_management::ChromeProfileReportRequest report;

  // Explicitly construct and pass a large temporary std::string object for
  // request payload only, to avoid Small String Optimization. This ensures it
  // is allocated on the heap and destroyed at the end of this statement.
  base::test::TestFuture<const AttestationResult&> future;
  service_->GetAttestationResponse(kFlowName, report,
                                   std::string(kLargePayloadSize, 'r'),
                                   kTimestamp, kNonce, future.GetCallback());

  EXPECT_TRUE(future.Wait());
}

// This test validates the hashes that will be used for the content binding
// on iOS when using a specific report with versioning enabled.
TEST_F(DeviceAttestationServiceIOSTest,
       GetAttestationResponse_WithContentBindingVersioning) {
  // DISCLAIMER: If you are updating this test it is because you have modified
  // the version of our report. Any changes should be done behind a flag and
  // reflected on the server.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      enterprise_signals::features::kContentBindingVersioningEnabledIOS);

  // Populate the report the way the shared profile report generator does on
  // iOS. Some of these fields (e.g. disk encryption, screen lock,
  // and most profile signals) are reported but deliberately not part of the V1
  // binding, which the expected payload below reflects.
  enterprise_management::ChromeProfileReportRequest report;

  enterprise_management::OSReport* os_report = report.mutable_os_report();
  os_report->set_device_manufacturer(kDeviceManufacturer);
  os_report->set_device_model(kDeviceModel);
  os_report->set_disk_encryption(enterprise_management::SettingValue::ENABLED);
  os_report->set_name(kOSName);
  os_report->set_screen_lock_secured(
      enterprise_management::SettingValue::ENABLED);
  os_report->set_version(kOSVersion);
  os_report->mutable_ios_specific_attributes()->set_vendor_id(kVendorId);

  enterprise_management::BrowserReport* browser_report =
      report.mutable_browser_report();
  browser_report->set_browser_version(kBrowserVersion);

  enterprise_management::ChromeUserProfileInfo* profile_info =
      browser_report->add_chrome_user_profile_infos();
  profile_info->set_profile_id(kProfileId);

  enterprise_management::ProfileSignalsReport* profile_signals =
      profile_info->mutable_profile_signals_report();
  profile_signals->set_password_protection_warning_trigger(
      enterprise_management::ProfileSignalsReport::PASSWORD_REUSE);
  profile_signals->set_profile_enrollment_domain(kProfileEnrollmentDomain);
  profile_signals->set_realtime_url_check_mode(
      enterprise_management::ProfileSignalsReport::ENABLED_MAIN_FRAME);
  profile_signals->set_safe_browsing_protection_level(
      enterprise_management::ProfileSignalsReport::ENHANCED_PROTECTION);
  profile_signals->set_site_isolation_enabled(true);
  profile_signals->add_security_event_providers(kSecurityEventProvider);

  base::test::TestFuture<const AttestationResult&> future;
  service_->GetAttestationResponse(kFlowName, report,
                                   /*legacy_request_payload=*/kLegacyPayload,
                                   kTimestamp, kNonce, future.GetCallback());

  const AttestationResult& result = future.Get();
  EXPECT_EQ(result.blob_generation_result.attestation_blob,
            kTestAttestationBlob);
  EXPECT_TRUE(result.blob_generation_result.error_message.empty());
  EXPECT_EQ(result.content_binding_version, kContentBindingVersionV1);

  // The hashes are intentionally hardcoded rather than computed so they can be
  // compared directly against the server-side tests. Each one is the base64
  // encoded SHA-256 of its input; `request_hash` hashes
  // `<payload>.<timestamp>.<nonce>`, where the payload is the V1 serialized
  // JSON of the report populated above:
  //   {"browser_version":"149.0.7797.0","device_manufacturer":"Apple Inc.",
  //    "device_model":"iPhone14,2","operating_system":"iOS",
  //    "os_version":"17.4","profile_enrollment_domain":"test2.com",
  //    "profile_id":"profile_id","realtime_url_check_mode":1,
  //    "safe_browsing_protection_level":2,
  //    "security_event_providers":["provider9"],"vendor_id":"fake_vendor_id",
  //    "version":1}
  EXPECT_EQ(mock_service_->last_content_binding(),
            AttestationServiceIOS::ContentBinding({
                {"nonce_hash", "JZwzQsABfEiksUVXG+BiJ5Cmqa2YxeY9utdCViMtSMo="},
                {"request_hash",
                 "PmSzfFjUpGTUV3a9aKJAnHvBcxKdBzxtoq5UgRD4Njk="},
                {"timestamp_hash",
                 "shloxL+uLwC7hyH5SxqtLSP0JylONKpUn8LJWo56XeU="},
            }));

  // A successful generation records the request, the success result, the blob
  // size and the success latency, but no failure latency.
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Attestation.Requested", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Attestation.Result",
      AttestationBlobResult::kSuccess, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Attestation.BlobSize",
      std::string_view(kTestAttestationBlob).size(), 1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Success.Latency", 1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Failure.Latency", 0);
}

// This test validates that when content binding versioning is disabled, the raw
// browser version and profile ID are used for the content binding on iOS.
TEST_F(DeviceAttestationServiceIOSTest,
       GetAttestationResponse_WithoutContentBindingVersioning) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      enterprise_signals::features::kContentBindingVersioningEnabledIOS);

  enterprise_management::ChromeProfileReportRequest report;
  enterprise_management::BrowserReport* browser_report =
      report.mutable_browser_report();
  browser_report->set_browser_version(kBrowserVersion);
  enterprise_management::ChromeUserProfileInfo* profile_info =
      browser_report->add_chrome_user_profile_infos();
  profile_info->set_profile_id(kProfileId);

  base::test::TestFuture<const AttestationResult&> future;
  service_->GetAttestationResponse(kFlowName, report,
                                   /*legacy_request_payload=*/kLegacyPayload,
                                   kTimestamp, kNonce, future.GetCallback());

  const AttestationResult& result = future.Get();
  EXPECT_EQ(result.blob_generation_result.attestation_blob,
            kTestAttestationBlob);
  EXPECT_TRUE(result.blob_generation_result.error_message.empty());
  EXPECT_EQ(result.content_binding_version, kContentBindingVersionV0);

  const AttestationServiceIOS::ContentBinding& content_binding =
      mock_service_->last_content_binding();
  EXPECT_EQ(content_binding.size(), 2u);
  EXPECT_EQ(content_binding.at("browser_version"), kBrowserVersion);
  EXPECT_EQ(content_binding.at("profile_id"), kProfileId);
}

// Tests that changing inputs (timestamp, nonce, or report payload) changes the
// respective content binding hashes as expected.
TEST_F(DeviceAttestationServiceIOSTest,
       GetAttestationResponse_HashesChangeWhenInputsChange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      enterprise_signals::features::kContentBindingVersioningEnabledIOS);

  enterprise_management::ChromeProfileReportRequest report;
  report.mutable_os_report()->set_version("17.4");

  // Base request.
  base::test::TestFuture<const AttestationResult&> future_base;
  service_->GetAttestationResponse(kFlowName, report,
                                   /*legacy_request_payload=*/"", "1713895415",
                                   "nonce_a", future_base.GetCallback());
  EXPECT_TRUE(future_base.Wait());
  AttestationServiceIOS::ContentBinding base_binding =
      mock_service_->last_content_binding();

  // Changing timestamp should change timestamp_hash and request_hash, but not
  // nonce_hash.
  base::test::TestFuture<const AttestationResult&> future_ts;
  service_->GetAttestationResponse(kFlowName, report,
                                   /*legacy_request_payload=*/"", "1713895416",
                                   "nonce_a", future_ts.GetCallback());
  EXPECT_TRUE(future_ts.Wait());
  AttestationServiceIOS::ContentBinding ts_binding =
      mock_service_->last_content_binding();
  EXPECT_NE(ts_binding.at("timestamp_hash"), base_binding.at("timestamp_hash"));
  EXPECT_NE(ts_binding.at("request_hash"), base_binding.at("request_hash"));
  EXPECT_EQ(ts_binding.at("nonce_hash"), base_binding.at("nonce_hash"));
  EXPECT_EQ(ts_binding.at("timestamp_hash"), Sha256Base64("1713895416"));

  // Changing nonce should change nonce_hash and request_hash, but not
  // timestamp_hash.
  base::test::TestFuture<const AttestationResult&> future_nonce;
  service_->GetAttestationResponse(kFlowName, report,
                                   /*legacy_request_payload=*/"", "1713895415",
                                   "nonce_b", future_nonce.GetCallback());
  EXPECT_TRUE(future_nonce.Wait());
  AttestationServiceIOS::ContentBinding nonce_binding =
      mock_service_->last_content_binding();
  EXPECT_NE(nonce_binding.at("nonce_hash"), base_binding.at("nonce_hash"));
  EXPECT_NE(nonce_binding.at("request_hash"), base_binding.at("request_hash"));
  EXPECT_EQ(nonce_binding.at("timestamp_hash"),
            base_binding.at("timestamp_hash"));
  EXPECT_EQ(nonce_binding.at("nonce_hash"), Sha256Base64("nonce_b"));

  // Changing a bound report signal should change request_hash, but not
  // timestamp_hash or nonce_hash.
  enterprise_management::ChromeProfileReportRequest modified_report;
  modified_report.mutable_os_report()->set_version("17.5");
  base::test::TestFuture<const AttestationResult&> future_report;
  service_->GetAttestationResponse(kFlowName, modified_report,
                                   /*legacy_request_payload=*/"", "1713895415",
                                   "nonce_a", future_report.GetCallback());
  EXPECT_TRUE(future_report.Wait());
  AttestationServiceIOS::ContentBinding report_binding =
      mock_service_->last_content_binding();
  EXPECT_NE(report_binding.at("request_hash"), base_binding.at("request_hash"));
  EXPECT_EQ(report_binding.at("timestamp_hash"),
            base_binding.at("timestamp_hash"));
  EXPECT_EQ(report_binding.at("nonce_hash"), base_binding.at("nonce_hash"));

  // Changing a report field that is not part of the iOS signal list (device
  // name is not collected on iOS) must not affect the binding at all.
  enterprise_management::ChromeProfileReportRequest unbound_report = report;
  unbound_report.mutable_browser_device_identifier()->set_computer_name(
      "device_name");
  base::test::TestFuture<const AttestationResult&> future_unbound;
  service_->GetAttestationResponse(kFlowName, unbound_report,
                                   /*legacy_request_payload=*/"", "1713895415",
                                   "nonce_a", future_unbound.GetCallback());
  EXPECT_TRUE(future_unbound.Wait());
  EXPECT_EQ(mock_service_->last_content_binding(), base_binding);
}

TEST_F(DeviceAttestationServiceIOSTest,
       GetAttestationResponse_ConcurrentRequests) {
  enterprise_management::ChromeProfileReportRequest report;

  base::test::TestFuture<const AttestationResult&> future1;
  base::test::TestFuture<const AttestationResult&> future2;

  service_->GetAttestationResponse(kFlowName, report,
                                   /*legacy_request_payload=*/"",
                                   kDefaultTimestamp, kDefaultNonce,
                                   future1.GetCallback());
  service_->GetAttestationResponse(kFlowName, report,
                                   /*legacy_request_payload=*/"",
                                   kDefaultTimestamp, kDefaultNonce,
                                   future2.GetCallback());

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
      kFlowName, report, /*legacy_request_payload=*/"", kDefaultTimestamp,
      kDefaultNonce, future.GetCallback());

  const AttestationResult& result = future.Get();
  EXPECT_TRUE(result.blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(result.blob_generation_result.error_message,
            "Attestation service unavailable");

  // No generation was attempted, so the result is recorded but no latency is:
  // a zero-duration sample here would skew the failure latency distribution.
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Attestation.Requested", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Attestation.Result",
      AttestationBlobResult::kServiceUnavailable, 1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.BlobSize", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Success.Latency", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Failure.Latency", 0);
}

TEST_F(DeviceAttestationServiceIOSTest, SnapshotGenerationFailureReturnsError) {
  auto error_service = std::make_unique<ErrorAttestationServiceIOS>(
      AttestationServiceIOS::AttestationError::kSnapshotGenerationFailed);
  std::unique_ptr<DeviceAttestationServiceIOS> failing_service =
      std::make_unique<DeviceAttestationServiceIOS>(std::move(error_service));
  enterprise_management::ChromeProfileReportRequest report;

  base::test::TestFuture<const AttestationResult&> future;
  failing_service->GetAttestationResponse(
      kFlowName, report, /*legacy_request_payload=*/"", kDefaultTimestamp,
      kDefaultNonce, future.GetCallback());

  const AttestationResult& result = future.Get();
  EXPECT_TRUE(result.blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(result.blob_generation_result.error_message,
            "Attestation snapshot generation failed");

  // Generation was attempted and failed, so both the result and the failure
  // latency are recorded, but no blob size.
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Attestation.Requested", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Attestation.Result",
      AttestationBlobResult::kGenerationFailed, 1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.BlobSize", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Success.Latency", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Failure.Latency", 1);
}

class DeviceAttestationServiceIOSErrorTest
    : public DeviceAttestationServiceIOSTest,
      public testing::WithParamInterface<AttestationErrorTestCase> {};

TEST_P(DeviceAttestationServiceIOSErrorTest,
       AttestationErrorsReturnExpectedMessages) {
  const AttestationErrorTestCase& test_case = GetParam();
  std::unique_ptr<DeviceAttestationServiceIOS> service =
      std::make_unique<DeviceAttestationServiceIOS>(
          std::make_unique<ErrorAttestationServiceIOS>(test_case.error));
  enterprise_management::ChromeProfileReportRequest report;
  base::test::TestFuture<const AttestationResult&> future;
  service->GetAttestationResponse(kFlowName, report,
                                  /*legacy_request_payload=*/"",
                                  kDefaultTimestamp, kDefaultNonce,
                                  future.GetCallback());
  const AttestationResult& result = future.Get();
  EXPECT_TRUE(result.blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(result.blob_generation_result.error_message,
            test_case.expected_message);
}

INSTANTIATE_TEST_SUITE_P(All,
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
      kFlowName, report, /*legacy_request_payload=*/"", kDefaultTimestamp,
      kDefaultNonce, future.GetCallback());

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

  std::unique_ptr<AsyncAttestationServiceIOS> async_mock =
      std::make_unique<AsyncAttestationServiceIOS>();
  AsyncAttestationServiceIOS* raw_async_mock = async_mock.get();
  std::unique_ptr<DeviceAttestationServiceIOS> async_service =
      std::make_unique<DeviceAttestationServiceIOS>(std::move(async_mock));
  enterprise_management::ChromeProfileReportRequest report;

  base::test::TestFuture<const AttestationResult&> future;
  async_service->GetAttestationResponse(
      kFlowName, report, /*legacy_request_payload=*/"", kDefaultTimestamp,
      kDefaultNonce, future.GetCallback());

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

  std::unique_ptr<MockAttestationServiceIOS> local_mock =
      std::make_unique<MockAttestationServiceIOS>();
  std::unique_ptr<DeviceAttestationServiceIOS> local_service =
      std::make_unique<DeviceAttestationServiceIOS>(std::move(local_mock));

  local_service->GetAttestationResponse(
      kFlowName, report, /*legacy_request_payload=*/"", kDefaultTimestamp,
      kDefaultNonce,
      base::BindLambdaForTesting(
          [&](const AttestationResult& result) { local_service.reset(); }));

  EXPECT_EQ(local_service, nullptr);
}

TEST_F(DeviceAttestationServiceIOSTest, FactoryCreatesServiceWithProvider) {
  enterprise_management::ChromeProfileReportRequest report;

  DeviceAttestationServiceFactory::SetAttestationServiceIOSProvider(
      base::BindRepeating([]() -> std::unique_ptr<AttestationServiceIOS> {
        return std::make_unique<MockAttestationServiceIOS>();
      }));

  std::unique_ptr<DeviceAttestationService> service =
      DeviceAttestationServiceFactory::GetInstance()
          ->CreateDeviceAttestationService();
  ASSERT_NE(service, nullptr);

  base::test::TestFuture<const AttestationResult&> future;
  service->GetAttestationResponse(kFlowName, report,
                                  /*legacy_request_payload=*/"",
                                  kDefaultTimestamp, kDefaultNonce,
                                  future.GetCallback());

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
  service->GetAttestationResponse(kFlowName, report,
                                  /*legacy_request_payload=*/"",
                                  kDefaultTimestamp, kDefaultNonce,
                                  future.GetCallback());

  const AttestationResult& result = future.Get();
  EXPECT_TRUE(result.blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(result.blob_generation_result.error_message,
            "Attestation service unavailable");
}

}  // namespace enterprise
