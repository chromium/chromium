// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/attestation_flow.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/time/tick_clock.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/attestation/attestation_flow_factory.h"
#include "chromeos/ash/components/attestation/attestation_flow_integrated.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::DoDefault;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::Sequence;
using testing::StrictMock;
using testing::WithArgs;

namespace ash {
namespace attestation {

namespace {

constexpr char kFakeUserEmail[] = "fake@test.com";
constexpr char kFakeKeyName[] = "fake_key_name";

}  // namespace

class AttestationFlowTest : public testing::Test {
 public:
  AttestationFlowTest() { AttestationClient::InitializeFake(); }
  ~AttestationFlowTest() override { AttestationClient::Shutdown(); }
  void QuitRunLoopCertificateCallback(
      AttestationFlow::CertificateCallback callback,
      AttestationStatus status,
      const std::string& cert) {
    LOG(WARNING) << "Quitting run loop.";
    run_loop_->Quit();
    if (callback)
      std::move(callback).Run(status, cert);
  }

 protected:
  void RunUntilIdle() {
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop_->RunUntilIdle();
  }

  void Run() {
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop_->Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<base::RunLoop> run_loop_;
};

// Same as `AttestationFlowTest` except this is used to run death tests in
// isolated processes.
class AttestationFlowDeathTest : public AttestationFlowTest {
 public:
  AttestationFlowDeathTest() = default;
  AttestationFlowDeathTest(const AttestationFlowDeathTest&) = delete;
  AttestationFlowDeathTest& operator=(const AttestationFlowDeathTest&) = delete;
  ~AttestationFlowDeathTest() override = default;
};

TEST_F(AttestationFlowTest, GetCertificate) {
  // Verify the order of calls in a sequence.
  Sequence flow_order;

  // Set the enrollment status as `false` so the full enrollment flow is
  // triggered.
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(false);
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  // Use StrictMock when we want to verify invocation frequency.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  proxy->fake()->set_enroll_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaEnrollResponse());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(*proxy, SendEnrollRequest(AttestationClient::Get()
                                            ->GetTestInterface()
                                            ->GetFakePcaEnrollRequest(),
                                        _))
      .Times(1)
      .InSequence(flow_order);

  const AccountId account_id = AccountId::FromUserEmail(kFakeUserEmail);

  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistLegacyCreateCertificateRequest(
          kFakeUserEmail, "fake_origin",
          ::attestation::ENTERPRISE_USER_CERTIFICATE,
          ::attestation::KEY_TYPE_RSA);

  proxy->fake()->set_cert_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaCertResponse());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          AttestationClient::Get()->GetTestInterface()->GetFakePcaCertRequest(),
          _))
      .Times(1)
      .InSequence(flow_order);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(
      observer,
      MockCertificateCallback(
          ATTESTATION_SUCCESS,
          AttestationClient::Get()->GetTestInterface()->GetFakeCertificate()))
      .Times(1)
      .InSequence(flow_order);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/account_id,
      /*request_origin=*/"fake_origin", /*force_new_key=*/true,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/kFakeKeyName,
      /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();

  EXPECT_EQ(AttestationClient::Get()->GetTestInterface()->GetFakeCertificate(),
            AttestationClient::Get()
                ->GetTestInterface()
                ->GetMutableKeyInfoReply(kFakeUserEmail, kFakeKeyName)
                ->certificate());
}

