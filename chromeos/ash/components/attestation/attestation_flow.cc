// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/attestation_flow.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/attestation/attestation_flow_utils.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/account_id/account_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace attestation {

namespace {

absl::optional<::attestation::CertificateProfile> ProfileToAttestationProtoEnum(
    AttestationCertificateProfile p) {
  switch (p) {
    case PROFILE_ENTERPRISE_MACHINE_CERTIFICATE:
      return ::attestation::CertificateProfile::ENTERPRISE_MACHINE_CERTIFICATE;
    case PROFILE_ENTERPRISE_USER_CERTIFICATE:
      return ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE;
    case PROFILE_CONTENT_PROTECTION_CERTIFICATE:
      return ::attestation::CertificateProfile::CONTENT_PROTECTION_CERTIFICATE;
    case PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE:
      return ::attestation::CertificateProfile::
          ENTERPRISE_ENROLLMENT_CERTIFICATE;
    case PROFILE_SOFT_BIND_CERTIFICATE:
      return ::attestation::CertificateProfile::SOFT_BIND_CERTIFICATE;
    case PROFILE_DEVICE_SETUP_CERTIFICATE:
      return ::attestation::CertificateProfile::DEVICE_SETUP_CERTIFICATE;
  }
  return {};
}

::attestation::ACAType ToAcaType(PrivacyCAType type) {
  switch (type) {
    case DEFAULT_PCA:
      return ::attestation::DEFAULT_ACA;
    case TEST_PCA:
      return ::attestation::TEST_ACA;
  }
  LOG(DFATAL) << "Unknown type to convert: " << type;
  return ::attestation::DEFAULT_ACA;
}

// A reasonable timeout that gives enough time for attestation to be ready,
// yet does not make the caller wait too long.
constexpr uint16_t kReadyTimeoutInSeconds = 60;

// Delay before checking again whether the TPM has been prepared for
// attestation.
constexpr uint16_t kRetryDelayInMilliseconds = 300;

}  // namespace

AttestationKeyType AttestationFlow::GetKeyTypeForProfile(
    AttestationCertificateProfile certificate_profile) {
  switch (certificate_profile) {
    case PROFILE_ENTERPRISE_MACHINE_CERTIFICATE:
    case PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE:
    case PROFILE_DEVICE_SETUP_CERTIFICATE:
      return KEY_DEVICE;
    case PROFILE_ENTERPRISE_USER_CERTIFICATE:
    case PROFILE_CONTENT_PROTECTION_CERTIFICATE:
    case PROFILE_SOFT_BIND_CERTIFICATE:
      return KEY_USER;
  }
  NOTREACHED();
  return KEY_USER;
}

AttestationFlow::AttestationFlow(std::unique_ptr<ServerProxy> server_proxy,
                                 ::attestation::KeyType crypto_key_type)
    : attestation_client_(AttestationClient::Get()),
      server_proxy_(std::move(server_proxy)),
      crypto_key_type_(crypto_key_type),
      ready_timeout_(base::Seconds(kReadyTimeoutInSeconds)),
      retry_delay_(base::Milliseconds(kRetryDelayInMilliseconds)) {}

AttestationFlow::AttestationFlow(std::unique_ptr<ServerProxy> server_proxy)
    : AttestationFlow(std::move(server_proxy), ::attestation::KEY_TYPE_RSA) {}

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
  attestation_client_->GetStatus(
      ::attestation::GetStatusRequest(),
      base::BindOnce(&AttestationFlow::OnEnrollmentCheckComplete,
                     weak_factory_.GetWeakPtr(),
                     std::move(start_certificate_request)));
}

