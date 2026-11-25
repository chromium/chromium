// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/ash/device_attestation_service_ash.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/device_signals/core/common/signals_features.h"
#include "components/enterprise/device_attestation/device_attestation_metrics.h"
#include "components/enterprise/device_attestation/device_attestation_service_factory.h"
#include "components/enterprise/device_attestation/device_attestation_utils.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/hash.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise {

class DeviceAttestationServiceAshTest : public testing::Test {
 public:
  DeviceAttestationServiceAshTest()
      : install_attributes_(
            ash::StubInstallAttributes::CreateCloudManaged("example.com",
                                                           "device_id")) {
    ash::AttestationClient::InitializeFake();
    service_ = std::make_unique<DeviceAttestationServiceAsh>();
  }

  ~DeviceAttestationServiceAshTest() override {
    if (ash::AttestationClient::Get()) {
      ash::AttestationClient::Shutdown();
    }
  }

 protected:
  enterprise_management::ChromeProfileReportRequest CreateReport(
      bool is_affiliated) {
    enterprise_management::ChromeProfileReportRequest report;
    auto* profile_info =
        report.mutable_browser_report()->add_chrome_user_profile_infos();
    profile_info->set_id("/home/chronos/u-12345");
    profile_info->set_profile_id("profile_123");
    profile_info->mutable_affiliation()->set_is_affiliated(is_affiliated);
    auto* device_id = report.mutable_browser_device_identifier();
    device_id->set_computer_name("computer_name");
    device_id->set_serial_number("YX02T9XY");
    auto* os_report = report.mutable_os_report();
    os_report->set_name("ChromeOS");
    os_report->set_disk_encryption(
        enterprise_management::SettingValue::ENABLED);
    os_report->set_os_firewall(enterprise_management::SettingValue::ENABLED);
    os_report->set_screen_lock_secured(
        enterprise_management::SettingValue::DISABLED);
    return report;
  }

