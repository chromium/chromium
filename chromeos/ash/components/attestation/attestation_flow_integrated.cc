// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/attestation_flow_integrated.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/account_id/account_id.h"

namespace ash {
namespace attestation {

namespace {

// A reasonable timeout that gives enough time for attestation to be ready,
// yet does not make the caller wait too long.
constexpr base::TimeDelta kReadyTimeout = base::Seconds(60);

// Delay before checking again whether the TPM has been prepared for
// attestation.
constexpr base::TimeDelta kRetryDelay = base::Milliseconds(300);

// Values for the attestation server switch.
constexpr char kAttestationServerDefault[] = "default";
constexpr char kAttestationServerTest[] = "test";

constexpr char kGetCertificateStatusName[] =
    "ChromeOS.Attestation.GetCertificateStatus";
constexpr int kGetCertificateStatusMaxValue =
    ::attestation::AttestationStatus_MAX + 1;

::attestation::ACAType GetConfiguredACAType() {
  std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          chromeos::switches::kAttestationServer);
  if (value.empty() || value == kAttestationServerDefault) {
    return ::attestation::ACAType::DEFAULT_ACA;
  }
  if (value == kAttestationServerTest) {
    return ::attestation::ACAType::TEST_ACA;
  }
  LOG(WARNING) << "Invalid attestation server value: " << value
               << "; using default.";
  return ::attestation::ACAType::DEFAULT_ACA;
}

bool IsPreparedWith(const ::attestation::GetEnrollmentPreparationsReply& reply,
                    ::attestation::ACAType aca_type) {
  for (const auto& preparation : reply.enrollment_preparations()) {
    if (preparation.first == aca_type) {
      return preparation.second;
    }
  }
  return false;
}

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
    default:
      return {};
  }
}

}  // namespace

AttestationFlowIntegrated::AttestationFlowIntegrated()
    : AttestationFlowIntegrated(GetConfiguredACAType()) {}

// This constructor passes |nullptr|s to the base class
// |AttestationFlow| because we don't use cryptohome client and server
// proxy in |AttestationFlowIntegrated|.
//
// TOOD(b/232893759): Remove this transitional state along with the removal of
// |AttestationFlow|.
AttestationFlowIntegrated::AttestationFlowIntegrated(
    ::attestation::ACAType aca_type)
    : aca_type_(aca_type),
      attestation_client_(AttestationClient::Get()),
      ready_timeout_(kReadyTimeout),
      retry_delay_(kRetryDelay) {}

AttestationFlowIntegrated::~AttestationFlowIntegrated() = default;

void AttestationFlowIntegrated::GetCertificate(
    AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    bool force_new_key,
    ::attestation::KeyType key_crypto_type,
    const std::string& key_name,
    const std::optional<AttestationFlow::CertProfileSpecificData>&
        profile_specific_data,
    CertificateCallback callback) {
  EnrollCallback start_certificate_request =
      base::BindOnce(&AttestationFlowIntegrated::StartCertificateRequest,
                     weak_factory_.GetWeakPtr(), certificate_profile,
                     account_id, request_origin, force_new_key, key_crypto_type,
                     key_name, profile_specific_data, std::move(callback));

  GetFeatures(std::move(start_certificate_request));
}

void AttestationFlowIntegrated::GetFeatures(EnrollCallback callback) {
  attestation_client_->GetFeatures(
      ::attestation::GetFeaturesRequest(),
      base::BindOnce(&AttestationFlowIntegrated::OnGetFeaturesComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttestationFlowIntegrated::OnGetFeaturesComplete(
    EnrollCallback callback,
    const ::attestation::GetFeaturesReply& reply) {
  if (reply.is_available()) {
    base::TimeTicks end_time = base::TimeTicks::Now() + ready_timeout_;
    WaitForAttestationPrepared(end_time, std::move(callback));
  } else {
    std::move(callback).Run(EnrollState::kNotAvailable);
  }
}

void AttestationFlowIntegrated::WaitForAttestationPrepared(
    base::TimeTicks end_time,
    EnrollCallback callback) {
  ::attestation::GetEnrollmentPreparationsRequest request;
  request.set_aca_type(aca_type_);
  attestation_client_->GetEnrollmentPreparations(
      request, base::BindOnce(
                   &AttestationFlowIntegrated::OnPreparedCheckComplete,
                   weak_factory_.GetWeakPtr(), end_time, std::move(callback)));
}

void AttestationFlowIntegrated::OnPreparedCheckComplete(
    base::TimeTicks end_time,
    EnrollCallback callback,
    const ::attestation::GetEnrollmentPreparationsReply& reply) {
  if (reply.status() == ::attestation::STATUS_SUCCESS &&
      IsPreparedWith(reply, aca_type_)) {
    std::move(callback).Run(EnrollState::kEnrolled);
    return;
  }

  if (base::TimeTicks::Now() < end_time) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AttestationFlowIntegrated::WaitForAttestationPrepared,
                       weak_factory_.GetWeakPtr(), end_time,
                       std::move(callback)),
        retry_delay_);
    return;
  }
  std::move(callback).Run(EnrollState::kError);
}

void AttestationFlowIntegrated::StartCertificateRequest(
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
      LOG(ERROR) << __func__ << ": Not prepared.";
      std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
      return;

    case EnrollState::kNotAvailable:
      std::move(callback).Run(ATTESTATION_NOT_AVAILABLE, "");
      return;

    case EnrollState::kEnrolled:
      break;
  }

  ::attestation::GetCertificateRequest request;
  request.set_aca_type(aca_type_);
  std::optional<::attestation::CertificateProfile> profile_attestation_enum =
      ProfileToAttestationProtoEnum(certificate_profile);
  if (!profile_attestation_enum) {
    LOG(ERROR) << __func__ << ": Unexpected profile value: "
               << static_cast<int>(certificate_profile);
    std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
    return;
  }

  request.set_certificate_profile(*profile_attestation_enum);
  request.set_request_origin(request_origin);
  if (GetKeyTypeForProfile(certificate_profile) == KEY_USER) {
    request.set_username(cryptohome::Identification(account_id).id());
  }
  request.set_key_type(key_crypto_type);
  request.set_key_label(key_name);
  request.set_shall_trigger_enrollment(true);
  request.set_forced(generate_new_key);

  if (profile_attestation_enum ==
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

  attestation_client_->GetCertificate(
      request, base::BindOnce(&AttestationFlowIntegrated::OnCertRequestFinished,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttestationFlowIntegrated::OnCertRequestFinished(
    CertificateCallback callback,
    const ::attestation::GetCertificateReply& reply) {
  base::UmaHistogramExactLinear(kGetCertificateStatusName, reply.status(),
                                kGetCertificateStatusMaxValue);
  if (reply.status() == ::attestation::STATUS_SUCCESS) {
    std::move(callback).Run(ATTESTATION_SUCCESS, reply.certificate());
  } else {
    std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
  }
}

}  // namespace attestation
}  // namespace ash
