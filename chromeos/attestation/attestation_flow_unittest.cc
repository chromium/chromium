// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/tick_clock.h"
#include "base/timer/timer.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
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

namespace chromeos {
namespace attestation {

namespace {

void AsyncCallbackFalse(cryptohome::AsyncMethodCaller::Callback callback) {
  callback.Run(false, cryptohome::MOUNT_ERROR_NONE);
}

}  // namespace

class AttestationFlowTest : public testing::Test {
 public:
  void QuitRunLoopCertificateCallback(
      const AttestationFlow::CertificateCallback& callback,
      AttestationStatus status,
      const std::string& cert) {
    LOG(WARNING) << "Quitting run loop.";
    run_loop_->Quit();
    if (!callback.is_null())
      callback.Run(status, cert);
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
  base::RunLoop* run_loop_;
};

TEST_F(AttestationFlowTest, GetCertificate) {
  // Verify the order of calls in a sequence.
  Sequence flow_order;

  // Use DBusCallbackFalse so the full enrollment flow is triggered.
  chromeos::FakeCryptohomeClient client;
  client.set_tpm_attestation_is_enrolled(false);
  client.set_tpm_attestation_is_prepared(true);

  // Use StrictMock when we want to verify invocation frequency.
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateEnrollRequest(_, _))
      .Times(1)
      .InSequence(flow_order);

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(
      *proxy,
      SendEnrollRequest(
          cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest, _))
      .Times(1)
      .InSequence(flow_order);

  std::string fake_enroll_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest;
  fake_enroll_response += "_response";
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationEnroll(_, fake_enroll_response, _))
      .Times(1)
      .InSequence(flow_order);

