// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/attestation/fake_attestation_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/cros_system_api/dbus/attestation/dbus-constants.h"

namespace ash {
namespace {

constexpr int kCertificateNotAssigned = 0;
constexpr char kFakeCertPrefix[] = "fake cert";
constexpr char kResponseSuffix[] = "_response";
constexpr char kSignatureSuffix[] = "_signature";
constexpr char kEnterpriseChallengeResponseSuffix[] = "enterprise_challenge";
constexpr char kIncludeSpkacSuffix[] = "_with_spkac";
constexpr char kFakePcaEnrollRequest[] = "fake enroll request";
constexpr char kFakePcaEnrollResponse[] = "fake enroll response";
constexpr char kFakePcaCertRequest[] = "fake cert request";
constexpr char kFakePcaCertResponse[] = "fake cert response";
constexpr char kFakeCertificate[] = "fake certificate";

// Posts `callback` on the current thread's task runner, passing it the
// `reply` message.
template <class ReplyType>
void PostProtoResponse(base::OnceCallback<void(const ReplyType&)> callback,
                       const ReplyType& reply) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reply));
}

// Posts `callback` on the current thread's task runner, passing it the
// `reply` message with `delay`.
template <class ReplyType>
void PostProtoResponseWithDelay(
    base::OnceCallback<void(const ReplyType&)> callback,
    const ReplyType& reply,
    const base::TimeDelta& delay) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), reply), delay);
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

FakeAttestationClient::FakeAttestationClient() {
  features_reply_.set_is_available(true);
  status_reply_.set_enrolled(true);
}

FakeAttestationClient::~FakeAttestationClient() = default;

void FakeAttestationClient::GetKeyInfo(
    const ::attestation::GetKeyInfoRequest& request,
    GetKeyInfoCallback callback) {
  ::attestation::GetKeyInfoReply reply;
  if (key_info_dbus_error_count_ > 0) {
    reply.set_status(::attestation::STATUS_DBUS_ERROR);
    key_info_dbus_error_count_--;
  } else {
    auto iter = key_info_database_.find(request);
    if (iter != key_info_database_.end()) {
      reply = iter->second;
    } else {
      reply.set_status(::attestation::STATUS_INVALID_PARAMETER);
    }
  }

  PostProtoResponse(std::move(callback), reply);
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
  ::attestation::SignReply reply;
  reply.set_status(sign_status_);
  if (reply.status() == ::attestation::STATUS_SUCCESS) {
    reply.set_signature(kSignatureSuffix);
  }
  PostProtoResponse(std::move(callback), reply);
}

