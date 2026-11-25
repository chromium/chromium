// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/ash/device_attestation_service_ash.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/enterprise/device_attestation/common/device_attestation_types.h"
#include "components/enterprise/device_attestation/device_attestation_metrics.h"
#include "components/enterprise/device_attestation/device_attestation_utils.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/hash.h"

namespace enterprise {

namespace {

bool IsDeviceEnterpriseManaged() {
  return ash::InstallAttributes::IsInitialized() &&
         ash::InstallAttributes::Get()->IsEnterpriseManaged();
}

bool IsProfileAffiliated(
    const enterprise_management::ChromeProfileReportRequest& report) {
  if (!report.has_browser_report() ||
      report.browser_report().chrome_user_profile_infos_size() != 1) {
    return false;
  }
  const auto& profile_info =
      report.browser_report().chrome_user_profile_infos(0);
  return profile_info.has_affiliation() &&
         profile_info.affiliation().is_affiliated();
}

// ChromeOS only supports the V1 content binding. This is deliberately
// independent of `kContentBindingVersioningEnabled`, which gates the V0 to V1
// migration on other platforms.
constexpr int kContentBindingVersion = 1;

}  // namespace

DeviceAttestationServiceAsh::DeviceAttestationServiceAsh() = default;

DeviceAttestationServiceAsh::~DeviceAttestationServiceAsh() = default;

void DeviceAttestationServiceAsh::GetAttestationResponse(
    std::string_view flow_name,
    const enterprise_management::ChromeProfileReportRequest& report,
    std::string_view legacy_request_payload,
    std::string_view timestamp,
    std::string_view nonce,
    DeviceAttestationCallback callback) {
  // Profiles that are not eligible for hardware attestation (consumer-owned
  // devices and unaffiliated profiles) are expected to report software signals
  // only. As with the no-op DeviceAttestationService used on platforms without
  // attestation, this is not an attestation request, so it is deliberately
  // returned before LogAttestationBlobRequested() and records no metrics.
  // This keeps kServiceUnavailable for genuine failures below.
  if (!IsDeviceEnterpriseManaged() || !IsProfileAffiliated(report)) {
    AttestationResult final_result;
    final_result.blob_generation_result.error_message =
        "Device Attestation is unsupported";
    final_result.content_binding_version = kContentBindingVersion;
    std::move(callback).Run(std::move(final_result));
    return;
  }

  const base::TimeTicks start_time = base::TimeTicks::Now();
  LogAttestationBlobRequested();

  auto* attestation_client = ash::AttestationClient::Get();
  if (!attestation_client) {
    LogAttestationBlobServiceUnavailable();
    AttestationResult final_result;
    final_result.blob_generation_result.error_message =
        "Attestation client is unavailable";
    final_result.content_binding_version = kContentBindingVersion;
    std::move(callback).Run(std::move(final_result));
    return;
  }

  // `legacy_request_payload` (the V0 content binding) is intentionally unused,
  // see `kContentBindingVersion`.
  const std::string request_payload = GenerateV1ContentBindingString(report);

  // Hash the serialized request payload with SHA-256 and Base64-encode the
  // digest so the challenge passed to SignSimpleChallenge is compact and
  // delimiter-safe.
  std::string payload_hash =
      base::Base64Encode(crypto::hash::Sha256(request_payload));
  std::string challenge =
      base::StrCat({flow_name, ":", nonce, ":", timestamp, ":", payload_hash});

  ::attestation::SignSimpleChallengeRequest request;
  request.set_key_label(ash::attestation::kEnterpriseMachineKey);
  request.set_challenge(challenge);
  // request.set_username(username) is left empty since this is for the whole
  // device key.

  attestation_client->SignSimpleChallenge(
      request,
      base::BindOnce(&DeviceAttestationServiceAsh::OnChallengeResponseReady,
                     weak_ptr_factory_.GetWeakPtr(), start_time,
                     std::move(callback)));
}

void DeviceAttestationServiceAsh::OnChallengeResponseReady(
    base::TimeTicks start_time,
    DeviceAttestationCallback callback,
    const ::attestation::SignSimpleChallengeReply& reply) {
  AttestationResult final_result;

  // Note: `reply.has_status()` must not be checked here. The attestation
  // service leaves `status` unset on success, relying on the proto default of
  // STATUS_SUCCESS.
  if (reply.status() == ::attestation::STATUS_SUCCESS &&
      !reply.challenge_response().empty()) {
    final_result.blob_generation_result.attestation_blob =
        base::Base64Encode(reply.challenge_response());
  } else if (reply.status() == ::attestation::STATUS_SUCCESS) {
    final_result.blob_generation_result.error_message =
        "Empty challenge response from attestation service.";
  } else {
    final_result.blob_generation_result.error_message =
        "Failed to sign simple challenge. Status: " +
        base::NumberToString(static_cast<int>(reply.status()));
  }

  LogAttestationBlobGenerated(start_time, final_result.blob_generation_result);

  final_result.content_binding_version = kContentBindingVersion;

  std::move(callback).Run(std::move(final_result));
}

}  // namespace enterprise