// This is pretty much identical to `GetCertificate` test but for
// `DEVICE_SETUP_CERTIFICATE`
TEST_F(AttestationFlowTest, GetCertificate_DeviceSetupCertificate) {
  // Verify the order of calls in a sequence.
  Sequence flow_order;

  // Set the enrollment status as `false` so the full enrollment flow is
  // triggered.
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(false);
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  // Use StrictMock when we want to verify invocation frequency.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  proxy->fake()->set_enroll_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaEnrollResponse());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(*proxy, SendEnrollRequest(AttestationClient::Get()
                                            ->GetTestInterface()
                                            ->GetFakePcaEnrollRequest(),
                                        _))
      .Times(1)
      .InSequence(flow_order);

  // `DEVICE_SETUP_CERTIFICATE` is associated with the device, not to a
  // username.
  const std::string kEmptyUsername = std::string();

  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistLegacyCreateCertificateRequest(
          kEmptyUsername, "fake_origin",
          ::attestation::DEVICE_SETUP_CERTIFICATE, ::attestation::KEY_TYPE_RSA);

  proxy->fake()->set_cert_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaCertResponse());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          AttestationClient::Get()->GetTestInterface()->GetFakePcaCertRequest(),
          _))
      .Times(1)
      .InSequence(flow_order);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(
      observer,
      MockCertificateCallback(
          ATTESTATION_SUCCESS,
          AttestationClient::Get()->GetTestInterface()->GetFakeCertificate()))
      .Times(1)
      .InSequence(flow_order);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  ::attestation::DeviceSetupCertificateRequestMetadata profile_specific_data;
  profile_specific_data.set_id("random_id");
  profile_specific_data.set_content_binding("content_binding");
  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  const std::string kOrigin = "fake_origin";
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_DEVICE_SETUP_CERTIFICATE,
      /*account_id=*/EmptyAccountId(), /*request_origin=*/kOrigin,
      /*force_new_key=*/true,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/kFakeKeyName,
      /*profile_specific_data=*/
      std::make_optional(
          AttestationFlow::CertProfileSpecificData(profile_specific_data)),
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();

  EXPECT_EQ(AttestationClient::Get()->GetTestInterface()->GetFakeCertificate(),
            AttestationClient::Get()
                ->GetTestInterface()
                ->GetMutableKeyInfoReply(kEmptyUsername, kFakeKeyName)
                ->certificate());
}

// This is pretty much identical to `GetCertificate` while the flow under test
// is created by the factory function to make sure that the factory function
// instantiates an object of the intended type.
TEST_F(AttestationFlowTest, GetCertificateCreatedByFactory) {
  // Verify the order of calls in a sequence.
  Sequence flow_order;

  // Set the enrollment status as `false` so the full enrollment flow is
  // triggered.
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(false);
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  // Use StrictMock when we want to verify invocation frequency.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  proxy->fake()->set_enroll_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaEnrollResponse());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(*proxy, SendEnrollRequest(AttestationClient::Get()
                                            ->GetTestInterface()
                                            ->GetFakePcaEnrollRequest(),
                                        _))
      .Times(1)
      .InSequence(flow_order);

  const AccountId account_id = AccountId::FromUserEmail(kFakeUserEmail);

  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistLegacyCreateCertificateRequest(
          kFakeUserEmail, "fake_origin",
          ::attestation::ENTERPRISE_USER_CERTIFICATE,
          ::attestation::KEY_TYPE_RSA);

  proxy->fake()->set_cert_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaCertResponse());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          AttestationClient::Get()->GetTestInterface()->GetFakePcaCertRequest(),
          _))
      .Times(1)
      .InSequence(flow_order);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(
      observer,
      MockCertificateCallback(
          ATTESTATION_SUCCESS,
          AttestationClient::Get()->GetTestInterface()->GetFakeCertificate()))
      .Times(1)
      .InSequence(flow_order);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowFactory attestation_flow_factory;
  attestation_flow_factory.Initialize(std::move(proxy_interface));
  attestation_flow_factory.GetFallback()->GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/account_id, /*request_origin=*/"fake_origin",
      /*force_new_key=*/true,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/kFakeKeyName, /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();

  EXPECT_EQ(AttestationClient::Get()->GetTestInterface()->GetFakeCertificate(),
            AttestationClient::Get()
                ->GetTestInterface()
                ->GetMutableKeyInfoReply(kFakeUserEmail, kFakeKeyName)
                ->certificate());
}

