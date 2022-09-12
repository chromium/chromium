// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/attestation_flow_integrated.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/attestation/attestation_flow_utils.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/account_id/account_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
// TOOD(b/158955123): Remove this transitional state along with the removal of
// |AttestationFlow|.
AttestationFlowIntegrated::AttestationFlowIntegrated(
    ::attestation::ACAType aca_type)
    : AttestationFlow(/*server_proxy=*/nullptr),
      aca_type_(aca_type),
      attestation_client_(AttestationClient::Get()),
      ready_timeout_(kReadyTimeout),
      retry_delay_(kRetryDelay) {}

AttestationFlowIntegrated::~AttestationFlowIntegrated() = default;

void AttestationFlowIntegrated::GetCertificate(
    AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    bool force_new_key,
    const std::string& key_name,
    CertificateCallback callback) {
  const std::string attestation_key_name =
      !key_name.empty()
          ? key_name
          : GetKeyNameForProfile(certificate_profile, request_origin);

  base::OnceCallback<void(bool)> start_certificate_request = base::BindOnce(
      &AttestationFlowIntegrated::StartCertificateRequest,
      weak_factory_.GetWeakPtr(), certificate_profile, account_id,
      request_origin, force_new_key, attestation_key_name, std::move(callback));

  base::TimeTicks end_time = base::TimeTicks::Now() + ready_timeout_;
  WaitForAttestationPrepared(end_time, std::move(start_certificate_request));
}

void AttestationFlowIntegrated::WaitForAttestationPrepared(
    base::TimeTicks end_time,
    base::OnceCallback<void(bool)> callback) {
  ::attestation::GetEnrollmentPreparationsRequest request;
  request.set_aca_type(aca_type_);
  attestation_client_->GetEnrollmentPreparations(
      request, base::BindOnce(
                   &AttestationFlowIntegrated::OnPreparedCheckComplete,
                   weak_factory_.GetWeakPtr(), end_time, std::move(callback)));
}

void AttestationFlowIntegrated::OnPreparedCheckComplete(
    base::TimeTicks end_time,
    base::OnceCallback<void(bool)> callback,
    const ::attestation::GetEnrollmentPreparationsReply& reply) {
  if (reply.status() == ::attestation::STATUS_SUCCESS &&
      IsPreparedWith(reply, aca_type_)) {
    std::move(callback).Run(true);
    return;
  }

  if (base::TimeTicks::Now() < end_time) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AttestationFlowIntegrated::WaitForAttestationPrepared,
                       weak_factory_.GetWeakPtr(), end_time,
                       std::move(callback)),
        retry_delay_);
    return;
  }
  std::move(callback).Run(false);
}

void AttestationFlowIntegrated::StartCertificateRequest(
    AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    bool generate_new_key,
    const std::string& key_name,
    CertificateCallback callback,
    bool is_prepared) {
  if (!is_prepared) {
    LOG(ERROR) << __func__ << ": Not prepared.";
    std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
    return;
  }

  ::attestation::GetCertificateRequest request;
  request.set_aca_type(aca_type_);
  absl::optional<::attestation::CertificateProfile> profile_attestation_enum =
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
  request.set_key_label(key_name);
  request.set_shall_trigger_enrollment(true);
  request.set_forced(generate_new_key);

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
