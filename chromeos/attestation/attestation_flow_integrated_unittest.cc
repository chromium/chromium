// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/attestation/attestation_flow_integrated.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "chromeos/attestation/attestation_flow_utils.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/attestation/attestation_client.h"
#include "chromeos/dbus/attestation/interface.pb.h"
#include "chromeos/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace attestation {

namespace {

using testing::_;
using testing::SaveArg;

}  // namespace

class AttestationFlowIntegratedTest : public testing::Test {
 public:
  AttestationFlowIntegratedTest() {
    chromeos::AttestationClient::InitializeFake();
  }
  ~AttestationFlowIntegratedTest() override {
    chromeos::AttestationClient::Shutdown();
  }
  void QuitRunLoopCertificateCallback(
      AttestationFlowIntegrated::CertificateCallback callback,
      AttestationStatus status,
      const std::string& cert) {
    LOG(WARNING) << "Quitting run loop.";
    run_loop_->Quit();
    if (callback)
      std::move(callback).Run(status, cert);
  }

 protected:
  void AllowlistCertificateRequest(
      ::attestation::ACAType aca_type,
      ::attestation::GetCertificateRequest request) {
    request.set_aca_type(aca_type);
    if (request.key_label().empty()) {
      request.set_key_label(
          GetKeyNameForProfile(static_cast<AttestationCertificateProfile>(
                                   request.certificate_profile()),
                               request.request_origin()));
    }
    chromeos::AttestationClient::Get()
        ->GetTestInterface()
        ->AllowlistCertificateRequest(request);
  }
  void Run() {
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop_->Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop* run_loop_;
};

TEST_F(AttestationFlowIntegratedTest, GetCertificate) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");

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
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(), callback1.Get());
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(), callback2.Get());
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/false, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback3.Get()));
  Run();
  EXPECT_FALSE(certificate1.empty());
  EXPECT_FALSE(certificate2.empty());
  EXPECT_NE(certificate1, certificate2);
  EXPECT_EQ(certificate2, certificate3);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateFailed) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  AttestationStatus status = AttestationStatus::ATTESTATION_SUCCESS;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(SaveArg<0>(&status));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_NE(status, AttestationStatus::ATTESTATION_SUCCESS);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateFailedInvalidProfile) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::CAST_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  AttestationStatus status = AttestationStatus::ATTESTATION_SUCCESS;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(SaveArg<0>(&status));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_NE(status, AttestationStatus::ATTESTATION_SUCCESS);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateAttestationNotPrepared) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparationsSequence({false, true});

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  std::string certificate;
  EXPECT_CALL(callback, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate));

  AttestationFlowIntegrated flow;
  flow.set_retry_delay_for_testing(base::TimeDelta::FromMilliseconds(10));
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_FALSE(certificate.empty());
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateAttestationNeverPrepared) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(false);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  AttestationStatus status = AttestationStatus::ATTESTATION_SUCCESS;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(SaveArg<0>(&status));

  AttestationFlowIntegrated flow;
  flow.set_ready_timeout_for_testing(base::TimeDelta::FromMilliseconds(10));
  flow.set_retry_delay_for_testing(base::TimeDelta::FromMilliseconds(3));
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_NE(status, AttestationStatus::ATTESTATION_SUCCESS);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateAttestationTestAca) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");

  AllowlistCertificateRequest(::attestation::ACAType::TEST_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  std::string certificate;
  EXPECT_CALL(callback, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate));

  AttestationFlowIntegrated flow(::attestation::ACAType::TEST_ACA);
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_FALSE(certificate.empty());
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateAcaTypeFromCommandline) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(chromeos::switches::kAttestationServer,
                                  "test");
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");

  AllowlistCertificateRequest(::attestation::ACAType::TEST_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  std::string certificate;
  EXPECT_CALL(callback, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_FALSE(certificate.empty());
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateAttestationEmptyAccountId) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_MACHINE_CERTIFICATE);
  request.set_username("");
  request.set_key_label("label");
  request.set_request_origin("origin");

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  std::string certificate;
  EXPECT_CALL(callback, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      EmptyAccountId(), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_FALSE(certificate.empty());
}

TEST_F(AttestationFlowIntegratedTest,
       GetCertificateAttestationKeyNameFromProfile) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_ENROLLMENT_CERTIFICATE);
  request.set_username("");
  // Note: no key label is set.
  request.set_request_origin("origin");

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  std::string certificate;
  EXPECT_CALL(callback, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      EmptyAccountId(), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_FALSE(certificate.empty());
}

}  // namespace attestation
}  // namespace chromeos