// This is pretty much identical to |GetCertificate| item but during
// construction the ecc key type is specified.
TEST_F(AttestationFlowTest, GetCertificate_Ecc) {
  // Verify the order of calls in a sequence.
  Sequence flow_order;

  // Set the enrollment status as `false` so the full enrollment flow is
  // triggered.
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(false);
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  // Use StrictMock when we want to verify invocation frequency.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  proxy->fake()->set_enroll_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaEnrollResponse());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(*proxy, SendEnrollRequest(AttestationClient::Get()
                                            ->GetTestInterface()
                                            ->GetFakePcaEnrollRequest(),
                                        _))
      .Times(1)
      .InSequence(flow_order);

  const AccountId account_id = AccountId::FromUserEmail(kFakeUserEmail);

  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistLegacyCreateCertificateRequest(
          kFakeUserEmail, "fake_origin",
          ::attestation::ENTERPRISE_USER_CERTIFICATE,
          ::attestation::KEY_TYPE_ECC);

  proxy->fake()->set_cert_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaCertResponse());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          AttestationClient::Get()->GetTestInterface()->GetFakePcaCertRequest(),
          _))
      .Times(1)
      .InSequence(flow_order);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(
      observer,
      MockCertificateCallback(
          ATTESTATION_SUCCESS,
          AttestationClient::Get()->GetTestInterface()->GetFakeCertificate()))
      .Times(1)
      .InSequence(flow_order);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/account_id,
      /*request_origin=*/"fake_origin", /*force_new_key=*/true,
      /*key_crypto_type=*/::attestation::KEY_TYPE_ECC,
      /*key_name=*/kFakeKeyName,
      /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();

  EXPECT_EQ(AttestationClient::Get()->GetTestInterface()->GetFakeCertificate(),
            AttestationClient::Get()
                ->GetTestInterface()
                ->GetMutableKeyInfoReply(kFakeUserEmail, kFakeKeyName)
                ->certificate());
}

// This is pretty much identical to `GetCertificate` while the fake attestation
// client only accepts the requests with test ACA type specified.
TEST_F(AttestationFlowTest, GetCertificate_TestACA) {
  // Verify the order of calls in a sequence.
  Sequence flow_order;

  // Set the enrollment status as `false` so the full enrollment flow is
  // triggered.
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(false);
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  // Set the ACA type to test ACA so we can make sure the enroll request and the
  // certificate request has the right ACA type.
  AttestationClient::Get()->GetTestInterface()->set_aca_type_for_legacy_flow(
      ::attestation::TEST_ACA);

  // Use StrictMock when we want to verify invocation frequency.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  proxy->fake()->set_enroll_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaEnrollResponse());
  // Set the PCA type returned by `ServerProxy` so it can meet the expectation
  // by `FakeAttestationClient`.
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(Return(TEST_PCA));
  EXPECT_CALL(*proxy, SendEnrollRequest(AttestationClient::Get()
                                            ->GetTestInterface()
                                            ->GetFakePcaEnrollRequest(),
                                        _))
      .Times(1)
      .InSequence(flow_order);

  const AccountId account_id = AccountId::FromUserEmail(kFakeUserEmail);

  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistLegacyCreateCertificateRequest(
          kFakeUserEmail, "fake_origin",
          ::attestation::ENTERPRISE_USER_CERTIFICATE,
          ::attestation::KEY_TYPE_RSA);

  proxy->fake()->set_cert_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaCertResponse());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          AttestationClient::Get()->GetTestInterface()->GetFakePcaCertRequest(),
          _))
      .Times(1)
      .InSequence(flow_order);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(
      observer,
      MockCertificateCallback(
          ATTESTATION_SUCCESS,
          AttestationClient::Get()->GetTestInterface()->GetFakeCertificate()))
      .Times(1)
      .InSequence(flow_order);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/account_id,
      /*request_origin=*/"fake_origin", /*force_new_key=*/true,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/kFakeKeyName, /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();

  EXPECT_EQ(AttestationClient::Get()->GetTestInterface()->GetFakeCertificate(),
            AttestationClient::Get()
                ->GetTestInterface()
                ->GetMutableKeyInfoReply(kFakeUserEmail, kFakeKeyName)
                ->certificate());
}

