// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/attestation_flow_integrated.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/attestation/attestation_flow_factory.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace attestation {

namespace {

using testing::_;
using testing::SaveArg;
using testing::StrictMock;

}  // namespace

class AttestationFlowIntegratedTest : public testing::Test {
 public:
  AttestationFlowIntegratedTest() { AttestationClient::InitializeFake(); }
  ~AttestationFlowIntegratedTest() override { AttestationClient::Shutdown(); }
  void QuitRunLoopCertificateCallback(
      AttestationFlowIntegrated::CertificateCallback callback,
      AttestationStatus status,
      const std::string& cert) {
    run_loop_->Quit();
    if (callback)
      std::move(callback).Run(status, cert);
  }

 protected:
  void AllowlistCertificateRequest(
      ::attestation::ACAType aca_type,
      ::attestation::GetCertificateRequest request) {
    request.set_aca_type(aca_type);
    AttestationClient::Get()->GetTestInterface()->AllowlistCertificateRequest(
        request);
  }
  void Run() {
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop_->Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<base::RunLoop> run_loop_;
  base::HistogramTester histogram_tester_;
};

// Same as `AttestationFlowIntegratedTest` except this used to run death tests
// in isolated processes.
class AttestationFlowIntegratedDeathTest
    : public AttestationFlowIntegratedTest {
 public:
  AttestationFlowIntegratedDeathTest() = default;
  AttestationFlowIntegratedDeathTest(
      const AttestationFlowIntegratedDeathTest&) = delete;
  AttestationFlowIntegratedDeathTest& operator=(
      const AttestationFlowIntegratedDeathTest&) = delete;
  ~AttestationFlowIntegratedDeathTest() override = default;
};

TEST_F(AttestationFlowIntegratedTest, GetCertificate) {
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");
  request.set_key_type(::attestation::KEY_TYPE_RSA);

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback1,
      callback2, callback3;
  std::string certificate1, certificate2, certificate3;
  EXPECT_CALL(callback1, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate1));
  EXPECT_CALL(callback2, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate2));
  EXPECT_CALL(callback3, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate3));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/AccountId::FromUserEmail(request.username()),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/std::nullopt, callback1.Get());
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/AccountId::FromUserEmail(request.username()),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/std::nullopt, /*callback=*/callback2.Get());
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/AccountId::FromUserEmail(request.username()),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/false, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/std::nullopt,
      /*callback=*/
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback3.Get()));
  Run();
  EXPECT_FALSE(certificate1.empty());
  EXPECT_FALSE(certificate2.empty());
  EXPECT_NE(certificate1, certificate2);
  EXPECT_EQ(certificate2, certificate3);
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 3);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateWithECC) {
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");
  request.set_key_type(::attestation::KEY_TYPE_ECC);

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback1,
      callback2, callback3;
  std::string certificate1, certificate2, certificate3;
  EXPECT_CALL(callback1, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate1));
  EXPECT_CALL(callback2, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate2));
  EXPECT_CALL(callback3, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate3));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/AccountId::FromUserEmail(request.username()),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_ECC,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/std::nullopt, /*callback=*/callback1.Get());
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/AccountId::FromUserEmail(request.username()),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_ECC,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/std::nullopt, /*callback=*/callback2.Get());
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/AccountId::FromUserEmail(request.username()),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/false, /*key_crypto_type=*/::attestation::KEY_TYPE_ECC,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/std::nullopt,
      /*callback=*/
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback3.Get()));
  Run();
  EXPECT_FALSE(certificate1.empty());
  EXPECT_FALSE(certificate2.empty());
  EXPECT_NE(certificate1, certificate2);
  EXPECT_EQ(certificate2, certificate3);
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 3);
}