  // Runs the SignSimpleChallenge reply handler directly. Used for replies that
  // the fake attestation client cannot produce.
  AttestationResult HandleChallengeReply(
      const ::attestation::SignSimpleChallengeReply& reply) {
    base::test::TestFuture<const AttestationResult&> test_future;
    service_->OnChallengeResponseReady(base::TimeTicks::Now(),
                                       test_future.GetCallback(), reply);
    return test_future.Get();
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  ash::ScopedStubInstallAttributes install_attributes_;
  std::unique_ptr<DeviceAttestationServiceAsh> service_;
};

TEST_F(DeviceAttestationServiceAshTest, AffiliatedSuccess_V1ContentBinding) {
  base::test::ScopedFeatureList feature_list{
      enterprise_signals::features::kContentBindingVersioningEnabled};
  enterprise_management::ChromeProfileReportRequest report =
      CreateReport(/*is_affiliated=*/true);

  auto* test_interface = ash::AttestationClient::Get()->GetTestInterface();
  test_interface->set_sign_simple_challenge_status(
      ::attestation::STATUS_SUCCESS);
  test_interface->AllowlistSignSimpleChallengeKey(
      "", ash::attestation::kEnterpriseMachineKey);

  base::test::TestFuture<const AttestationResult&> test_future;
  service_->GetAttestationResponse("flow_name", report, "legacy_payload",
                                   "1713895415", "some_nonce",
                                   test_future.GetCallback());

  ASSERT_TRUE(test_future.Wait());
  const AttestationResult& result = test_future.Get();
  EXPECT_FALSE(result.blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(result.content_binding_version, 1);
  EXPECT_TRUE(result.blob_generation_result.error_message.empty());

  std::string decoded_blob;
  ASSERT_TRUE(base::Base64Decode(result.blob_generation_result.attestation_blob,
                                 &decoded_blob));
  ::attestation::SignedData signed_data;
  ASSERT_TRUE(signed_data.ParseFromString(decoded_blob));

  std::string expected_payload_hash = base::Base64Encode(
      crypto::hash::Sha256(GenerateV1ContentBindingString(report)));
  std::string expected_challenge =
      base::StrCat({"flow_name:some_nonce:1713895415:", expected_payload_hash});
  EXPECT_TRUE(test_interface->VerifySimpleChallengeResponse(expected_challenge,
                                                            signed_data));

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Attestation.Requested", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Attestation.Result",
      AttestationBlobResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.BlobSize", 1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Success.Latency", 1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Failure.Latency", 0);
}

// ChromeOS only supports the V1 content binding, so disabling the feature
// gating the V0 to V1 migration on other platforms must not downgrade it.
TEST_F(DeviceAttestationServiceAshTest,
       AffiliatedSuccess_V1ContentBindingWhenVersioningDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      enterprise_signals::features::kContentBindingVersioningEnabled);
  enterprise_management::ChromeProfileReportRequest report =
      CreateReport(/*is_affiliated=*/true);

  auto* test_interface = ash::AttestationClient::Get()->GetTestInterface();
  test_interface->set_sign_simple_challenge_status(
      ::attestation::STATUS_SUCCESS);
  test_interface->AllowlistSignSimpleChallengeKey(
      "", ash::attestation::kEnterpriseMachineKey);

  base::test::TestFuture<const AttestationResult&> test_future;
  service_->GetAttestationResponse("flow_name", report, "legacy_payload",
                                   "1713895415", "some_nonce",
                                   test_future.GetCallback());

  ASSERT_TRUE(test_future.Wait());
  const AttestationResult& result = test_future.Get();
  EXPECT_FALSE(result.blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(result.content_binding_version, 1);
  EXPECT_TRUE(result.blob_generation_result.error_message.empty());

  std::string decoded_blob;
  ASSERT_TRUE(base::Base64Decode(result.blob_generation_result.attestation_blob,
                                 &decoded_blob));
  ::attestation::SignedData signed_data;
  ASSERT_TRUE(signed_data.ParseFromString(decoded_blob));

  std::string expected_payload_hash = base::Base64Encode(
      crypto::hash::Sha256(GenerateV1ContentBindingString(report)));
  std::string expected_challenge =
      base::StrCat({"flow_name:some_nonce:1713895415:", expected_payload_hash});
  EXPECT_TRUE(test_interface->VerifySimpleChallengeResponse(expected_challenge,
                                                            signed_data));

  std::string legacy_challenge = base::StrCat(
      {"flow_name:some_nonce:1713895415:",
       base::Base64Encode(crypto::hash::Sha256("legacy_payload"))});
  EXPECT_FALSE(test_interface->VerifySimpleChallengeResponse(legacy_challenge,
                                                             signed_data));
}

TEST_F(DeviceAttestationServiceAshTest, UnaffiliatedProfileUnsupported) {
  enterprise_management::ChromeProfileReportRequest report =
      CreateReport(/*is_affiliated=*/false);

  base::test::TestFuture<const AttestationResult&> test_future;
  service_->GetAttestationResponse("flow_name", report, "legacy_payload",
                                   "1713895415", "some_nonce",
                                   test_future.GetCallback());

  ASSERT_TRUE(test_future.Wait());
  EXPECT_TRUE(
      test_future.Get().blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(test_future.Get().blob_generation_result.error_message,
            "Device Attestation is unsupported");

  // Ineligible profiles are not attestation requests: nothing is recorded.
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Requested", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Result", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.BlobSize", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Success.Latency", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Failure.Latency", 0);
}

TEST_F(DeviceAttestationServiceAshTest, MultipleProfilesUnsupported) {
  enterprise_management::ChromeProfileReportRequest report =
      CreateReport(/*is_affiliated=*/true);
  auto* second_profile_info =
      report.mutable_browser_report()->add_chrome_user_profile_infos();
  second_profile_info->set_id("/home/chronos/u-67890");
  second_profile_info->set_profile_id("profile_456");
  second_profile_info->mutable_affiliation()->set_is_affiliated(true);

  base::test::TestFuture<const AttestationResult&> test_future;
  service_->GetAttestationResponse("flow_name", report, "legacy_payload",
                                   "1713895415", "some_nonce",
                                   test_future.GetCallback());

  ASSERT_TRUE(test_future.Wait());
  EXPECT_TRUE(
      test_future.Get().blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(test_future.Get().blob_generation_result.error_message,
            "Device Attestation is unsupported");

  // Ineligible profiles are not attestation requests: nothing is recorded.
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Requested", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Result", 0);
}

TEST_F(DeviceAttestationServiceAshTest, ConsumerOwnedDeviceUnsupported) {
  install_attributes_.Get()->SetConsumerOwned();
  enterprise_management::ChromeProfileReportRequest report =
      CreateReport(/*is_affiliated=*/true);

  base::test::TestFuture<const AttestationResult&> test_future;
  service_->GetAttestationResponse("flow_name", report, "legacy_payload",
                                   "1713895415", "some_nonce",
                                   test_future.GetCallback());

  ASSERT_TRUE(test_future.Wait());
  EXPECT_TRUE(
      test_future.Get().blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(test_future.Get().blob_generation_result.error_message,
            "Device Attestation is unsupported");

  // Ineligible profiles are not attestation requests: nothing is recorded.
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Requested", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Result", 0);
}

TEST_F(DeviceAttestationServiceAshTest, AttestationClientUnavailable) {
  ash::AttestationClient::Shutdown();
  enterprise_management::ChromeProfileReportRequest report =
      CreateReport(/*is_affiliated=*/true);

  base::test::TestFuture<const AttestationResult&> test_future;
  service_->GetAttestationResponse("flow_name", report, "legacy_payload",
                                   "1713895415", "some_nonce",
                                   test_future.GetCallback());

  ASSERT_TRUE(test_future.Wait());
  EXPECT_TRUE(
      test_future.Get().blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(test_future.Get().blob_generation_result.error_message,
            "Attestation client is unavailable");

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Attestation.Requested", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Attestation.Result",
      AttestationBlobResult::kServiceUnavailable, 1);
}

TEST_F(DeviceAttestationServiceAshTest, SignSimpleChallengeFailure) {
  enterprise_management::ChromeProfileReportRequest report =
      CreateReport(/*is_affiliated=*/true);

  auto* test_interface = ash::AttestationClient::Get()->GetTestInterface();
  test_interface->set_sign_simple_challenge_status(
      ::attestation::STATUS_UNEXPECTED_DEVICE_ERROR);
  test_interface->AllowlistSignSimpleChallengeKey(
      "", ash::attestation::kEnterpriseMachineKey);

  base::test::TestFuture<const AttestationResult&> test_future;
  service_->GetAttestationResponse("flow_name", report, "legacy_payload",
                                   "1713895415", "some_nonce",
                                   test_future.GetCallback());

  ASSERT_TRUE(test_future.Wait());
  EXPECT_TRUE(
      test_future.Get().blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(test_future.Get().blob_generation_result.error_message,
            "Failed to sign simple challenge. Status: 1");

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

TEST_F(DeviceAttestationServiceAshTest, SuccessStatusWithEmptyChallenge) {
  ::attestation::SignSimpleChallengeReply reply;
  reply.set_status(::attestation::STATUS_SUCCESS);

  const AttestationResult result = HandleChallengeReply(reply);
  EXPECT_TRUE(result.blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(result.blob_generation_result.error_message,
            "Empty challenge response from attestation service.");
  EXPECT_EQ(result.content_binding_version, 1);

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

// The attestation service reports success by leaving `status` unset, relying
// on the proto default. Such replies must be treated as successful.
TEST_F(DeviceAttestationServiceAshTest, UnsetStatusWithChallengeIsSuccess) {
  ::attestation::SignSimpleChallengeReply reply;
  reply.set_challenge_response("challenge_response");
  ASSERT_FALSE(reply.has_status());

  const AttestationResult result = HandleChallengeReply(reply);
  EXPECT_EQ(result.blob_generation_result.attestation_blob,
            base::Base64Encode("challenge_response"));
  EXPECT_TRUE(result.blob_generation_result.error_message.empty());
  EXPECT_EQ(result.content_binding_version, 1);

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.DeviceSignals.Attestation.Result",
      AttestationBlobResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Success.Latency", 1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Failure.Latency", 0);
}

TEST_F(DeviceAttestationServiceAshTest,
       FactoryCreatesDeviceAttestationServiceAsh) {
  base::test::ScopedFeatureList feature_list{
      enterprise_signals::features::kChromeOsDeviceAttestationEnabled};
  std::unique_ptr<DeviceAttestationService> factory_service =
      DeviceAttestationServiceFactory::GetInstance()
          ->CreateDeviceAttestationService();
  ASSERT_NE(factory_service, nullptr);

  enterprise_management::ChromeProfileReportRequest report =
      CreateReport(/*is_affiliated=*/true);
  auto* test_interface = ash::AttestationClient::Get()->GetTestInterface();
  test_interface->set_sign_simple_challenge_status(
      ::attestation::STATUS_SUCCESS);
  test_interface->AllowlistSignSimpleChallengeKey(
      "", ash::attestation::kEnterpriseMachineKey);

  base::test::TestFuture<const AttestationResult&> test_future;
  factory_service->GetAttestationResponse("flow_name", report, "legacy_payload",
                                          "1713895415", "some_nonce",
                                          test_future.GetCallback());

  ASSERT_TRUE(test_future.Wait());
  EXPECT_FALSE(
      test_future.Get().blob_generation_result.attestation_blob.empty());
  EXPECT_TRUE(test_future.Get().blob_generation_result.error_message.empty());
}

// Disabling the kill switch makes the factory fall back to the no-op service:
// no blob is generated and the attestation daemon is never involved.
TEST_F(DeviceAttestationServiceAshTest,
       FactoryCreatesNoOpServiceWhenAttestationDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      enterprise_signals::features::kChromeOsDeviceAttestationEnabled);
  std::unique_ptr<DeviceAttestationService> factory_service =
      DeviceAttestationServiceFactory::GetInstance()
          ->CreateDeviceAttestationService();
  ASSERT_NE(factory_service, nullptr);

  enterprise_management::ChromeProfileReportRequest report =
      CreateReport(/*is_affiliated=*/true);
  auto* test_interface = ash::AttestationClient::Get()->GetTestInterface();
  test_interface->set_sign_simple_challenge_status(
      ::attestation::STATUS_SUCCESS);
  test_interface->AllowlistSignSimpleChallengeKey(
      "", ash::attestation::kEnterpriseMachineKey);

  base::test::TestFuture<const AttestationResult&> test_future;
  factory_service->GetAttestationResponse("flow_name", report, "legacy_payload",
                                          "1713895415", "some_nonce",
                                          test_future.GetCallback());

  ASSERT_TRUE(test_future.Wait());
  EXPECT_TRUE(
      test_future.Get().blob_generation_result.attestation_blob.empty());
  EXPECT_EQ(test_future.Get().blob_generation_result.error_message,
            "Device Attestation is unsupported");

  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Requested", 0);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.DeviceSignals.Attestation.Result", 0);
}

}  // namespace enterprise