TEST_F(AttestationFlowTest, GetCertificate_Attestation_Not_Prepared) {
  // Verify the order of calls in a sequence.
  Sequence flow_order;

  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(false);
  AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparationsSequence({false, true});

  // Use StrictMock when we want to verify invocation frequency.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  proxy->fake()->set_enroll_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaEnrollResponse());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(*proxy, SendEnrollRequest(AttestationClient::Get()
                                            ->GetTestInterface()
                                            ->GetFakePcaEnrollRequest(),
                                        _))
      .Times(1)
      .InSequence(flow_order);

  const AccountId account_id = AccountId::FromUserEmail(kFakeUserEmail);

  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistLegacyCreateCertificateRequest(
          kFakeUserEmail, "fake_origin",
          ::attestation::ENTERPRISE_USER_CERTIFICATE,
          ::attestation::KEY_TYPE_RSA);

  proxy->fake()->set_cert_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaCertResponse());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          AttestationClient::Get()->GetTestInterface()->GetFakePcaCertRequest(),
          _))
      .Times(1)
      .InSequence(flow_order);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(
      observer,
      MockCertificateCallback(
          ATTESTATION_SUCCESS,
          AttestationClient::Get()->GetTestInterface()->GetFakeCertificate()))
      .Times(1)
      .InSequence(flow_order);
  AttestationFlow::CertificateCallback callback =
      base::BindOnce(&AttestationFlowTest::QuitRunLoopCertificateCallback,
                     base::Unretained(this),
                     base::BindOnce(&MockObserver::MockCertificateCallback,
                                    base::Unretained(&observer)));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.set_retry_delay(base::Milliseconds(30));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/account_id,
      /*request_origin=*/"fake_origin", /*force_new_key=*/true,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/kEnterpriseUserKey, /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(callback));

  Run();

  EXPECT_EQ(AttestationClient::Get()->GetTestInterface()->GetFakeCertificate(),
            AttestationClient::Get()
                ->GetTestInterface()
                ->GetMutableKeyInfoReply(kFakeUserEmail, kEnterpriseUserKey)
                ->certificate());
}