  const AccountId account_id = AccountId::FromUserEmail("fake@test.com");
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationCreateCertRequest(
                  _, PROFILE_ENTERPRISE_USER_CERTIFICATE,
                  cryptohome::Identification(account_id), "fake_origin", _))
      .Times(1)
      .InSequence(flow_order);

  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest, _))
      .Times(1)
      .InSequence(flow_order);

  std::string fake_cert_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest;
  fake_cert_response += "_response";
  EXPECT_CALL(async_caller, AsyncTpmAttestationFinishCertRequest(
                                fake_cert_response, KEY_USER,
                                cryptohome::Identification(account_id),
                                kEnterpriseUserKey, _))
      .Times(1)
      .InSequence(flow_order);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(
                  ATTESTATION_SUCCESS,
                  cryptohome::MockAsyncMethodCaller::kFakeAttestationCert))
      .Times(1)
      .InSequence(flow_order);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, account_id,
                      "fake_origin", true, std::string() /* key_name */,
                      mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_Attestation_Not_Prepared) {
  // Verify the order of calls in a sequence.
  Sequence flow_order;

  // Custom FakeCryptohomeClient to emulate a situation where it takes a bit
  // for attestation to be prepared.
  class FakeCryptohomeClient : public chromeos::FakeCryptohomeClient {
   public:
    void TpmAttestationIsPrepared(DBusMethodCallback<bool> callback) override {
      chromeos::FakeCryptohomeClient::TpmAttestationIsPrepared(
          std::move(callback));
      // Second call (and later), returns true.
      set_tpm_attestation_is_prepared(true);
    }
  };

  FakeCryptohomeClient client;
  client.set_tpm_attestation_is_enrolled(false);
  client.set_tpm_attestation_is_prepared(false);

  // Use StrictMock when we want to verify invocation frequency.
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateEnrollRequest(_, _))
      .Times(1)
      .InSequence(flow_order);

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(
      *proxy,
      SendEnrollRequest(
          cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest, _))
      .Times(1)
      .InSequence(flow_order);

  std::string fake_enroll_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest;
  fake_enroll_response += "_response";
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationEnroll(_, fake_enroll_response, _))
      .Times(1)
      .InSequence(flow_order);

  const AccountId account_id = AccountId::FromUserEmail("fake@test.com");
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationCreateCertRequest(
                  _, PROFILE_ENTERPRISE_USER_CERTIFICATE,
                  cryptohome::Identification(account_id), "fake_origin", _))
      .Times(1)
      .InSequence(flow_order);

  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest, _))
      .Times(1)
      .InSequence(flow_order);

  std::string fake_cert_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest;
  fake_cert_response += "_response";
  EXPECT_CALL(async_caller, AsyncTpmAttestationFinishCertRequest(
                                fake_cert_response, KEY_USER,
                                cryptohome::Identification(account_id),
                                kEnterpriseUserKey, _));

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(
                  ATTESTATION_SUCCESS,
                  cryptohome::MockAsyncMethodCaller::kFakeAttestationCert))
      .Times(1)
      .InSequence(flow_order);
  AttestationFlow::CertificateCallback callback =
      base::Bind(&AttestationFlowTest::QuitRunLoopCertificateCallback,
                 base::Unretained(this),
                 base::Bind(&MockObserver::MockCertificateCallback,
                            base::Unretained(&observer)));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.set_retry_delay(base::TimeDelta::FromMilliseconds(30));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, account_id,
                      "fake_origin", true, std::string() /* key_name */,
                      callback);

  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_Attestation_Never_Prepared) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(false, cryptohome::MOUNT_ERROR_NONE);

  chromeos::FakeCryptohomeClient client;
  client.set_tpm_attestation_is_enrolled(false);
  client.set_tpm_attestation_is_prepared(false);

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_UNSPECIFIED_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback callback =
      base::Bind(&AttestationFlowTest::QuitRunLoopCertificateCallback,
                 base::Unretained(this),
                 base::Bind(&MockObserver::MockCertificateCallback,
                            base::Unretained(&observer)));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.set_ready_timeout(base::TimeDelta::FromMilliseconds(20));
  flow.set_retry_delay(base::TimeDelta::FromMilliseconds(6));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(),
                      "fake_origin", true, std::string() /* key_name */,
                      callback);

  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_NoEK) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(false, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateEnrollRequest(_, _))
      .Times(1);

  chromeos::FakeCryptohomeClient client;
  client.set_tpm_attestation_is_enrolled(false);
  client.set_tpm_attestation_is_prepared(true);

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_UNSPECIFIED_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      true, std::string() /* key_name */, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_EKRejected) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateEnrollRequest(_, _))
      .Times(1);

  chromeos::FakeCryptohomeClient client;
  client.set_tpm_attestation_is_enrolled(false);
  client.set_tpm_attestation_is_prepared(true);

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(false);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(
      *proxy,
      SendEnrollRequest(
          cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest, _))
      .Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_UNSPECIFIED_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      true, std::string() /* key_name */, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_FailEnroll) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateEnrollRequest(_, _))
      .Times(1);
  std::string fake_enroll_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest;
  fake_enroll_response += "_response";
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationEnroll(_, fake_enroll_response, _))
      .WillOnce(WithArgs<2>(Invoke(AsyncCallbackFalse)));

  chromeos::FakeCryptohomeClient client;
  client.set_tpm_attestation_is_enrolled(false);
  client.set_tpm_attestation_is_prepared(true);

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(
      *proxy,
      SendEnrollRequest(
          cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest, _))
      .Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_UNSPECIFIED_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      true, std::string() /* key_name */, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetMachineCertificateAlreadyEnrolled) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateCertRequest(
                                _, PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
                                cryptohome::Identification(), "", _))
      .Times(1);
  std::string fake_cert_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest;
  fake_cert_response += "_response";
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationFinishCertRequest(
                  fake_cert_response, KEY_DEVICE, cryptohome::Identification(),
                  kEnterpriseMachineKey, _))
      .Times(1);

  chromeos::FakeCryptohomeClient client;

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest, _))
      .Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(
                  ATTESTATION_SUCCESS,
                  cryptohome::MockAsyncMethodCaller::kFakeAttestationCert))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_MACHINE_CERTIFICATE, EmptyAccountId(),
                      "", true, std::string() /* key_name */, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetEnrollmentCertificateAlreadyEnrolled) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateCertRequest(
                                _, PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE,
                                cryptohome::Identification(), "", _))
      .Times(1);
  std::string fake_cert_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest;
  fake_cert_response += "_response";
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationFinishCertRequest(
                  fake_cert_response, KEY_DEVICE, cryptohome::Identification(),
                  kEnterpriseEnrollmentKey, _))
      .Times(1);

  chromeos::FakeCryptohomeClient client;

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest, _))
      .Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(
                  ATTESTATION_SUCCESS,
                  cryptohome::MockAsyncMethodCaller::kFakeAttestationCert))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::BindRepeating(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE,
                      EmptyAccountId(), "", true, std::string() /* key_name */,
                      mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_FailCreateCertRequest) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(false, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateCertRequest(
                                _, PROFILE_ENTERPRISE_USER_CERTIFICATE,
                                cryptohome::Identification(), "", _))
      .Times(1);

  chromeos::FakeCryptohomeClient client;

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_UNSPECIFIED_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      true, std::string() /* key_name */, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_CertRequestRejected) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateCertRequest(
                                _, PROFILE_ENTERPRISE_USER_CERTIFICATE,
                                cryptohome::Identification(), "", _))
      .Times(1);

  chromeos::FakeCryptohomeClient client;

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(false);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest, _))
      .Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_UNSPECIFIED_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::BindRepeating(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      true, std::string() /* key_name */, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_CertRequestBadRequest) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateCertRequest(
                                _, PROFILE_ENTERPRISE_USER_CERTIFICATE,
                                cryptohome::Identification(), "", _))
      .Times(1);

  chromeos::FakeCryptohomeClient client;

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest, _))
      .Times(1);
  EXPECT_CALL(async_caller, AsyncTpmAttestationFinishCertRequest(_, _, _, _, _))
      .Times(1)
      .WillOnce(WithArgs<4>(Invoke(
          [](const cryptohome::AsyncMethodCaller::DataCallback& callback) {
            callback.Run(false, "");
          })));

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(
                            ATTESTATION_SERVER_BAD_REQUEST_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      true, std::string() /* key_name */, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_FailIsEnrolled) {
  // We're not expecting any async calls in this case; StrictMock will verify.
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;

  chromeos::FakeCryptohomeClient client;
  client.SetServiceIsAvailable(false);

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_UNSPECIFIED_FAILURE, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      true, std::string() /* key_name */, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_CheckExisting) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateCertRequest(
                                _, PROFILE_ENTERPRISE_USER_CERTIFICATE,
                                cryptohome::Identification(), "", _))
      .Times(1);
  std::string fake_cert_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest;
  fake_cert_response += "_response";
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationFinishCertRequest(fake_cert_response, KEY_USER,
                                                   cryptohome::Identification(),
                                                   kEnterpriseUserKey, _))
      .Times(1);

  chromeos::FakeCryptohomeClient client;

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest, _))
      .Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(
                  ATTESTATION_SUCCESS,
                  cryptohome::MockAsyncMethodCaller::kFakeAttestationCert))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      false, std::string() /* key_name */, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_AlreadyExists) {
  // We're not expecting any async calls in this case; StrictMock will verify.
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;

  chromeos::FakeCryptohomeClient client;
  client.SetTpmAttestationUserCertificate(cryptohome::AccountIdentifier(),
                                          kEnterpriseUserKey, "fake_cert");

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer,
              MockCertificateCallback(ATTESTATION_SUCCESS, "fake_cert"))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      false, std::string() /* key_name */, mock_callback);
  RunUntilIdle();
}

}  // namespace attestation
}  // namespace chromeos
