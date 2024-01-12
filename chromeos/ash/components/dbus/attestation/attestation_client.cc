// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/attestation/attestation_client.h"

#include <utility>

#include <google/protobuf/message_lite.h>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/attestation/fake_attestation_client.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/attestation/dbus-constants.h"

namespace ash {
namespace {

// Values for the attestation server switch.
const char kAttestationServerDefault[] = "default";
const char kAttestationServerTest[] = "test";

// An arbitrary timeout for getting a certificate.
constexpr base::TimeDelta kGetCertificateTimeout = base::Seconds(80);

AttestationClient* g_instance = nullptr;

// Tries to parse a proto message from |response| into |proto|.
// Returns false if |response| is nullptr or the message cannot be parsed.
bool ParseProto(dbus::Response* response,
                google::protobuf::MessageLite* proto) {
  if (!response) {
    LOG(ERROR) << "Failed to call attestationd";
    return false;
  }

  dbus::MessageReader reader(response);
  if (!reader.PopArrayOfBytesAsProto(proto)) {
    LOG(ERROR) << "Failed to parse response message from attestationd";
    return false;
  }

  return true;
}

// "Real" implementation of AttestationClient talking to the Attestation daemon
// on the Chrome OS side.
class AttestationClientImpl : public AttestationClient {
 public:
  AttestationClientImpl() = default;
  ~AttestationClientImpl() override = default;

  // Not copyable or movable.
  AttestationClientImpl(const AttestationClientImpl&) = delete;
  AttestationClientImpl& operator=(const AttestationClientImpl&) = delete;
  AttestationClientImpl(AttestationClientImpl&&) = delete;
  AttestationClientImpl& operator=(AttestationClientImpl&&) = delete;

  // AttestationClient overrides:
  void GetKeyInfo(const ::attestation::GetKeyInfoRequest& request,
                  GetKeyInfoCallback callback) override {
    CallProtoMethod(attestation::kGetKeyInfo, request, std::move(callback));
  }

  void GetEndorsementInfo(
      const ::attestation::GetEndorsementInfoRequest& request,
      GetEndorsementInfoCallback callback) override {
    CallProtoMethod(attestation::kGetEndorsementInfo, request,
                    std::move(callback));
  }

  void GetAttestationKeyInfo(
      const ::attestation::GetAttestationKeyInfoRequest& request,
      GetAttestationKeyInfoCallback callback) override {
    CallProtoMethod(attestation::kGetAttestationKeyInfo, request,
                    std::move(callback));
  }

  void ActivateAttestationKey(
      const ::attestation::ActivateAttestationKeyRequest& request,
      ActivateAttestationKeyCallback callback) override {
    CallProtoMethod(attestation::kActivateAttestationKey, request,
                    std::move(callback));
  }

  void CreateCertifiableKey(
      const ::attestation::CreateCertifiableKeyRequest& request,
      CreateCertifiableKeyCallback callback) override {
    CallProtoMethod(attestation::kCreateCertifiableKey, request,
                    std::move(callback));
  }

  void Decrypt(const ::attestation::DecryptRequest& request,
               DecryptCallback callback) override {
    CallProtoMethod(attestation::kDecrypt, request, std::move(callback));
  }

  void Sign(const ::attestation::SignRequest& request,
            SignCallback callback) override {
    CallProtoMethod(attestation::kSign, request, std::move(callback));
  }

  void RegisterKeyWithChapsToken(
      const ::attestation::RegisterKeyWithChapsTokenRequest& request,
      RegisterKeyWithChapsTokenCallback callback) override {
    CallProtoMethod(attestation::kRegisterKeyWithChapsToken, request,
                    std::move(callback));
  }

  void GetEnrollmentPreparations(
      const ::attestation::GetEnrollmentPreparationsRequest& request,
      GetEnrollmentPreparationsCallback callback) override {
    CallProtoMethod(attestation::kGetEnrollmentPreparations, request,
                    std::move(callback));
  }

  void GetFeatures(const ::attestation::GetFeaturesRequest& request,
                   GetFeaturesCallback callback) override {
    CallProtoMethod(attestation::kGetFeatures, request, std::move(callback));
  }

  void GetStatus(const ::attestation::GetStatusRequest& request,
                 GetStatusCallback callback) override {
    CallProtoMethod(attestation::kGetStatus, request, std::move(callback));
  }

  void Verify(const ::attestation::VerifyRequest& request,
              VerifyCallback callback) override {
    CallProtoMethod(attestation::kVerify, request, std::move(callback));
  }

  void CreateEnrollRequest(
      const ::attestation::CreateEnrollRequestRequest& request,
      CreateEnrollRequestCallback callback) override {
    CallProtoMethod(attestation::kCreateEnrollRequest, request,
                    std::move(callback));
  }

  void FinishEnroll(const ::attestation::FinishEnrollRequest& request,
                    FinishEnrollCallback callback) override {
    CallProtoMethod(attestation::kFinishEnroll, request, std::move(callback));
  }

  void CreateCertificateRequest(
      const ::attestation::CreateCertificateRequestRequest& request,
      CreateCertificateRequestCallback callback) override {
    CallProtoMethod(attestation::kCreateCertificateRequest, request,
                    std::move(callback));
  }

  void FinishCertificateRequest(
      const ::attestation::FinishCertificateRequestRequest& request,
      FinishCertificateRequestCallback callback) override {
    CallProtoMethod(attestation::kFinishCertificateRequest, request,
                    std::move(callback));
  }

  void Enroll(const ::attestation::EnrollRequest& request,
              EnrollCallback callback) override {
    CallProtoMethod(attestation::kEnroll, request, std::move(callback));
  }