TEST_F(AttestationFlowTest, GetCertificate_Attestation_Never_Prepared) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(false);
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      false);

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_UNSPECIFIED_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback callback =
      base::BindOnce(&AttestationFlowTest::QuitRunLoopCertificateCallback,
                     base::Unretained(this),
                     base::BindOnce(&MockObserver::MockCertificateCallback,
                                    base::Unretained(&observer)));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.set_ready_timeout(base::Milliseconds(20));
  flow.set_retry_delay(base::Milliseconds(6));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/EmptyAccountId(),
      /*request_origin=*/"fake_origin", /*force_new_key=*/true,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/"fake_key_name", /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(callback));

  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_Attestation_Not_Available) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(false);
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_features_reply()
      ->set_is_available(false);

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(ATTESTATION_NOT_AVAILABLE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback callback =
      base::BindOnce(&AttestationFlowTest::QuitRunLoopCertificateCallback,
                     base::Unretained(this),
                     base::BindOnce(&MockObserver::MockCertificateCallback,
                                    base::Unretained(&observer)));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.set_ready_timeout(base::Milliseconds(20));
  flow.set_retry_delay(base::Milliseconds(6));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/EmptyAccountId(),
      /*request_origin=*/"fake_origin", /*force_new_key=*/true,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/kFakeKeyName, /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(callback));

  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_Attestation_Never_Confirm_Prepared) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(false);
  AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparationsStatus(
          ::attestation::STATUS_NOT_AVAILABLE);

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_UNSPECIFIED_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback callback =
      base::BindOnce(&AttestationFlowTest::QuitRunLoopCertificateCallback,
                     base::Unretained(this),
                     base::BindOnce(&MockObserver::MockCertificateCallback,
                                    base::Unretained(&observer)));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.set_ready_timeout(base::Milliseconds(20));
  flow.set_retry_delay(base::Milliseconds(6));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/EmptyAccountId(),
      /*request_origin=*/"fake_origin", /*force_new_key=*/true,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/"fake_key_name", /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(callback));

  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_Attestation_Not_Verified) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(false);
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_verified_boot(false);
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_UNSPECIFIED_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback callback =
      base::BindOnce(&AttestationFlowTest::QuitRunLoopCertificateCallback,
                     base::Unretained(this),
                     base::BindOnce(&MockObserver::MockCertificateCallback,
                                    base::Unretained(&observer)));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.set_ready_timeout(base::Milliseconds(20));
  flow.set_retry_delay(base::Milliseconds(6));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_SOFT_BIND_CERTIFICATE,
      /*account_id=*/EmptyAccountId(),
      /*request_origin=*/"fake_origin", /*force_new_key=*/true,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/"fake_key_name", /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(callback));

  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_NoEK) {
  AttestationClient::Get()->GetTestInterface()->set_enroll_request_status(
      ::attestation::STATUS_UNEXPECTED_DEVICE_ERROR);
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(false);
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_UNSPECIFIED_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/EmptyAccountId(), /*request_origin=*/"",
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/"fake_key_name", /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_EKRejected) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(false);
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(false);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(*proxy, SendEnrollRequest(AttestationClient::Get()
                                            ->GetTestInterface()
                                            ->GetFakePcaEnrollRequest(),
                                        _))
      .Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_UNSPECIFIED_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/EmptyAccountId(), /*request_origin=*/"",
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/"fake_key_name", /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_FailEnroll) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(false);
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      true);

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  proxy->fake()->set_enroll_response(
      "bad " +
      AttestationClient::Get()->GetTestInterface()->GetFakePcaEnrollResponse());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(*proxy, SendEnrollRequest(AttestationClient::Get()
                                            ->GetTestInterface()
                                            ->GetFakePcaEnrollRequest(),
                                        _))
      .Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_UNSPECIFIED_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/EmptyAccountId(), /*request_origin=*/"",
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/"fake_key_name", /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetMachineCertificateAlreadyEnrolled) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(true);
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistLegacyCreateCertificateRequest(
          /*username=*/"", /*request_origin=*/"",
          ::attestation::ENTERPRISE_MACHINE_CERTIFICATE,
          ::attestation::KEY_TYPE_RSA);

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  proxy->fake()->set_cert_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaCertResponse());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          AttestationClient::Get()->GetTestInterface()->GetFakePcaCertRequest(),
          _))
      .Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(
      observer,
      MockCertificateCallback(
          ATTESTATION_SUCCESS,
          AttestationClient::Get()->GetTestInterface()->GetFakeCertificate()))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
      /*account_id=*/EmptyAccountId(),
      /*request_origin=*/"", /*force_new_key=*/true,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/kFakeKeyName, /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();
  EXPECT_EQ(AttestationClient::Get()->GetTestInterface()->GetFakeCertificate(),
            AttestationClient::Get()
                ->GetTestInterface()
                ->GetMutableKeyInfoReply(
                    /*username=*/"", kFakeKeyName)
                ->certificate());
}

