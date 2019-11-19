// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/attestation/attestation_flow.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
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

// Redirects to one of three callbacks based on a boolean value and dbus call
// status.
//
// Parameters
//   on_true - Called when status=success and value=true.
//   on_false - Called when status=success and value=false.
//   on_fail - Called when status=failure.
//   result - The result returned by the D-Bus operation.
void DBusBoolRedirectCallback(const base::Closure& on_true,
                              const base::Closure& on_false,
                              const base::Closure& on_fail,
                              const std::string& on_fail_message,
                              base::Optional<bool> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Attestation: Failed to " << on_fail_message << ".";
    if (!on_fail.is_null())
      on_fail.Run();
    return;
  }
  const base::Closure& task = result.value() ? on_true : on_false;
  if (!task.is_null())
    task.Run();
}

void DBusCertificateMethodCallback(
    const AttestationFlow::CertificateCallback& callback,
    base::Optional<CryptohomeClient::TpmAttestationDataResult> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Attestation: DBus data operation failed.";
    if (!callback.is_null())
      callback.Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
    return;
  }
  if (!callback.is_null()) {
    callback.Run(
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

std::string AttestationFlow::GetKeyNameForProfile(
    AttestationCertificateProfile certificate_profile,
    const std::string& request_origin) {
  switch (certificate_profile) {
    case PROFILE_ENTERPRISE_MACHINE_CERTIFICATE:
      return kEnterpriseMachineKey;
    case PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE:
      return kEnterpriseEnrollmentKey;
    case PROFILE_ENTERPRISE_USER_CERTIFICATE:
      return kEnterpriseUserKey;
    case PROFILE_CONTENT_PROTECTION_CERTIFICATE:
      return std::string(kContentProtectionKeyPrefix) + request_origin;
  }
  NOTREACHED();
  return "";
}

AttestationFlow::AttestationFlow(cryptohome::AsyncMethodCaller* async_caller,
                                 CryptohomeClient* cryptohome_client,
                                 std::unique_ptr<ServerProxy> server_proxy)
    : async_caller_(async_caller),
      cryptohome_client_(cryptohome_client),
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
    const CertificateCallback& callback) {
  // If this device has not enrolled with the Privacy CA, we need to do that
  // first.  Once enrolled we can proceed with the certificate request.
  const base::Closure do_cert_request =
      base::Bind(&AttestationFlow::StartCertificateRequest,
                 weak_factory_.GetWeakPtr(), certificate_profile, account_id,
                 request_origin, force_new_key, key_name, callback);
  const base::RepeatingClosure on_failure =
      base::BindRepeating(callback, ATTESTATION_UNSPECIFIED_FAILURE, "");
  const base::Closure initiate_enroll = base::Bind(
      &AttestationFlow::WaitForAttestationReadyAndStartEnroll,
      weak_factory_.GetWeakPtr(), base::TimeTicks::Now() + ready_timeout_,
      on_failure,
      base::Bind(&AttestationFlow::StartEnroll, weak_factory_.GetWeakPtr(),
                 on_failure, do_cert_request));
  cryptohome_client_->TpmAttestationIsEnrolled(base::BindOnce(
      &DBusBoolRedirectCallback,
      do_cert_request,  // If enrolled, proceed with cert request.
      initiate_enroll,  // If not enrolled, initiate enrollment.
      on_failure, "check enrollment state"));
}

void AttestationFlow::WaitForAttestationReadyAndStartEnroll(
    base::TimeTicks end_time,
    const base::Closure& on_failure,
    const base::Closure& next_task) {
  const base::Closure retry_initiate_enroll =
      base::Bind(&AttestationFlow::CheckAttestationReadyAndReschedule,
                 weak_factory_.GetWeakPtr(), end_time, on_failure, next_task);
  cryptohome_client_->TpmAttestationIsPrepared(base::BindOnce(
      &DBusBoolRedirectCallback, next_task, retry_initiate_enroll, on_failure,
      "check for attestation readiness"));
}

void AttestationFlow::StartEnroll(const base::Closure& on_failure,
                                  const base::Closure& next_task) {
  // Get the attestation service to create a Privacy CA enrollment request.
  async_caller_->AsyncTpmAttestationCreateEnrollRequest(
      server_proxy_->GetType(),
      base::Bind(&AttestationFlow::SendEnrollRequestToPCA,
                 weak_factory_.GetWeakPtr(), on_failure, next_task));
}

void AttestationFlow::SendEnrollRequestToPCA(const base::Closure& on_failure,
                                             const base::Closure& next_task,
                                             bool success,
                                             const std::string& data) {
  if (!success) {
    LOG(ERROR) << "Attestation: Failed to create enroll request.";
    if (!on_failure.is_null())
      on_failure.Run();
    return;
  }

  // Send the request to the Privacy CA.
  server_proxy_->SendEnrollRequest(
      data, base::Bind(&AttestationFlow::SendEnrollResponseToDaemon,
                       weak_factory_.GetWeakPtr(), on_failure, next_task));
}

void AttestationFlow::SendEnrollResponseToDaemon(
    const base::Closure& on_failure,
    const base::Closure& next_task,
    bool success,
    const std::string& data) {
  if (!success) {
    LOG(ERROR) << "Attestation: Enroll request failed.";
    if (!on_failure.is_null())
      on_failure.Run();
    return;
  }

  // Forward the response to the attestation service to complete enrollment.
  async_caller_->AsyncTpmAttestationEnroll(
      server_proxy_->GetType(), data,
      base::Bind(&AttestationFlow::OnEnrollComplete, weak_factory_.GetWeakPtr(),
                 on_failure, next_task));
}

void AttestationFlow::OnEnrollComplete(const base::Closure& on_failure,
                                       const base::Closure& next_task,
                                       bool success,
                                       cryptohome::MountError /*not_used*/) {
  if (!success) {
    LOG(ERROR) << "Attestation: Failed to complete enrollment.";
    if (!on_failure.is_null())
      on_failure.Run();
    return;
  }

  // Enrollment has successfully completed, we can move on to whatever is next.
  if (!next_task.is_null())
    next_task.Run();
}

void AttestationFlow::StartCertificateRequest(
    AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    bool generate_new_key,
    const std::string& key_name,
    const CertificateCallback& callback) {
  AttestationKeyType key_type = GetKeyTypeForProfile(certificate_profile);
  std::string attestation_key_name =
      !key_name.empty()
          ? key_name
          : GetKeyNameForProfile(certificate_profile, request_origin);
  if (generate_new_key) {
    // Get the attestation service to create a Privacy CA certificate request.
    async_caller_->AsyncTpmAttestationCreateCertRequest(
        server_proxy_->GetType(), certificate_profile,
        cryptohome::Identification(account_id), request_origin,
        base::Bind(&AttestationFlow::SendCertificateRequestToPCA,
                   weak_factory_.GetWeakPtr(), key_type, account_id,
                   attestation_key_name, callback));
  } else {
    // If the key already exists, query the existing certificate.
    const base::Closure on_key_exists = base::Bind(
        &AttestationFlow::GetExistingCertificate, weak_factory_.GetWeakPtr(),
        key_type, account_id, attestation_key_name, callback);
    // If the key does not exist, call this method back with |generate_new_key|
    // set to true.
    const base::Closure on_key_not_exists =
        base::Bind(&AttestationFlow::StartCertificateRequest,
                   weak_factory_.GetWeakPtr(), certificate_profile, account_id,
                   request_origin, true, attestation_key_name, callback);
    cryptohome_client_->TpmAttestationDoesKeyExist(
        key_type, cryptohome::CreateAccountIdentifierFromAccountId(account_id),
        attestation_key_name,
        base::BindOnce(
            &DBusBoolRedirectCallback, on_key_exists, on_key_not_exists,
            base::BindRepeating(callback, ATTESTATION_UNSPECIFIED_FAILURE, ""),
            "check for existence of attestation key"));
  }
}

void AttestationFlow::SendCertificateRequestToPCA(
    AttestationKeyType key_type,
    const AccountId& account_id,
    const std::string& key_name,
    const CertificateCallback& callback,
    bool success,
    const std::string& data) {
  if (!success) {
    LOG(ERROR) << "Attestation: Failed to create certificate request.";
    if (!callback.is_null())
      callback.Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
    return;
  }

  // Send the request to the Privacy CA.
  server_proxy_->SendCertificateRequest(
      data, base::Bind(&AttestationFlow::SendCertificateResponseToDaemon,
                       weak_factory_.GetWeakPtr(), key_type, account_id,
                       key_name, callback));
}

void AttestationFlow::SendCertificateResponseToDaemon(
    AttestationKeyType key_type,
    const AccountId& account_id,
    const std::string& key_name,
    const CertificateCallback& callback,
    bool success,
    const std::string& data) {
  if (!success) {
    LOG(ERROR) << "Attestation: Certificate request failed.";
    if (!callback.is_null())
      callback.Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
    return;
  }

  // Forward the response to the attestation service to complete the operation.
  async_caller_->AsyncTpmAttestationFinishCertRequest(
      data, key_type, cryptohome::Identification(account_id), key_name,
      base::BindRepeating(&AttestationFlow::OnCertRequestFinished,
                          weak_factory_.GetWeakPtr(), callback));
}

void AttestationFlow::OnCertRequestFinished(const CertificateCallback& callback,
                                            bool success,
                                            const std::string& data) {
  if (success)
    callback.Run(ATTESTATION_SUCCESS, data);
  else
    callback.Run(ATTESTATION_SERVER_BAD_REQUEST_FAILURE, data);
}

void AttestationFlow::GetExistingCertificate(
    AttestationKeyType key_type,
    const AccountId& account_id,
    const std::string& key_name,
    const CertificateCallback& callback) {
  cryptohome_client_->TpmAttestationGetCertificate(
      key_type, cryptohome::CreateAccountIdentifierFromAccountId(account_id),
      key_name, base::BindOnce(&DBusCertificateMethodCallback, callback));
}

void AttestationFlow::CheckAttestationReadyAndReschedule(
    base::TimeTicks end_time,
    const base::Closure& on_failure,
    const base::Closure& next_task) {
  if (base::TimeTicks::Now() < end_time) {
    LOG(WARNING) << "Attestation: Not prepared yet."
                 << " Retrying in " << retry_delay_ << ".";
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AttestationFlow::WaitForAttestationReadyAndStartEnroll,
                       weak_factory_.GetWeakPtr(), end_time, on_failure,
                       next_task),
        retry_delay_);
  } else {
    LOG(ERROR) << "Attestation: Not prepared. Giving up on retrying.";
    if (!on_failure.is_null())
      on_failure.Run();
  }
}

ServerProxy::~ServerProxy() = default;

PrivacyCAType ServerProxy::GetType() {
  return DEFAULT_PCA;
}

}  // namespace attestation
}  // namespace chromeos