void AttestationFlow::OnEnrollmentCheckComplete(
    base::OnceCallback<void(bool)> callback,
    const ::attestation::GetStatusReply& reply) {
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    LOG(ERROR) << "Attestation: Failed to check enrollment state. Status: "
               << reply.status();
    std::move(callback).Run(false);
    return;
  }

  if (reply.enrolled()) {
    std::move(callback).Run(true);
    return;
  }

  // The device is not enrolled; check if it's enrollment prepared.
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
    ::attestation::CreateEnrollRequestRequest request;
    request.set_aca_type(ToAcaType(server_proxy_->GetType()));
    AttestationClient::Get()->CreateEnrollRequest(
        request,
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
    const ::attestation::CreateEnrollRequestReply& reply) {
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    LOG(ERROR) << "Attestation: Failed to create enroll request; status: "
               << reply.status();
    std::move(callback).Run(false);
    return;
  }

  // Send the request to the Privacy CA.
  server_proxy_->SendEnrollRequest(
      reply.pca_request(),
      base::BindOnce(&AttestationFlow::SendEnrollResponseToDaemon,
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
  ::attestation::FinishEnrollRequest request;
  request.set_pca_response(data);
  request.set_aca_type(ToAcaType(server_proxy_->GetType()));
  AttestationClient::Get()->FinishEnroll(
      request, base::BindOnce(&AttestationFlow::OnEnrollComplete,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttestationFlow::OnEnrollComplete(
    base::OnceCallback<void(bool)> callback,
    const ::attestation::FinishEnrollReply& reply) {
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    LOG(ERROR) << "Attestation: Failed to complete enrollment; status: "
               << reply.status();
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
    const absl::optional<::attestation::CertificateProfile>
        attestation_profile =
            ProfileToAttestationProtoEnum(certificate_profile);
    if (!attestation_profile) {
      LOG(DFATAL) << "Attestation: Unrecognized profile type: "
                  << certificate_profile;
      return;
    }

    ::attestation::CreateCertificateRequestRequest request;
    if (key_type == KEY_USER) {
      request.set_username(cryptohome::Identification(account_id).id());
    }
    request.set_certificate_profile(*attestation_profile);
    request.set_request_origin(request_origin);
    request.set_key_type(crypto_key_type_);
    request.set_aca_type(ToAcaType(server_proxy_->GetType()));

    attestation_client_->CreateCertificateRequest(
        request, base::BindOnce(&AttestationFlow::SendCertificateRequestToPCA,
                                weak_factory_.GetWeakPtr(), key_type,
                                account_id, key_name, std::move(callback)));
    return;
  }

  ::attestation::GetKeyInfoRequest request;
  if (key_type == KEY_USER) {
    request.set_username(cryptohome::Identification(account_id).id());
  }
  request.set_key_label(key_name);
  attestation_client_->GetKeyInfo(
      request, base::BindOnce(&AttestationFlow::OnGetKeyInfoComplete,
                              weak_factory_.GetWeakPtr(), certificate_profile,
                              account_id, request_origin, key_name, key_type,
                              std::move(callback)));
}

void AttestationFlow::OnGetKeyInfoComplete(
    AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    const std::string& key_name,
    AttestationKeyType key_type,
    CertificateCallback callback,
    const ::attestation::GetKeyInfoReply& reply) {
  // If the key already exists, return the existing certificate.
  if (reply.status() == ::attestation::STATUS_SUCCESS) {
    std::move(callback).Run(ATTESTATION_SUCCESS, reply.certificate());
    return;
  }

  // If the key does not exist, call this method back with |generate_new_key|
  // set to true.
  if (reply.status() == ::attestation::STATUS_INVALID_PARAMETER) {
    StartCertificateRequest(certificate_profile, account_id, request_origin,
                            /*generate_new_key=*/true, key_name,
                            std::move(callback), /*enrolled=*/true);
    return;
  }

  // Otherwise the key info query fails.
  LOG(ERROR) << "Attestation: Failed to check for existence of key; status: "
             << reply.status() << ".";
  std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
}

void AttestationFlow::SendCertificateRequestToPCA(
    AttestationKeyType key_type,
    const AccountId& account_id,
    const std::string& key_name,
    CertificateCallback callback,
    const ::attestation::CreateCertificateRequestReply& reply) {
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    LOG(ERROR) << "Attestation: Failed to create certificate request. Status: "
               << reply.status();
    std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
    return;
  }

  // Send the request to the Privacy CA.
  server_proxy_->SendCertificateRequest(
      reply.pca_request(),
      base::BindOnce(&AttestationFlow::SendCertificateResponseToDaemon,
                     weak_factory_.GetWeakPtr(), key_type, account_id, key_name,
                     std::move(callback)));
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

  ::attestation::FinishCertificateRequestRequest request;
  if (key_type == KEY_USER) {
    request.set_username(cryptohome::Identification(account_id).id());
  }
  request.set_key_label(key_name);
  request.set_pca_response(data);
  AttestationClient::Get()->FinishCertificateRequest(
      request, base::BindOnce(&AttestationFlow::OnCertRequestFinished,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttestationFlow::OnCertRequestFinished(
    CertificateCallback callback,
    const ::attestation::FinishCertificateRequestReply& reply) {
  if (reply.status() == ::attestation::STATUS_SUCCESS) {
    std::move(callback).Run(ATTESTATION_SUCCESS, reply.certificate());
  } else {
    LOG(ERROR) << "Failed to finish certificate request; status: "
               << reply.status();
    std::move(callback).Run(ATTESTATION_SERVER_BAD_REQUEST_FAILURE,
                            /*pem_certificate_chain=*/"");
  }
}

ServerProxy::~ServerProxy() = default;

PrivacyCAType ServerProxy::GetType() {
  return DEFAULT_PCA;
}

}  // namespace attestation
}  // namespace ash