// This is pretty much identical to `GetCertificate` while the flow under test
// is created by the factory function to make sure that the factory function
// instantiates an object of the intended type.
TEST_F(AttestationFlowIntegratedTest, GetCertificateCreatedByFactory) {
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");
  request.set_key_type(::attestation::KEY_TYPE_RSA);

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback1,
      callback2, callback3;
  std::string certificate1, certificate2, certificate3;
  EXPECT_CALL(callback1, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate1));
  EXPECT_CALL(callback2, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate2));
  EXPECT_CALL(callback3, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate3));

  AttestationFlowFactory attestation_flow_factory;
  // `AttestationFlowIntegrated` doesn't use `ServerProxy`. Create the factory
  // with a strict mock of `ServerProxy` so we can catch unexpected calls.
  attestation_flow_factory.Initialize(
      std::make_unique<StrictMock<MockServerProxy>>());
  AttestationFlow* flow = attestation_flow_factory.GetDefault();
  flow->GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*force_new_key=*/true, ::attestation::KEY_TYPE_RSA, request.key_label(),
      std::nullopt, callback1.Get());
  flow->GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*force_new_key=*/true, ::attestation::KEY_TYPE_RSA, request.key_label(),
      std::nullopt, callback2.Get());
  flow->GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*force_new_key=*/false, ::attestation::KEY_TYPE_RSA, request.key_label(),
      std::nullopt,
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback3.Get()));
  Run();
  EXPECT_FALSE(certificate1.empty());
  EXPECT_FALSE(certificate2.empty());
  EXPECT_NE(certificate1, certificate2);
  EXPECT_EQ(certificate2, certificate3);
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 3);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateFailed) {
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");
  request.set_key_type(::attestation::KEY_TYPE_RSA);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  AttestationStatus status = AttestationStatus::ATTESTATION_SUCCESS;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(SaveArg<0>(&status));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/AccountId::FromUserEmail(request.username()),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/std::nullopt,
      /*callback=*/
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_NE(status, AttestationStatus::ATTESTATION_SUCCESS);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 0);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Attestation.GetCertificateStatus", 1);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateFailedInvalidProfile) {
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::CAST_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");
  request.set_key_type(::attestation::KEY_TYPE_RSA);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  AttestationStatus status = AttestationStatus::ATTESTATION_SUCCESS;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(SaveArg<0>(&status));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/AccountId::FromUserEmail(request.username()),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/std::nullopt,
      /*callback=*/
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_NE(status, AttestationStatus::ATTESTATION_SUCCESS);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Attestation.GetCertificateStatus", 0);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateAttestationNotPrepared) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparationsSequence({false, true});

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");
  request.set_key_type(::attestation::KEY_TYPE_RSA);

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  std::string certificate;
  EXPECT_CALL(callback, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate));

  AttestationFlowIntegrated flow;
  flow.set_retry_delay_for_testing(base::Milliseconds(10));
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/AccountId::FromUserEmail(request.username()),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/std::nullopt,
      /*callback=*/
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_FALSE(certificate.empty());
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 1);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateAttestationNeverPrepared) {
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      false);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");
  request.set_key_type(::attestation::KEY_TYPE_RSA);

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  AttestationStatus status = AttestationStatus::ATTESTATION_SUCCESS;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(SaveArg<0>(&status));

  AttestationFlowIntegrated flow;
  flow.set_ready_timeout_for_testing(base::Milliseconds(10));
  flow.set_retry_delay_for_testing(base::Milliseconds(3));
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/AccountId::FromUserEmail(request.username()),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/std::nullopt,
      /*callback=*/
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_NE(status, AttestationStatus::ATTESTATION_SUCCESS);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Attestation.GetCertificateStatus", 0);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateAttestationNotAvailable) {
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      false);

  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_features_reply()
      ->set_is_available(false);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");
  request.set_key_type(::attestation::KEY_TYPE_RSA);

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  AttestationStatus status = AttestationStatus::ATTESTATION_SUCCESS;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(SaveArg<0>(&status));

  AttestationFlowIntegrated flow;
  flow.set_ready_timeout_for_testing(base::Milliseconds(10));
  flow.set_retry_delay_for_testing(base::Milliseconds(3));
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/AccountId::FromUserEmail(request.username()),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/std::nullopt,
      /*callback=*/
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_EQ(status, AttestationStatus::ATTESTATION_NOT_AVAILABLE);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Attestation.GetCertificateStatus", 0);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateAttestationTestAca) {
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");
  request.set_key_type(::attestation::KEY_TYPE_RSA);

  AllowlistCertificateRequest(::attestation::ACAType::TEST_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  std::string certificate;
  EXPECT_CALL(callback, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate));

  AttestationFlowIntegrated flow(::attestation::ACAType::TEST_ACA);
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/AccountId::FromUserEmail(request.username()),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/std::nullopt,
      /*callback=*/
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_FALSE(certificate.empty());
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 1);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateAcaTypeFromCommandline) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(chromeos::switches::kAttestationServer,
                                  "test");
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");
  request.set_key_type(::attestation::KEY_TYPE_RSA);

  AllowlistCertificateRequest(::attestation::ACAType::TEST_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  std::string certificate;
  EXPECT_CALL(callback, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/AccountId::FromUserEmail(request.username()),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/std::nullopt,
      /*callback=*/
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_FALSE(certificate.empty());
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 1);
}

