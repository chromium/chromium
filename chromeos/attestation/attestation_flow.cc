// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/attestation/attestation_flow.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "chromeos/attestation/attestation_flow_utils.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/attestation/attestation_client.h"
#include "chromeos/dbus/attestation/interface.pb.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "components/account_id/account_id.h"

namespace chromeos {
namespace attestation {

namespace {

// A reasonable timeout that gives enough time for attestation to be ready,
// yet does not make the caller wait too long.
constexpr uint16_t kReadyTimeoutInSeconds = 60;

// Delay before checking again whether the TPM has been prepared for
// attestation.
constexpr uint16_t kRetryDelayInMilliseconds = 300;

void DBusCertificateMethodCallback(
    AttestationFlow::CertificateCallback callback,
    base::Optional<CryptohomeClient::TpmAttestationDataResult> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Attestation: DBus data operation failed.";
    std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
    return;
  }
  if (callback) {
    std::move(callback).Run(
        result->success ? ATTESTATION_SUCCESS : ATTESTATION_UNSPECIFIED_FAILURE,
        result->data);
  }
}

}  // namespace

AttestationKeyType AttestationFlow::GetKeyTypeForProfile(
    AttestationCertificateProfile certificate_profile) {
  switch (certificate_profile) {
    case PROFILE_ENTERPRISE_MACHINE_CERTIFICATE:
    case PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE:
      return KEY_DEVICE;
    case PROFILE_ENTERPRISE_USER_CERTIFICATE:
    case PROFILE_CONTENT_PROTECTION_CERTIFICATE:
      return KEY_USER;
  }
  NOTREACHED();
  return KEY_USER;
}

AttestationFlow::AttestationFlow(cryptohome::AsyncMethodCaller* async_caller,
                                 CryptohomeClient* cryptohome_client,
                                 std::unique_ptr<ServerProxy> server_proxy)
    : async_caller_(async_caller),
      cryptohome_client_(cryptohome_client),
      attestation_client_(AttestationClient::Get()),
      server_proxy_(std::move(server_proxy)),
      ready_timeout_(base::TimeDelta::FromSeconds(kReadyTimeoutInSeconds)),
      retry_delay_(
          base::TimeDelta::FromMilliseconds(kRetryDelayInMilliseconds)) {}

AttestationFlow::~AttestationFlow() = default;

void AttestationFlow::GetCertificate(
    AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    bool force_new_key,
    const std::string& key_name,
    CertificateCallback callback) {
  std::string attestation_key_name =
      !key_name.empty()
          ? key_name
          : GetKeyNameForProfile(certificate_profile, request_origin);

  base::OnceCallback<void(bool)> start_certificate_request = base::BindOnce(
      &AttestationFlow::StartCertificateRequest, weak_factory_.GetWeakPtr(),
      certificate_profile, account_id, request_origin, force_new_key,
      attestation_key_name, std::move(callback));

  // If this device has not enrolled with the Privacy CA, we need to do that
  // first.  Once enrolled we can proceed with the certificate request.
  cryptohome_client_->TpmAttestationIsEnrolled(base::BindOnce(
      &AttestationFlow::OnEnrollmentCheckComplete, weak_factory_.GetWeakPtr(),
      std::move(start_certificate_request)));
}

void AttestationFlow::OnEnrollmentCheckComplete(
    base::OnceCallback<void(bool)> callback,
    base::Optional<bool> result) {
  if (!result) {
    LOG(ERROR) << "Attestation: Failed to check enrollment state.";
    std::move(callback).Run(false);
    return;
  }

  if (*result) {
    std::move(callback).Run(true);
    return;
  }

  base::TimeTicks end_time = base::TimeTicks::Now() + ready_timeout_;
  WaitForAttestationPrepared(end_time, std::move(callback));
}

void AttestationFlow::WaitForAttestationPrepared(
    base::TimeTicks end_time,
    base::OnceCallback<void(bool)> callback) {
  ::attestation::GetEnrollmentPreparationsRequest request;
  attestation_client_->GetEnrollmentPreparations(
      request, base::BindOnce(&AttestationFlow::OnPreparedCheckComplete,
                              weak_factory_.GetWeakPtr(), end_time,
                              std::move(callback)));
}

void AttestationFlow::OnPreparedCheckComplete(
    base::TimeTicks end_time,
    base::OnceCallback<void(bool)> callback,
    const ::attestation::GetEnrollmentPreparationsReply& reply) {
  if (AttestationClient::IsAttestationPrepared(reply)) {
    // Get the attestation service to create a Privacy CA enrollment request.
    async_caller_->AsyncTpmAttestationCreateEnrollRequest(
        server_proxy_->GetType(),
        base::BindOnce(&AttestationFlow::SendEnrollRequestToPCA,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  if (base::TimeTicks::Now() < end_time) {
    LOG(WARNING) << "Attestation: Not prepared yet."
                 << " Retrying in " << retry_delay_ << ".";
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AttestationFlow::WaitForAttestationPrepared,
                       weak_factory_.GetWeakPtr(), end_time,
                       std::move(callback)),
        retry_delay_);
    return;
  }

  LOG(ERROR) << "Attestation: Not prepared. Giving up on retrying.";
  std::move(callback).Run(false);
}

void AttestationFlow::SendEnrollRequestToPCA(
    base::OnceCallback<void(bool)> callback,
    bool success,
    const std::string& data) {
  if (!success) {
    LOG(ERROR) << "Attestation: Failed to create enroll request.";
    std::move(callback).Run(false);
    return;
  }

  // Send the request to the Privacy CA.
  server_proxy_->SendEnrollRequest(
      data, base::BindOnce(&AttestationFlow::SendEnrollResponseToDaemon,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttestationFlow::SendEnrollResponseToDaemon(
    base::OnceCallback<void(bool)> callback,
    bool success,
    const std::string& data) {
  if (!success) {
    LOG(ERROR) << "Attestation: Enroll request failed.";
    std::move(callback).Run(false);
    return;
  }

  // Forward the response to the attestation service to complete enrollment.
  async_caller_->AsyncTpmAttestationEnroll(
      server_proxy_->GetType(), data,
      base::BindOnce(&AttestationFlow::OnEnrollComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttestationFlow::OnEnrollComplete(base::OnceCallback<void(bool)> callback,
                                       bool success,
                                       cryptohome::MountError /*not_used*/) {
  if (!success) {
    LOG(ERROR) << "Attestation: Failed to complete enrollment.";
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

void AttestationFlow::StartCertificateRequest(
    AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    bool generate_new_key,
    const std::string& key_name,
    CertificateCallback callback,
    bool enrolled) {
  if (!enrolled) {
    std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
    return;
  }

  AttestationKeyType key_type = GetKeyTypeForProfile(certificate_profile);
  if (generate_new_key) {
    // Get the attestation service to create a Privacy CA certificate request.
    async_caller_->AsyncTpmAttestationCreateCertRequest(
        server_proxy_->GetType(), certificate_profile,
        cryptohome::Identification(account_id), request_origin,
        base::BindOnce(&AttestationFlow::SendCertificateRequestToPCA,
                       weak_factory_.GetWeakPtr(), key_type, account_id,
                       key_name, std::move(callback)));
    return;
  }

  cryptohome_client_->TpmAttestationDoesKeyExist(
      key_type, cryptohome::CreateAccountIdentifierFromAccountId(account_id),
      key_name,
      base::BindOnce(&AttestationFlow::OnKeyExistCheckComplete,
                     weak_factory_.GetWeakPtr(), certificate_profile,
                     account_id, request_origin, key_name, key_type,
                     std::move(callback)));
}

void AttestationFlow::OnKeyExistCheckComplete(
    AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    const std::string& key_name,
    AttestationKeyType key_type,
    CertificateCallback callback,
    base::Optional<bool> result) {
  if (!result) {
    LOG(ERROR) << "Attestation: Failed to check for existence of key.";
    std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
    return;
  }

  // If the key already exists, query the existing certificate.
  if (*result) {
    GetExistingCertificate(key_type, account_id, key_name, std::move(callback));
    return;
  }

  // If the key does not exist, call this method back with |generate_new_key|
  // set to true.
  StartCertificateRequest(certificate_profile, account_id, request_origin, true,
                          key_name, std::move(callback), true);
}

void AttestationFlow::SendCertificateRequestToPCA(AttestationKeyType key_type,
                                                  const AccountId& account_id,
                                                  const std::string& key_name,
                                                  CertificateCallback callback,
                                                  bool success,
                                                  const std::string& data) {
  if (!success) {
    LOG(ERROR) << "Attestation: Failed to create certificate request.";
    std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
    return;
  }

  // Send the request to the Privacy CA.
  server_proxy_->SendCertificateRequest(
      data, base::BindOnce(&AttestationFlow::SendCertificateResponseToDaemon,
                           weak_factory_.GetWeakPtr(), key_type, account_id,
                           key_name, std::move(callback)));
}

void AttestationFlow::SendCertificateResponseToDaemon(
    AttestationKeyType key_type,
    const AccountId& account_id,
    const std::string& key_name,
    CertificateCallback callback,
    bool success,
    const std::string& data) {
  if (!success) {
    LOG(ERROR) << "Attestation: Certificate request failed.";
    std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
    return;
  }

  // Forward the response to the attestation service to complete the operation.
  async_caller_->AsyncTpmAttestationFinishCertRequest(
      data, key_type, cryptohome::Identification(account_id), key_name,
      base::BindOnce(&AttestationFlow::OnCertRequestFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttestationFlow::OnCertRequestFinished(CertificateCallback callback,
                                            bool success,
                                            const std::string& data) {
  if (success)
    std::move(callback).Run(ATTESTATION_SUCCESS, data);
  else
    std::move(callback).Run(ATTESTATION_SERVER_BAD_REQUEST_FAILURE, data);
}

void AttestationFlow::GetExistingCertificate(AttestationKeyType key_type,
                                             const AccountId& account_id,
                                             const std::string& key_name,
                                             CertificateCallback callback) {
  cryptohome_client_->TpmAttestationGetCertificate(
      key_type, cryptohome::CreateAccountIdentifierFromAccountId(account_id),
      key_name,
      base::BindOnce(&DBusCertificateMethodCallback, std::move(callback)));
}

ServerProxy::~ServerProxy() = default;

PrivacyCAType ServerProxy::GetType() {
  return DEFAULT_PCA;
}

}  // namespace attestation
}  // namespace chromeos