void FakeAttestationClient::RegisterKeyWithChapsToken(
    const ::attestation::RegisterKeyWithChapsTokenRequest& request,
    RegisterKeyWithChapsTokenCallback callback) {
  ::attestation::RegisterKeyWithChapsTokenReply reply;
  if (allowlisted_register_keys_.count(
          {request.username(), request.key_label()}) == 0) {
    reply.set_status(::attestation::STATUS_INVALID_PARAMETER);
  } else {
    reply.set_status(register_key_status_);
  }
  PostProtoResponse(std::move(callback), reply);
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

void FakeAttestationClient::GetFeatures(
    const ::attestation::GetFeaturesRequest& request,
    GetFeaturesCallback callback) {
  PostProtoResponse(std::move(callback), features_reply_);
}

void FakeAttestationClient::GetStatus(
    const ::attestation::GetStatusRequest& request,
    GetStatusCallback callback) {
  PostProtoResponse(std::move(callback), status_reply_);
}

void FakeAttestationClient::Verify(const ::attestation::VerifyRequest& request,
                                   VerifyCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::CreateEnrollRequest(
    const ::attestation::CreateEnrollRequestRequest& request,
    CreateEnrollRequestCallback callback) {
  ::attestation::CreateEnrollRequestReply reply;
  reply.set_status(enroll_request_status_);
  if (aca_type_for_legacy_mode_ != request.aca_type()) {
    reply.set_status(::attestation::STATUS_UNEXPECTED_DEVICE_ERROR);
  }
  if (reply.status() == ::attestation::STATUS_SUCCESS) {
    reply.set_pca_request(GetFakePcaEnrollRequest());
  }
  PostProtoResponse(std::move(callback), reply);
}

void FakeAttestationClient::FinishEnroll(
    const ::attestation::FinishEnrollRequest& request,
    FinishEnrollCallback callback) {
  ::attestation::FinishEnrollReply reply;
  reply.set_status(request.pca_response() == GetFakePcaEnrollResponse()
                       ? ::attestation::STATUS_SUCCESS
                       : ::attestation::STATUS_UNEXPECTED_DEVICE_ERROR);
  if (aca_type_for_legacy_mode_ != request.aca_type()) {
    reply.set_status(::attestation::STATUS_UNEXPECTED_DEVICE_ERROR);
  }
  PostProtoResponse(std::move(callback), reply);
}

void FakeAttestationClient::CreateCertificateRequest(
    const ::attestation::CreateCertificateRequestRequest& request,
    CreateCertificateRequestCallback callback) {
  ::attestation::CreateCertificateRequestReply reply;
  reply.set_status(cert_request_status_);
  if (aca_type_for_legacy_mode_ != request.aca_type()) {
    reply.set_status(::attestation::STATUS_UNEXPECTED_DEVICE_ERROR);
  }
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    PostProtoResponse(std::move(callback), reply);
    return;
  }
  for (const auto& req : allowlisted_create_requests_) {
    if (req.username() == request.username() &&
        req.request_origin() == request.request_origin() &&
        req.certificate_profile() == request.certificate_profile() &&
        req.key_type() == request.key_type()) {
      reply.set_pca_request(GetFakePcaCertRequest());
      PostProtoResponse(std::move(callback), reply);
      return;
    }
  }
  reply.set_status(::attestation::STATUS_UNEXPECTED_DEVICE_ERROR);
  PostProtoResponse(std::move(callback), reply);
}

void FakeAttestationClient::FinishCertificateRequest(
    const ::attestation::FinishCertificateRequestRequest& request,
    FinishCertificateRequestCallback callback) {
  ::attestation::FinishCertificateRequestReply reply;
  if (request.pca_response() != GetFakePcaCertResponse()) {
    reply.set_status(::attestation::STATUS_UNEXPECTED_DEVICE_ERROR);
  }
  if (reply.status() == ::attestation::STATUS_SUCCESS) {
    reply.set_certificate(GetFakeCertificate());
    // Also, insert the certificate into the fake database.
    GetMutableKeyInfoReply(request.username(), request.key_label())
        ->set_certificate(GetFakeCertificate());
  }
  PostProtoResponse(std::move(callback), reply);
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

  if (request.shall_trigger_enrollment()) {
    status_reply_.set_enrolled(true);
  }
  if (!status_reply_.enrolled()) {
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
  ::attestation::SignEnterpriseChallengeReply reply;
  if (allowlisted_sign_enterprise_challenge_keys_.count(request) == 0) {
    reply.set_status(::attestation::STATUS_INVALID_PARAMETER);
  } else {
    reply.set_status(sign_enterprise_challenge_status_);
  }
  if (reply.status() == ::attestation::STATUS_SUCCESS) {
    reply.set_challenge_response(GetEnterpriseChallengeFakeSignature(
        request.challenge(), request.include_signed_public_key()));
  }
  PostProtoResponseWithDelay(std::move(callback), reply,
                             sign_enterprise_challenge_delay_);
}

void FakeAttestationClient::SignSimpleChallenge(
    const ::attestation::SignSimpleChallengeRequest& request,
    SignSimpleChallengeCallback callback) {
  ::attestation::SignSimpleChallengeReply reply;
  if (allowlisted_sign_simple_challenge_keys_.count(
          {request.username(), request.key_label()}) == 0) {
    reply.set_status(::attestation::STATUS_INVALID_PARAMETER);
  } else {
    reply.set_status(sign_simple_challenge_status_);
  }
  if (reply.status() == ::attestation::STATUS_SUCCESS) {
    ::attestation::SignedData signed_data;
    signed_data.set_data(request.challenge() + kResponseSuffix);
    signed_data.set_signature(request.challenge() + kSignatureSuffix);
    reply.set_challenge_response(signed_data.SerializeAsString());
  }
  PostProtoResponse(std::move(callback), reply);
}

void FakeAttestationClient::SetKeyPayload(
    const ::attestation::SetKeyPayloadRequest& request,
    SetKeyPayloadCallback callback) {
  ::attestation::GetKeyInfoRequest get_key_info_request;
  get_key_info_request.set_username(request.username());
  get_key_info_request.set_key_label(request.key_label());
  auto iter = key_info_database_.find(get_key_info_request);
  ::attestation::SetKeyPayloadReply reply;
  if (iter == key_info_database_.end()) {
    reply.set_status(::attestation::STATUS_INVALID_PARAMETER);
  } else {
    iter->second.set_payload(request.payload());
  }
  PostProtoResponse(std::move(callback), reply);
}

void FakeAttestationClient::DeleteKeys(
    const ::attestation::DeleteKeysRequest& request,
    DeleteKeysCallback callback) {
  delete_keys_history_.push_back(request);
  ::attestation::DeleteKeysReply reply;
  reply.set_status(delete_keys_status_);
  PostProtoResponse(std::move(callback), reply);
}

void FakeAttestationClient::ResetIdentity(
    const ::attestation::ResetIdentityRequest& request,
    ResetIdentityCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::GetEnrollmentId(
    const ::attestation::GetEnrollmentIdRequest& request,
    GetEnrollmentIdCallback callback) {
  ::attestation::GetEnrollmentIdReply reply;
  if (enrollment_id_dbus_error_count_ != 0) {
    reply.set_status(::attestation::STATUS_DBUS_ERROR);
    enrollment_id_dbus_error_count_--;
  } else {
    reply.set_status(::attestation::STATUS_SUCCESS);
    reply.set_enrollment_id(request.ignore_cache() ? enrollment_id_ignore_cache_
                                                   : enrollment_id_);
  }
  PostProtoResponse(std::move(callback), reply);
}

void FakeAttestationClient::GetCertifiedNvIndex(
    const ::attestation::GetCertifiedNvIndexRequest& request,
    GetCertifiedNvIndexCallback callback) {
  NOTIMPLEMENTED();
}

void FakeAttestationClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  std::move(callback).Run(/*service_is_available=*/true);
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

::attestation::GetFeaturesReply*
FakeAttestationClient::mutable_features_reply() {
  return &features_reply_;
}

::attestation::GetStatusReply* FakeAttestationClient::mutable_status_reply() {
  return &status_reply_;
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

const std::vector<::attestation::DeleteKeysRequest>&
FakeAttestationClient::delete_keys_history() const {
  return delete_keys_history_;
}

void FakeAttestationClient::ClearDeleteKeysHistory() {
  delete_keys_history_.clear();
}

void FakeAttestationClient::set_enrollment_id_ignore_cache(
    const std::string& id) {
  enrollment_id_ignore_cache_ = id;
}

void FakeAttestationClient::set_cached_enrollment_id(const std::string& id) {
  enrollment_id_ = id;
}

void FakeAttestationClient::set_enrollment_id_dbus_error_count(int count) {
  enrollment_id_dbus_error_count_ = count;
}

::attestation::GetKeyInfoReply* FakeAttestationClient::GetMutableKeyInfoReply(
    const std::string& username,
    const std::string& label) {
  ::attestation::GetKeyInfoRequest request;
  request.set_username(username);
  request.set_key_label(label);
  // If there doesn't exist the entry yet, just create a new one.
  return &(key_info_database_[request]);
}

void FakeAttestationClient::set_key_info_dbus_error_count(int count) {
  key_info_dbus_error_count_ = count;
}

int FakeAttestationClient::key_info_dbus_error_count() const {
  return key_info_dbus_error_count_;
}

bool FakeAttestationClient::VerifySimpleChallengeResponse(
    const std::string& challenge,
    const ::attestation::SignedData& signed_data) {
  return signed_data.data() == challenge + kResponseSuffix &&
         signed_data.signature() == challenge + kSignatureSuffix;
}

void FakeAttestationClient::set_sign_simple_challenge_status(
    ::attestation::AttestationStatus status) {
  sign_simple_challenge_status_ = status;
}

void FakeAttestationClient::set_sign_status(
    ::attestation::AttestationStatus status) {
  sign_status_ = status;
}

void FakeAttestationClient::AllowlistSignSimpleChallengeKey(
    const std::string& username,
    const std::string& label) {
  allowlisted_sign_simple_challenge_keys_.insert({username, label});
}

void FakeAttestationClient::set_register_key_status(
    ::attestation::AttestationStatus status) {
  register_key_status_ = status;
}

void FakeAttestationClient::AllowlistRegisterKey(const std::string& username,
                                                 const std::string& label) {
  allowlisted_register_keys_.insert({username, label});
}

void FakeAttestationClient::set_sign_enterprise_challenge_status(
    ::attestation::AttestationStatus status) {
  sign_enterprise_challenge_status_ = status;
}

void FakeAttestationClient::AllowlistSignEnterpriseChallengeKey(
    const ::attestation::SignEnterpriseChallengeRequest& request) {
  allowlisted_sign_enterprise_challenge_keys_.insert(request);
}

std::string FakeAttestationClient::GetEnterpriseChallengeFakeSignature(
    const std::string& challenge,
    bool include_spkac) const {
  std::string challenge_response =
      challenge + kEnterpriseChallengeResponseSuffix;
  if (include_spkac) {
    challenge_response += kIncludeSpkacSuffix;
  }
  return challenge_response;
}

void FakeAttestationClient::set_sign_enterprise_challenge_delay(
    const base::TimeDelta& delay) {
  sign_enterprise_challenge_delay_ = delay;
}

void FakeAttestationClient::set_aca_type_for_legacy_flow(
    ::attestation::ACAType aca_type) {
  aca_type_for_legacy_mode_ = aca_type;
}

void FakeAttestationClient::set_enroll_request_status(
    ::attestation::AttestationStatus status) {
  enroll_request_status_ = status;
}

std::string FakeAttestationClient::GetFakePcaEnrollRequest() const {
  return kFakePcaEnrollRequest;
}

std::string FakeAttestationClient::GetFakePcaEnrollResponse() const {
  return kFakePcaEnrollResponse;
}

void FakeAttestationClient::AllowlistLegacyCreateCertificateRequest(
    const std::string& username,
    const std::string& request_origin,
    ::attestation::CertificateProfile profile,
    ::attestation::KeyType key_type) {
  ::attestation::CreateCertificateRequestRequest request;
  request.set_username(username);
  request.set_request_origin(request_origin);
  request.set_certificate_profile(profile);
  request.set_key_type(key_type);
  allowlisted_create_requests_.push_back(request);
}

void FakeAttestationClient::set_cert_request_status(
    ::attestation::AttestationStatus status) {
  cert_request_status_ = status;
}

std::string FakeAttestationClient::GetFakePcaCertRequest() const {
  return kFakePcaCertRequest;
}

std::string FakeAttestationClient::GetFakePcaCertResponse() const {
  return kFakePcaCertResponse;
}

std::string FakeAttestationClient::GetFakeCertificate() const {
  return kFakeCertificate;
}

void FakeAttestationClient::set_delete_keys_status(
    ::attestation::AttestationStatus status) {
  delete_keys_status_ = status;
}

AttestationClient::TestInterface* FakeAttestationClient::GetTestInterface() {
  return this;
}

}  // namespace ash