TEST_F(AttestationFlowIntegratedTest, GetMachineCertificate) {
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_MACHINE_CERTIFICATE);
  request.set_key_label("label");
  request.set_request_origin("origin");
  request.set_key_type(::attestation::KEY_TYPE_RSA);

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  std::string certificate;
  EXPECT_CALL(callback, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/EmptyAccountId(),
      /*request_origin=*/request.request_origin(), /*force_new_key=*/true,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/request.key_label(), /*profile_specific_data=*/std::nullopt,
      /*callback=*/
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_FALSE(certificate.empty());
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 1);
}

// Same as GetCertificate test, but for `DEVICE_SETUP_CERTIFICATE`.
TEST_F(AttestationFlowIntegratedTest, GetDeviceSetupCertificate) {
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  const std::string kId = "random_id";
  const std::string kContentBinding = "content_binding";

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::DEVICE_SETUP_CERTIFICATE);
  request.set_key_label("label");
  request.set_request_origin("origin");
  request.set_key_type(::attestation::KEY_TYPE_RSA);
  request.mutable_device_setup_certificate_request_metadata()->set_id(kId);
  request.mutable_device_setup_certificate_request_metadata()
      ->set_content_binding(kContentBinding);

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback1,
      callback2, callback3;
  std::string certificate1, certificate2, certificate3;
  EXPECT_CALL(callback1, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate1));
  EXPECT_CALL(callback2, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate2));
  EXPECT_CALL(callback3, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate3));

  ::attestation::DeviceSetupCertificateRequestMetadata profile_specific_data;
  profile_specific_data.set_id(kId);
  profile_specific_data.set_content_binding(kContentBinding);
  auto optional_profile_specific_data = std::make_optional(
      AttestationFlow::CertProfileSpecificData(profile_specific_data));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/EmptyAccountId(),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/optional_profile_specific_data,
      /*callback=*/callback1.Get());
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/EmptyAccountId(),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/optional_profile_specific_data,
      /*callback=*/callback2.Get());
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/EmptyAccountId(),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/false, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/optional_profile_specific_data,
      /*callback=*/
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback3.Get()));
  Run();
  EXPECT_FALSE(certificate1.empty());
  EXPECT_FALSE(certificate2.empty());
  EXPECT_NE(certificate1, certificate2);
  EXPECT_EQ(certificate2, certificate3);
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 3);
}

// There used to be an incidence that a non-empty username are sent when
// requesting a device key certificate, and we remove the username in the
// attestation flow process though it is not considered a valid input.
// TODO(b/179364923): Develop a better API design along with strict assertion
// instead of silently removing the username.
TEST_F(AttestationFlowIntegratedTest, GetMachineCertificateWithAccountId) {
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_MACHINE_CERTIFICATE);
  request.set_key_label("label");
  request.set_request_origin("origin");
  request.set_key_type(::attestation::KEY_TYPE_RSA);

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  std::string certificate;
  EXPECT_CALL(callback, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/AccountId::FromUserEmail("username@gmail.com"),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/std::nullopt,
      /*callback=*/
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_FALSE(certificate.empty());
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 1);
}

TEST_F(AttestationFlowIntegratedDeathTest,
       GetDeviceSetupCertificateWithIncorrectParams) {
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::DEVICE_SETUP_CERTIFICATE);
  // Do not set up profile_specific_data value.
  request.set_key_label("label");
  request.set_request_origin("origin");
  request.set_key_type(::attestation::KEY_TYPE_RSA);

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  AttestationFlowIntegrated flow;
  // `profile_specific_data` is `std::nullopt`.
  flow.GetCertificate(
      /*certificate_profile=*/static_cast<AttestationCertificateProfile>(
          request.certificate_profile()),
      /*account_id=*/EmptyAccountId(),
      /*request_origin=*/request.request_origin(),
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/request.key_label(),
      /*profile_specific_data=*/std::nullopt,
      /*callback=*/
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), base::DoNothing()));
  EXPECT_DCHECK_DEATH(Run());
}

}  // namespace attestation
}  // namespace ash