// There used to be an incidence that a non-empty username are sent when
// requesting a device key certificate, and we remove the username in the
// attestation flow process though it is not considered a valid input.
// TODO(b/179364923): Develop a better API design along with strict assertion
// instead of silently removing the username.
TEST_F(AttestationFlowTest, GetMachineCertificateWithUsername) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(true);
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistLegacyCreateCertificateRequest(
          /*username=*/"", /*request_origin=*/"",
          ::attestation::ENTERPRISE_MACHINE_CERTIFICATE,
          ::attestation::KEY_TYPE_RSA);

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  proxy->fake()->set_cert_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaCertResponse());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          AttestationClient::Get()->GetTestInterface()->GetFakePcaCertRequest(),
          _));

  StrictMock<MockObserver> observer;

  const AccountId account_id = AccountId::FromUserEmail(kFakeUserEmail);

  EXPECT_CALL(
      observer,
      MockCertificateCallback(
          ATTESTATION_SUCCESS,
          AttestationClient::Get()->GetTestInterface()->GetFakeCertificate()))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
      /*account_id=*/account_id, /*request_origin=*/"",
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/kFakeKeyName, /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();
  // The certificate should be stored as a machine key instead of a user key.
  EXPECT_EQ(AttestationClient::Get()->GetTestInterface()->GetFakeCertificate(),
            AttestationClient::Get()
                ->GetTestInterface()
                ->GetMutableKeyInfoReply(/*username=*/"", kFakeKeyName)
                ->certificate());
}

TEST_F(AttestationFlowTest, GetEnrollmentCertificateAlreadyEnrolled) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(true);
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistLegacyCreateCertificateRequest(
          /*username=*/"", /*request_origin=*/"",
          ::attestation::ENTERPRISE_ENROLLMENT_CERTIFICATE,
          ::attestation::KEY_TYPE_RSA);

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  proxy->fake()->set_cert_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaCertResponse());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          AttestationClient::Get()->GetTestInterface()->GetFakePcaCertRequest(),
          _))
      .Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(
      observer,
      MockCertificateCallback(
          ATTESTATION_SUCCESS,
          AttestationClient::Get()->GetTestInterface()->GetFakeCertificate()))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE,
      /*account_id=*/EmptyAccountId(), /*request_origin=*/"",
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/kFakeKeyName, /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();
  EXPECT_EQ(AttestationClient::Get()->GetTestInterface()->GetFakeCertificate(),
            AttestationClient::Get()
                ->GetTestInterface()
                ->GetMutableKeyInfoReply(/*username=*/"", kFakeKeyName)
                ->certificate());
}

TEST_F(AttestationFlowTest, GetCertificate_FailCreateCertRequest) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(true);
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistLegacyCreateCertificateRequest(
          kFakeUserEmail, "fake_origin",
          ::attestation::ENTERPRISE_USER_CERTIFICATE,
          ::attestation::KEY_TYPE_RSA);

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_UNSPECIFIED_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/EmptyAccountId(), /*request_origin=*/"",
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/"fake_key_name", /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_CertRequestRejected) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(true);
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistLegacyCreateCertificateRequest(
          /*username=*/"", /*request_origin=*/"",
          ::attestation::ENTERPRISE_USER_CERTIFICATE,
          ::attestation::KEY_TYPE_RSA);

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(false);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          AttestationClient::Get()->GetTestInterface()->GetFakePcaCertRequest(),
          _))
      .Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_UNSPECIFIED_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/EmptyAccountId(), /*request_origin=*/"",
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/"fake_key_name", /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_CertRequestBadRequest) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(true);
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistLegacyCreateCertificateRequest(
          /*username=*/"", /*request_origin=*/"",
          ::attestation::ENTERPRISE_USER_CERTIFICATE,
          ::attestation::KEY_TYPE_RSA);

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  // Add a random suffix so the fake attestation client fails to finish
  // certificate.
  proxy->fake()->set_cert_response(
      "bad " +
      AttestationClient::Get()->GetTestInterface()->GetFakePcaCertResponse());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          AttestationClient::Get()->GetTestInterface()->GetFakePcaCertRequest(),
          _))
      .Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(
                            ATTESTATION_SERVER_BAD_REQUEST_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/EmptyAccountId(), /*request_origin=*/"",
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/"fake_key_name", /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_FailIsEnrolled) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_status(::attestation::STATUS_DBUS_ERROR);

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_UNSPECIFIED_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/EmptyAccountId(), /*request_origin=*/"",
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/"fake_key_name", /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_CheckExisting) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(true);
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistLegacyCreateCertificateRequest(
          /*username=*/"", /*request_origin=*/"",
          ::attestation::ENTERPRISE_USER_CERTIFICATE,
          ::attestation::KEY_TYPE_RSA);

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  proxy->fake()->set_cert_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaCertResponse());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          AttestationClient::Get()->GetTestInterface()->GetFakePcaCertRequest(),
          _))
      .Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(
      observer,
      MockCertificateCallback(
          ATTESTATION_SUCCESS,
          AttestationClient::Get()->GetTestInterface()->GetFakeCertificate()))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/EmptyAccountId(), /*request_origin=*/"",
      /*force_new_key=*/false, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/kFakeKeyName, /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();
  EXPECT_EQ(AttestationClient::Get()->GetTestInterface()->GetFakeCertificate(),
            AttestationClient::Get()
                ->GetTestInterface()
                ->GetMutableKeyInfoReply(
                    /*username=*/"", kFakeKeyName)
                ->certificate());
}

