// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/attestation/fake_attestation_client.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/cros_system_api/dbus/attestation/dbus-constants.h"

namespace chromeos {
namespace {

constexpr int kCertificateNotAssigned = 0;
constexpr char kFakeCertPrefix[] = "fake cert";

// Posts |callback| on the current thread's task runner, passing it the
// |response| message.
template <class ReplyType>
void PostProtoResponse(base::OnceCallback<void(const ReplyType&)> callback,
                       const ReplyType& reply) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reply));
}

bool GetCertificateRequestEqual(::attestation::GetCertificateRequest r1,
                                ::attestation::GetCertificateRequest r2) {
  // To prevent regression from future expansion to |GetCertificateRequest|, we
  // compare their serialized results so the difference doesn't get away from
  // this function. We can't really make use of
  // |google::protobuf::util::MessageDifferencer|, which doesn't apply to
  // |MessageLite|.

  // |shall_trigger_enrollment| and |forced| shouldn't affect the whilelisting.
  r1.clear_forced();
  r2.clear_forced();
  r1.clear_shall_trigger_enrollment();
  r2.clear_shall_trigger_enrollment();
  return r1.SerializeAsString() == r2.SerializeAsString();
}

}  // namespace

FakeAttestationClient::FakeAttestationClient() = default;

FakeAttestationClient::~FakeAttestationClient() = default;

void FakeAttestationClient::GetKeyInfo(
    const ::attestation::GetKeyInfoRequest& request,
    GetKeyInfoCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::GetEndorsementInfo(
    const ::attestation::GetEndorsementInfoRequest& request,
    GetEndorsementInfoCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::GetAttestationKeyInfo(
    const ::attestation::GetAttestationKeyInfoRequest& request,
    GetAttestationKeyInfoCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::ActivateAttestationKey(
    const ::attestation::ActivateAttestationKeyRequest& request,
    ActivateAttestationKeyCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::CreateCertifiableKey(
    const ::attestation::CreateCertifiableKeyRequest& request,
    CreateCertifiableKeyCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::Decrypt(
    const ::attestation::DecryptRequest& request,
    DecryptCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::Sign(const ::attestation::SignRequest& request,
                                 SignCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::RegisterKeyWithChapsToken(
    const ::attestation::RegisterKeyWithChapsTokenRequest& request,
    RegisterKeyWithChapsTokenCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::GetEnrollmentPreparations(
    const ::attestation::GetEnrollmentPreparationsRequest& request,
    GetEnrollmentPreparationsCallback callback) {
  ::attestation::GetEnrollmentPreparationsReply reply;
  reply.set_status(preparations_status_);

  if (reply.status() == ::attestation::STATUS_SUCCESS) {
    bool is_prepared = is_prepared_;
    // Override the state if there is a customized sequence.
    if (!preparation_sequences_.empty()) {
      is_prepared = preparation_sequences_.front();
      preparation_sequences_.pop_front();
    }
    if (is_prepared) {
      std::vector<::attestation::ACAType> prepared_types;
      // As we do in the attestation service, if the ACA type is not specified,
      // returns the statuses with all the possible ACA types.
      if (request.has_aca_type()) {
        prepared_types = {request.aca_type()};
      } else {
        prepared_types = {::attestation::DEFAULT_ACA, ::attestation::TEST_ACA};
      }
      for (const auto& type : prepared_types) {
        (*reply.mutable_enrollment_preparations())[type] = true;
      }
    }
  }

  PostProtoResponse(std::move(callback), reply);
}

void FakeAttestationClient::GetStatus(
    const ::attestation::GetStatusRequest& request,
    GetStatusCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::Verify(const ::attestation::VerifyRequest& request,
                                   VerifyCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::CreateEnrollRequest(
    const ::attestation::CreateEnrollRequestRequest& request,
    CreateEnrollRequestCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::FinishEnroll(
    const ::attestation::FinishEnrollRequest& request,
    FinishEnrollCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::CreateCertificateRequest(
    const ::attestation::CreateCertificateRequestRequest& request,
    CreateCertificateRequestCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::FinishCertificateRequest(
    const ::attestation::FinishCertificateRequestRequest& request,
    FinishCertificateRequestCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::Enroll(const ::attestation::EnrollRequest& request,
                                   EnrollCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::GetCertificate(
    const ::attestation::GetCertificateRequest& request,
    GetCertificateCallback callback) {
  ::attestation::GetCertificateReply reply;
  reply.set_status(
      ::attestation::AttestationStatus::STATUS_UNEXPECTED_DEVICE_ERROR);

  is_enrolled_ |= request.shall_trigger_enrollment();
  if (!is_enrolled_) {
    PostProtoResponse(std::move(callback), reply);
    return;
  }

  for (size_t i = 0; i < allowlisted_requests_.size(); ++i) {
    if (GetCertificateRequestEqual(allowlisted_requests_[i], request)) {
      if (request.forced() ||
          certificate_indices_[i] == kCertificateNotAssigned) {
        ++certificate_count_;
        certificate_indices_[i] = certificate_count_;
      }
      reply.set_status(::attestation::AttestationStatus::STATUS_SUCCESS);
      reply.set_certificate(kFakeCertPrefix +
                            base::NumberToString(certificate_indices_[i]));
      break;
    }
  }

  PostProtoResponse(std::move(callback), reply);
}

void FakeAttestationClient::SignEnterpriseChallenge(
    const ::attestation::SignEnterpriseChallengeRequest& request,
    SignEnterpriseChallengeCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::SignSimpleChallenge(
    const ::attestation::SignSimpleChallengeRequest& request,
    SignSimpleChallengeCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::SetKeyPayload(
    const ::attestation::SetKeyPayloadRequest& request,
    SetKeyPayloadCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::DeleteKeys(
    const ::attestation::DeleteKeysRequest& request,
    DeleteKeysCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::ResetIdentity(
    const ::attestation::ResetIdentityRequest& request,
    ResetIdentityCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::GetEnrollmentId(
    const ::attestation::GetEnrollmentIdRequest& request,
    GetEnrollmentIdCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::GetCertifiedNvIndex(
    const ::attestation::GetCertifiedNvIndexRequest& request,
    GetCertifiedNvIndexCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::ConfigureEnrollmentPreparations(bool is_prepared) {
  preparations_status_ = ::attestation::STATUS_SUCCESS;
  is_prepared_ = is_prepared;
}

void FakeAttestationClient::ConfigureEnrollmentPreparationsSequence(
    std::deque<bool> sequence) {
  preparations_status_ = ::attestation::STATUS_SUCCESS;
  preparation_sequences_ = std::move(sequence);
}

void FakeAttestationClient::ConfigureEnrollmentPreparationsStatus(
    ::attestation::AttestationStatus status) {
  CHECK_NE(status, ::attestation::STATUS_SUCCESS);
  preparations_status_ = status;
}

void FakeAttestationClient::AllowlistCertificateRequest(
    const ::attestation::GetCertificateRequest& request) {
  for (const auto& req : allowlisted_requests_) {
    if (GetCertificateRequestEqual(req, request)) {
      return;
    }
  }
  allowlisted_requests_.push_back(request);
  certificate_indices_.push_back(kCertificateNotAssigned);
}

AttestationClient::TestInterface* FakeAttestationClient::GetTestInterface() {
  return this;
}

}  // namespace chromeos
