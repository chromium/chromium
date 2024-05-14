// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/attestation_flow.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/account_id/account_id.h"

namespace ash {
namespace attestation {

namespace {

std::optional<::attestation::CertificateProfile> ProfileToAttestationProtoEnum(
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
    case PROFILE_DEVICE_TRUST_USER_CERTIFICATE:
      return ::attestation::CertificateProfile::DEVICE_TRUST_USER_CERTIFICATE;
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
    case PROFILE_DEVICE_TRUST_USER_CERTIFICATE:
      return KEY_DEVICE;
    case PROFILE_ENTERPRISE_USER_CERTIFICATE:
    case PROFILE_CONTENT_PROTECTION_CERTIFICATE:
    case PROFILE_SOFT_BIND_CERTIFICATE:
      return KEY_USER;
  }
  NOTREACHED_IN_MIGRATION();
  return KEY_USER;
}

AttestationFlowLegacy::AttestationFlowLegacy(
    std::unique_ptr<ServerProxy> server_proxy)
    : attestation_client_(AttestationClient::Get()),
      server_proxy_(std::move(server_proxy)),
      ready_timeout_(base::Seconds(kReadyTimeoutInSeconds)),
      retry_delay_(base::Milliseconds(kRetryDelayInMilliseconds)) {}

AttestationFlowLegacy::~AttestationFlowLegacy() = default;

void AttestationFlowLegacy::GetCertificate(
    AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    bool force_new_key,
    ::attestation::KeyType key_crypto_type,
    const std::string& key_name,
    const std::optional<CertProfileSpecificData>& profile_specific_data,
    CertificateCallback callback) {
  DCHECK(!key_name.empty());

  EnrollCallback start_certificate_request =
      base::BindOnce(&AttestationFlowLegacy::StartCertificateRequest,
                     weak_factory_.GetWeakPtr(), certificate_profile,
                     account_id, request_origin, force_new_key, key_crypto_type,
                     key_name, profile_specific_data, std::move(callback));

  // If this device has not enrolled with the Privacy CA, we need to do that
  // first.  Once enrolled we can proceed with the certificate request.
  ::attestation::GetStatusRequest status_request;
  status_request.set_extended_status(true);
  attestation_client_->GetStatus(
      status_request,
      base::BindOnce(&AttestationFlowLegacy::OnEnrollmentCheckComplete,
                     weak_factory_.GetWeakPtr(), certificate_profile,
                     std::move(start_certificate_request)));
}

void AttestationFlowLegacy::OnEnrollmentCheckComplete(
    AttestationCertificateProfile certificate_profile,
    EnrollCallback callback,
    const ::attestation::GetStatusReply& reply) {
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    LOG(ERROR) << "Attestation: Failed to check enrollment state. Status: "
               << reply.status();
    std::move(callback).Run(EnrollState::kError);
    return;
  }

  if (reply.enrolled()) {
    std::move(callback).Run(EnrollState::kEnrolled);
    return;
  }

  // The verified boot state is required for soft bind certificates.
  if (certificate_profile ==
          AttestationCertificateProfile::PROFILE_SOFT_BIND_CERTIFICATE &&
      !reply.verified_boot()) {
    LOG(ERROR) << "Attestation: Cannot create soft bind certificate without "
                  "verified boot.";
    std::move(callback).Run(EnrollState::kError);
    return;
  }