TEST_F(AttestationFlowTest, GetCertificate_AlreadyExists) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(true);
  AttestationClient::Get()
      ->GetTestInterface()
      ->GetMutableKeyInfoReply("", kEnterpriseUserKey)
      ->set_certificate("fake_cert");

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_SUCCESS, "fake_cert"))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_USER_CERTIFICATE,
      /*account_id=*/EmptyAccountId(), /*request_origin=*/"",
      /*force_new_key=*/false, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/kEnterpriseUserKey, /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();
}

// There used to be an incidence that a non-empty username are sent when
// requesting a device key certificate, and we remove the username in the
// attestation flow process though it is not considered a valid input.
// TODO(b/179364923): Develop a better API design along with strict assertion
// instead of silently removing the username.
TEST_F(AttestationFlowTest, GetCertificate_LookupMachineKeyWithAccountId) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->mutable_status_reply()
      ->set_enrolled(true);
  AttestationClient::Get()
      ->GetTestInterface()
      ->GetMutableKeyInfoReply("", kEnterpriseMachineKey)
      ->set_certificate("fake_cert");

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_SUCCESS, "fake_cert"))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::BindOnce(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  const AccountId account_id = AccountId::FromUserEmail(kFakeUserEmail);
  AttestationFlowLegacy flow(std::move(proxy_interface));
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
      /*account_id=*/account_id, /*request_origin=*/"",
      /*force_new_key=*/false, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/kEnterpriseMachineKey,
      /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(mock_callback));
  RunUntilIdle();
}

TEST_F(AttestationFlowDeathTest,
       GetCertificate_DeviceSetupCertificateWithIncorrectParams) {
  // Use StrictMock when we want to verify invocation frequency.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  proxy->fake()->set_enroll_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaEnrollResponse());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  // `DEVICE_SETUP_CERTIFICATE` is associated with the device, not to a
  // username.
  const std::string kEmptyUsername = std::string();

  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistLegacyCreateCertificateRequest(
          kEmptyUsername, "fake_origin",
          ::attestation::DEVICE_SETUP_CERTIFICATE, ::attestation::KEY_TYPE_RSA);

  proxy->fake()->set_cert_response(
      AttestationClient::Get()->GetTestInterface()->GetFakePcaCertResponse());

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlowLegacy flow(std::move(proxy_interface));
  const std::string kOrigin = "fake_origin";
  // Do not supply `profile_specific_data`.
  flow.GetCertificate(
      /*certificate_profile=*/PROFILE_DEVICE_SETUP_CERTIFICATE,
      /*account_id=*/EmptyAccountId(), /*request_origin=*/kOrigin,
      /*force_new_key=*/true,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/"fake_key_name",
      /*profile_specific_data=*/std::nullopt, /*callback=*/base::DoNothing());
  EXPECT_DCHECK_DEATH(RunUntilIdle());
}

}  // namespace attestation
}  // namespace ash