  void GetCertificate(const ::attestation::GetCertificateRequest& request,
                      GetCertificateCallback callback) override {
    CallProtoMethodWithTimeout(attestation::kGetCertificate,
                               kGetCertificateTimeout.InMilliseconds(), request,
                               std::move(callback));
  }

  void SignEnterpriseChallenge(
      const ::attestation::SignEnterpriseChallengeRequest& request,
      SignEnterpriseChallengeCallback callback) override {
    CallProtoMethod(attestation::kSignEnterpriseChallenge, request,
                    std::move(callback));
  }

  void SignSimpleChallenge(
      const ::attestation::SignSimpleChallengeRequest& request,
      SignSimpleChallengeCallback callback) override {
    CallProtoMethod(attestation::kSignSimpleChallenge, request,
                    std::move(callback));
  }

  void SetKeyPayload(const ::attestation::SetKeyPayloadRequest& request,
                     SetKeyPayloadCallback callback) override {
    CallProtoMethod(attestation::kSetKeyPayload, request, std::move(callback));
  }

  void DeleteKeys(const ::attestation::DeleteKeysRequest& request,
                  DeleteKeysCallback callback) override {
    CallProtoMethod(attestation::kDeleteKeys, request, std::move(callback));
  }

  void ResetIdentity(const ::attestation::ResetIdentityRequest& request,
                     ResetIdentityCallback callback) override {
    CallProtoMethod(attestation::kResetIdentity, request, std::move(callback));
  }

  void GetEnrollmentId(const ::attestation::GetEnrollmentIdRequest& request,
                       GetEnrollmentIdCallback callback) override {
    CallProtoMethod(attestation::kGetEnrollmentId, request,
                    std::move(callback));
  }

  void GetCertifiedNvIndex(
      const ::attestation::GetCertifiedNvIndexRequest& request,
      GetCertifiedNvIndexCallback callback) override {
    CallProtoMethod(attestation::kGetCertifiedNvIndex, request,
                    std::move(callback));
  }

  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override {
    proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(
        ::attestation::kAttestationServiceName,
        dbus::ObjectPath(attestation::kAttestationServicePath));
  }

 private:
  TestInterface* GetTestInterface() override { return nullptr; }

  // Calls attestationd's |method_name| method, passing in |request| as input
  // with |timeout_ms|. Once the (asynchronous) call finishes, |callback| is
  // called with the response proto.
  template <typename RequestType, typename ReplyType>
  void CallProtoMethodWithTimeout(
      const char* method_name,
      int timeout_ms,
      const RequestType& request,
      base::OnceCallback<void(const ReplyType&)> callback) {
    dbus::MethodCall method_call(attestation::kAttestationInterface,
                                 method_name);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      ReplyType reply;
      reply.set_status(attestation::STATUS_INVALID_PARAMETER);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), reply));
      return;
    }
    // Bind with the weak pointer of |this| so the response is not
    // handled once |this| is already destroyed.
    proxy_->CallMethod(
        &method_call, timeout_ms,
        base::BindOnce(&AttestationClientImpl::HandleResponse<ReplyType>,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Calls attestationd's |method_name| method, passing in |request| as input
  // with the default timeout. Once the (asynchronous) call finishes, |callback|
  // is called with the response proto.
  template <typename RequestType, typename ReplyType>
  void CallProtoMethod(const char* method_name,
                       const RequestType& request,
                       base::OnceCallback<void(const ReplyType&)> callback) {
    CallProtoMethodWithTimeout(method_name,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, request,
                               std::move(callback));
  }

  // Parses the response proto message from |response| and calls |callback| with
  // the decoded message. Calls |callback| with an |STATUS_DBUS_ERROR| message
  // on error, including timeout.
  template <typename ReplyType>
  void HandleResponse(base::OnceCallback<void(const ReplyType&)> callback,
                      dbus::Response* response) {
    ReplyType reply_proto;
    if (!ParseProto(response, &reply_proto))
      reply_proto.set_status(attestation::STATUS_DBUS_ERROR);
    std::move(callback).Run(reply_proto);
  }

  // D-Bus proxy for the Attestation daemon, not owned.
  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  base::WeakPtrFactory<AttestationClientImpl> weak_factory_{this};
};

}  // namespace

AttestationClient::AttestationClient() {
  CHECK(!g_instance);
  g_instance = this;
}

AttestationClient::~AttestationClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void AttestationClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new AttestationClientImpl())->Init(bus);
}

// static
void AttestationClient::InitializeFake() {
  new FakeAttestationClient();
}

// static
void AttestationClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  // The destructor resets |g_instance|.
  DCHECK(!g_instance);
}

// static
AttestationClient* AttestationClient::Get() {
  return g_instance;
}

// static
bool AttestationClient::IsAttestationPrepared(
    const ::attestation::GetEnrollmentPreparationsReply& reply) {
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    return false;
  }
  for (const auto& preparation : reply.enrollment_preparations()) {
    if (preparation.second) {
      return true;
    }
  }
  return false;
}

// static
::attestation::VAType AttestationClient::GetVerifiedAccessServerType() {
  std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          chromeos::switches::kAttestationServer);
  if (value.empty() || value == kAttestationServerDefault) {
    return ::attestation::DEFAULT_VA;
  }
  if (value == kAttestationServerTest) {
    return ::attestation::TEST_VA;
  }
  LOG(WARNING) << "Invalid Verified Access server value: " << value
               << ". Using default.";
  return attestation::DEFAULT_VA;
}

}  // namespace ash
