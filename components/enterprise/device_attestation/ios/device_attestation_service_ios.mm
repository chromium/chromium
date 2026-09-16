// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/ios/device_attestation_service_ios.h"

#include <cstdint>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/enterprise/device_attestation/common/device_attestation_types.h"
#include "components/enterprise/device_attestation/device_attestation_metrics.h"
#include "components/enterprise/device_attestation/ios/attestation_service_ios.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise {

namespace {

constexpr int kDefaultContentBindingVersion = 0;

std::string_view AttestationErrorToString(
    AttestationServiceIOS::AttestationError error) {
  switch (error) {
    case AttestationServiceIOS::AttestationError::kServiceUnavailable:
      return "Attestation service unavailable";
    case AttestationServiceIOS::AttestationError::kNetworkError:
      return "Attestation challenge fetch network error";
    case AttestationServiceIOS::AttestationError::kTimeout:
      return "Attestation challenge fetch timed out";
    case AttestationServiceIOS::AttestationError::kClientError:
      return "Attestation challenge fetch client error";
    case AttestationServiceIOS::AttestationError::kServerError:
      return "Attestation challenge fetch server error";
    case AttestationServiceIOS::AttestationError::kResponseParsingFailed:
      return "Attestation challenge response parsing failed";
    case AttestationServiceIOS::AttestationError::kNotInitialized:
      return "Attestation service not initialized";
    case AttestationServiceIOS::AttestationError::kSnapshotGenerationFailed:
      return "Attestation snapshot generation failed";
    case AttestationServiceIOS::AttestationError::kUnknown:
      return "Unknown attestation error";
  }
}

}  // namespace

DeviceAttestationServiceIOS::DeviceAttestationServiceIOS(
    std::unique_ptr<AttestationServiceIOS> attestation_service)
    : attestation_service_(std::move(attestation_service)) {}

DeviceAttestationServiceIOS::~DeviceAttestationServiceIOS() = default;

void DeviceAttestationServiceIOS::GetAttestationResponse(
    std::string_view /*flow_name*/,
    const enterprise_management::ChromeProfileReportRequest& report,
    std::string_view /*legacy_request_payload*/,
    std::string_view /*timestamp*/,
    std::string_view /*nonce*/,
    DeviceAttestationCallback callback) {
  // Captured before any work is done so that the content binding construction
  // below is part of the measured latency, matching Android.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  LogAttestationBlobRequested();

  if (!attestation_service_) {
    // No generation is attempted, so no latency is recorded; only the result.
    LogAttestationBlobServiceUnavailable();
    std::move(callback).Run(AttestationResult{
        .blob_generation_result = {.attestation_blob = "",
                                   .error_message =
                                       "Attestation service unavailable"},
        .content_binding_version = kDefaultContentBindingVersion});
    return;
  }

  AttestationServiceIOS::ContentBinding content_binding;
  if (report.has_browser_report()) {
    const enterprise_management::BrowserReport& browser_report =
        report.browser_report();
    if (!browser_report.browser_version().empty()) {
      content_binding["browser_version"] = browser_report.browser_version();
    }
    if (browser_report.chrome_user_profile_infos_size() == 1) {
      const enterprise_management::ChromeUserProfileInfo& profile_info =
          browser_report.chrome_user_profile_infos(0);
      if (!profile_info.profile_id().empty()) {
        content_binding["profile_id"] = profile_info.profile_id();
      }
    }
  }

  uint64_t request_id = next_request_id_++;
  // Store an entry before calling `GetSnapshot` so that if `GetSnapshot` runs
  // synchronously, `OnAttestationResponse` can erase it, preventing a leak
  // when `subscription` is returned.
  attestation_subscriptions_[request_id] = base::CallbackListSubscription();

  base::WeakPtr<DeviceAttestationServiceIOS> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  base::CallbackListSubscription subscription =
      attestation_service_->GetSnapshot(
          content_binding,
          base::BindOnce(&DeviceAttestationServiceIOS::OnAttestationResponse,
                         weak_this, request_id, start_time,
                         std::move(callback)));

  // If the callback ran synchronously during `GetSnapshot`, `this` may have
  // been destroyed.
  if (!weak_this) {
    return;
  }

  // If `GetSnapshot` ran synchronously, `OnAttestationResponse` has already
  // executed and erased `request_id` from `attestation_subscriptions_`.
  // If it ran asynchronously, store the subscription so it stays alive until
  // `OnAttestationResponse` runs.
  auto it = attestation_subscriptions_.find(request_id);
  if (it != attestation_subscriptions_.end()) {
    if (subscription) {
      it->second = std::move(subscription);
    } else {
      attestation_subscriptions_.erase(it);
    }
  }
}

void DeviceAttestationServiceIOS::OnAttestationResponse(
    uint64_t request_id,
    base::TimeTicks start_time,
    DeviceAttestationCallback callback,
    base::expected<std::string, AttestationServiceIOS::AttestationError>
        result) {
  attestation_subscriptions_.erase(request_id);

  BlobGenerationResult blob_generation_result;
  if (result.has_value()) {
    blob_generation_result.attestation_blob = std::move(result.value());
  } else {
    blob_generation_result.error_message =
        std::string(AttestationErrorToString(result.error()));
  }

  LogAttestationBlobGenerated(start_time, blob_generation_result);

  std::move(callback).Run(AttestationResult{
      .blob_generation_result = std::move(blob_generation_result),
      .content_binding_version = kDefaultContentBindingVersion});
}

}  // namespace enterprise