  // The device is not enrolled; check if it supports attestation.
  GetFeatures(std::move(callback));
}

void AttestationFlowLegacy::GetFeatures(EnrollCallback callback) {
  attestation_client_->GetFeatures(
      ::attestation::GetFeaturesRequest(),
      base::BindOnce(&AttestationFlowLegacy::OnGetFeaturesComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttestationFlowLegacy::OnGetFeaturesComplete(
    EnrollCallback callback,
    const ::attestation::GetFeaturesReply& reply) {
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    LOG(ERROR) << "Attestation: Failed to get features; status: "
               << reply.status();
    std::move(callback).Run(EnrollState::kError);
    return;
  }

  if (reply.is_available()) {
    // Check if the device is enrollment prepared.
    base::TimeTicks end_time = base::TimeTicks::Now() + ready_timeout_;
    WaitForAttestationPrepared(end_time, std::move(callback));
  } else {
    std::move(callback).Run(EnrollState::kNotAvailable);
  }
}

void AttestationFlowLegacy::WaitForAttestationPrepared(
    base::TimeTicks end_time,
    EnrollCallback callback) {
  ::attestation::GetEnrollmentPreparationsRequest request;
  attestation_client_->GetEnrollmentPreparations(
      request, base::BindOnce(&AttestationFlowLegacy::OnPreparedCheckComplete,
                              weak_factory_.GetWeakPtr(), end_time,
                              std::move(callback)));
}

void AttestationFlowLegacy::OnPreparedCheckComplete(
    base::TimeTicks end_time,
    EnrollCallback callback,
    const ::attestation::GetEnrollmentPreparationsReply& reply) {
  if (AttestationClient::IsAttestationPrepared(reply)) {
    // Get the attestation service to create a Privacy CA enrollment request.
    ::attestation::CreateEnrollRequestRequest request;
    request.set_aca_type(ToAcaType(server_proxy_->GetType()));
    AttestationClient::Get()->CreateEnrollRequest(
        request,
        base::BindOnce(&AttestationFlowLegacy::SendEnrollRequestToPCA,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  if (base::TimeTicks::Now() < end_time) {
    LOG(WARNING) << "Attestation: Not prepared yet."
                 << " Retrying in " << retry_delay_ << ".";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AttestationFlowLegacy::WaitForAttestationPrepared,
                       weak_factory_.GetWeakPtr(), end_time,
                       std::move(callback)),
        retry_delay_);
    return;
  }

  LOG(ERROR) << "Attestation: Not prepared. Giving up on retrying.";
  std::move(callback).Run(EnrollState::kError);
}

void AttestationFlowLegacy::SendEnrollRequestToPCA(
    EnrollCallback callback,
    const ::attestation::CreateEnrollRequestReply& reply) {
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    LOG(ERROR) << "Attestation: Failed to create enroll request; status: "
               << reply.status();
    std::move(callback).Run(EnrollState::kError);
    return;
  }

  // Send the request to the Privacy CA.
  server_proxy_->SendEnrollRequest(
      reply.pca_request(),
      base::BindOnce(&AttestationFlowLegacy::SendEnrollResponseToDaemon,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttestationFlowLegacy::SendEnrollResponseToDaemon(
    EnrollCallback callback,
    bool success,
    const std::string& data) {
  if (!success) {
    LOG(ERROR) << "Attestation: Enroll request failed.";
    std::move(callback).Run(EnrollState::kError);
    return;
  }

  // Forward the response to the attestation service to complete enrollment.
  ::attestation::FinishEnrollRequest request;
  request.set_pca_response(data);
  request.set_aca_type(ToAcaType(server_proxy_->GetType()));
  AttestationClient::Get()->FinishEnroll(
      request, base::BindOnce(&AttestationFlowLegacy::OnEnrollComplete,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttestationFlowLegacy::OnEnrollComplete(
    EnrollCallback callback,
    const ::attestation::FinishEnrollReply& reply) {
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    LOG(ERROR) << "Attestation: Failed to complete enrollment; status: "
               << reply.status();
    std::move(callback).Run(EnrollState::kError);
    return;
  }

  std::move(callback).Run(EnrollState::kEnrolled);
}

void AttestationFlowLegacy::StartCertificateRequest(
    AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    bool generate_new_key,
    ::attestation::KeyType key_crypto_type,
    const std::string& key_name,
    const std::optional<CertProfileSpecificData>& profile_specific_data,
    CertificateCallback callback,
    EnrollState enroll_state) {
  switch (enroll_state) {
    case EnrollState::kError:
      std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
      return;

    case EnrollState::kNotAvailable:
      std::move(callback).Run(ATTESTATION_NOT_AVAILABLE, "");
      return;

    case EnrollState::kEnrolled:
      break;
  }

  AttestationKeyType key_type = GetKeyTypeForProfile(certificate_profile);
  if (generate_new_key) {
    // Get the attestation service to create a Privacy CA certificate request.
    const std::optional<::attestation::CertificateProfile> attestation_profile =
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
    request.set_key_type(key_crypto_type);
    request.set_aca_type(ToAcaType(server_proxy_->GetType()));

    if (attestation_profile ==
        ::attestation::CertificateProfile::DEVICE_SETUP_CERTIFICATE) {
      DCHECK(profile_specific_data.has_value())
          << "profile_specific_data must be provided for "
             "DEVICE_SETUP_CERTIFICATE";
      DCHECK(absl::holds_alternative<
             ::attestation::DeviceSetupCertificateRequestMetadata>(
          profile_specific_data.value()))
          << "profile_specific_data must be of type "
             "::attestation::DeviceSetupCertificateRequestMetadata";

      request.mutable_device_setup_certificate_request_metadata()->set_id(
          absl::get<::attestation::DeviceSetupCertificateRequestMetadata>(
              profile_specific_data.value())
              .id());
      request.mutable_device_setup_certificate_request_metadata()
          ->set_content_binding(
              absl::get<::attestation::DeviceSetupCertificateRequestMetadata>(
                  profile_specific_data.value())
                  .content_binding());
    }

    attestation_client_->CreateCertificateRequest(
        request,
        base::BindOnce(&AttestationFlowLegacy::SendCertificateRequestToPCA,
                       weak_factory_.GetWeakPtr(), key_type, account_id,
                       key_name, std::move(callback)));
    return;
  }

  ::attestation::GetKeyInfoRequest request;
  if (key_type == KEY_USER) {
    request.set_username(cryptohome::Identification(account_id).id());
  }
  request.set_key_label(key_name);
  attestation_client_->GetKeyInfo(
      request,
      base::BindOnce(&AttestationFlowLegacy::OnGetKeyInfoComplete,
                     weak_factory_.GetWeakPtr(), certificate_profile,
                     account_id, request_origin, key_crypto_type, key_name,
                     key_type, profile_specific_data, std::move(callback)));
}

void AttestationFlowLegacy::OnGetKeyInfoComplete(
    AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    ::attestation::KeyType key_crypto_type,
    const std::string& key_name,
    AttestationKeyType key_type,
    const std::optional<CertProfileSpecificData>& profile_specific_data,
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
                            /*generate_new_key=*/true, key_crypto_type,
                            key_name, profile_specific_data,
                            std::move(callback), EnrollState::kEnrolled);
    return;
  }

  // Otherwise the key info query fails.
  LOG(ERROR) << "Attestation: Failed to check for existence of key; status: "
             << reply.status() << ".";
  std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
}

void AttestationFlowLegacy::SendCertificateRequestToPCA(
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
      base::BindOnce(&AttestationFlowLegacy::SendCertificateResponseToDaemon,
                     weak_factory_.GetWeakPtr(), key_type, account_id, key_name,
                     std::move(callback)));
}

void AttestationFlowLegacy::SendCertificateResponseToDaemon(
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
      request, base::BindOnce(&AttestationFlowLegacy::OnCertRequestFinished,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttestationFlowLegacy::OnCertRequestFinished(
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
