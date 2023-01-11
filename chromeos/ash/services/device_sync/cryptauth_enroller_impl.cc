// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_enroller_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate.h"
#include "chromeos/ash/services/device_sync/cryptauth_client_impl.h"
#include "crypto/sha2.h"

namespace ash {

namespace device_sync {

namespace {

// A successful SetupEnrollment or FinishEnrollment response should contain this
// status string.
const char kResponseStatusOk[] = "ok";

// The name of the "gcmV1" protocol that the enrolling device supports.
const char kSupportedEnrollmentTypeGcmV1[] = "gcmV1";

// The version field of the GcmMetadata message.
const int kGCMMetadataVersion = 1;

// Returns true if |device_info| contains the required fields for enrollment.
bool ValidateDeviceInfo(const cryptauth::GcmDeviceInfo& device_info) {
  if (!device_info.has_long_device_id()) {
    PA_LOG(ERROR)
        << "Expected long_device_id field in cryptauth::GcmDeviceInfo.";
    return false;
  }

  if (!device_info.has_device_type()) {
    PA_LOG(ERROR) << "Expected device_type field in cryptauth::GcmDeviceInfo.";
    return false;
  }

  return true;
}

// Creates the public metadata to put in the SecureMessage that is sent to the
// server with the FinishEnrollment request.
std::string CreateEnrollmentPublicMetadata() {
  cryptauth::GcmMetadata metadata;
  metadata.set_version(kGCMMetadataVersion);
  metadata.set_type(cryptauth::MessageType::ENROLLMENT);
  return metadata.SerializeAsString();
}

void RecordEnrollmentResult(bool success) {
  UMA_HISTOGRAM_BOOLEAN("CryptAuth.Enrollment.Result", success);
}

}  // namespace

CryptAuthEnrollerImpl::CryptAuthEnrollerImpl(
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<multidevice::SecureMessageDelegate> secure_message_delegate)
    : client_factory_(client_factory),
      secure_message_delegate_(std::move(secure_message_delegate)) {}

CryptAuthEnrollerImpl::~CryptAuthEnrollerImpl() {}

void CryptAuthEnrollerImpl::Enroll(
    const std::string& user_public_key,
    const std::string& user_private_key,
    const cryptauth::GcmDeviceInfo& device_info,
    cryptauth::InvocationReason invocation_reason,
    EnrollmentFinishedCallback callback) {
  if (enroll_called_) {
    PA_LOG(ERROR) << "Enroll() already called. Do not reuse.";
    std::move(callback).Run(false);
    return;
  }

  user_public_key_ = user_public_key;
  user_private_key_ = user_private_key;
  device_info_ = device_info;
  invocation_reason_ = invocation_reason;
  callback_ = std::move(callback);
  enroll_called_ = true;

  if (!ValidateDeviceInfo(device_info)) {
    std::move(callback_).Run(false);
    return;
  }

  secure_message_delegate_->GenerateKeyPair(
      base::BindOnce(&CryptAuthEnrollerImpl::OnKeyPairGenerated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthEnrollerImpl::OnKeyPairGenerated(const std::string& public_key,
                                               const std::string& private_key) {
  PA_LOG(VERBOSE)
      << "Ephemeral key pair generated, calling SetupEnrollment API.";
  session_public_key_ = public_key;
  session_private_key_ = private_key;

  cryptauth_client_ = client_factory_->CreateInstance();
  cryptauth::SetupEnrollmentRequest request;
  request.add_types(kSupportedEnrollmentTypeGcmV1);
  request.set_invocation_reason(invocation_reason_);
  cryptauth_client_->SetupEnrollment(
      request,
      base::BindOnce(&CryptAuthEnrollerImpl::OnSetupEnrollmentSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&CryptAuthEnrollerImpl::OnSetupEnrollmentFailure,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthEnrollerImpl::OnSetupEnrollmentSuccess(
    const cryptauth::SetupEnrollmentResponse& response) {
  if (response.status() != kResponseStatusOk) {
    PA_LOG(WARNING) << "Unexpected status for SetupEnrollment: "
                    << response.status();
    std::move(callback_).Run(false);
    return;
  }

  if (response.infos_size() == 0) {
    PA_LOG(ERROR) << "No response info returned by server for SetupEnrollment";
    std::move(callback_).Run(false);
    return;
  }

  PA_LOG(VERBOSE)
      << "SetupEnrollment request succeeded: deriving symmetric key.";
  setup_info_ = response.infos(0);
  device_info_.set_enrollment_session_id(setup_info_.enrollment_session_id());

  secure_message_delegate_->DeriveKey(
      session_private_key_, setup_info_.server_ephemeral_key(),
      base::BindOnce(&CryptAuthEnrollerImpl::OnKeyDerived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthEnrollerImpl::OnSetupEnrollmentFailure(
    NetworkRequestError error) {
  PA_LOG(WARNING) << "SetupEnrollment API failed with error: " << error;
  std::move(callback_).Run(false);
}

void CryptAuthEnrollerImpl::OnKeyDerived(const std::string& symmetric_key) {
  PA_LOG(VERBOSE) << "Derived symmetric key, "
                  << "encrypting enrollment data for upload.";

  // Make sure we're enrolling the same public key used below to sign the
  // secure message.
  device_info_.set_user_public_key(user_public_key_);
  device_info_.set_key_handle(user_public_key_);

  // Hash the symmetric key and add it to the |device_info_| to be uploaded.
  device_info_.set_device_authzen_key_hash(
      crypto::SHA256HashString(symmetric_key));

  // The server verifies that the access token set here and in the header
  // of the FinishEnrollment() request are the same.
  device_info_.set_oauth_token(cryptauth_client_->GetAccessTokenUsed());
  PA_LOG(VERBOSE) << "Using access token: " << device_info_.oauth_token();

  symmetric_key_ = symmetric_key;
  multidevice::SecureMessageDelegate::CreateOptions options;
  options.encryption_scheme = securemessage::NONE;
  options.signature_scheme = securemessage::ECDSA_P256_SHA256;
  options.verification_key_id = user_public_key_;

  // The inner message contains the signed device information that will be
  // sent to CryptAuth.
  secure_message_delegate_->CreateSecureMessage(
      device_info_.SerializeAsString(), user_private_key_, options,
      base::BindOnce(&CryptAuthEnrollerImpl::OnInnerSecureMessageCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthEnrollerImpl::OnInnerSecureMessageCreated(
    const std::string& inner_message) {
  if (inner_message.empty()) {
    PA_LOG(ERROR) << "Error creating inner message";
    std::move(callback_).Run(false);
    return;
  }

  multidevice::SecureMessageDelegate::CreateOptions options;
  options.encryption_scheme = securemessage::AES_256_CBC;
  options.signature_scheme = securemessage::HMAC_SHA256;
  options.public_metadata = CreateEnrollmentPublicMetadata();

  // The outer message encrypts and signs the inner message with the derived
  // symmetric session key.
  secure_message_delegate_->CreateSecureMessage(
      inner_message, symmetric_key_, options,
      base::BindOnce(&CryptAuthEnrollerImpl::OnOuterSecureMessageCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthEnrollerImpl::OnOuterSecureMessageCreated(
    const std::string& outer_message) {
  PA_LOG(VERBOSE) << "SecureMessage created, calling FinishEnrollment API.";

  cryptauth::FinishEnrollmentRequest request;
  request.set_enrollment_session_id(setup_info_.enrollment_session_id());
  request.set_enrollment_message(outer_message);
  request.set_device_ephemeral_key(session_public_key_);
  request.set_invocation_reason(invocation_reason_);

  cryptauth_client_ = client_factory_->CreateInstance();
  cryptauth_client_->FinishEnrollment(
      request,
      base::BindOnce(&CryptAuthEnrollerImpl::OnFinishEnrollmentSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&CryptAuthEnrollerImpl::OnFinishEnrollmentFailure,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthEnrollerImpl::OnFinishEnrollmentSuccess(
    const cryptauth::FinishEnrollmentResponse& response) {
  const bool success = response.status() == kResponseStatusOk;

  if (!success) {
    PA_LOG(WARNING) << "Unexpected status for FinishEnrollment: "
                    << response.status();
  }

  RecordEnrollmentResult(success);
  std::move(callback_).Run(success);
}

void CryptAuthEnrollerImpl::OnFinishEnrollmentFailure(
    NetworkRequestError error) {
  PA_LOG(WARNING) << "FinishEnrollment API failed with error: " << error;
  RecordEnrollmentResult(false /* success */);
  std::move(callback_).Run(false);
}

}  // namespace device_sync

}  // namespace ash
