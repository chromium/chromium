// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/enterprise/device_attestation/ios/device_attestation_service_ios.h"

#import <cstdint>
#import <string>
#import <string_view>
#import <utility>

#import "base/base64.h"
#import "base/functional/bind.h"
#import "base/json/json_writer.h"
#import "base/strings/strcat.h"
#import "base/time/time.h"
#import "base/types/expected.h"
#import "base/values.h"
#import "components/device_signals/core/common/signals_features.h"
#import "components/enterprise/device_attestation/common/device_attestation_types.h"
#import "components/enterprise/device_attestation/device_attestation_metrics.h"
#import "components/enterprise/device_attestation/ios/attestation_service_ios.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "crypto/hash.h"

namespace enterprise {

namespace {

std::string GetHashString(std::string_view payload) {
  return base::Base64Encode(crypto::hash::Sha256(payload));
}

AttestationServiceIOS::ContentBinding CreateAttestationHashes(
    std::string_view request_payload,
    std::string_view timestamp,
    std::string_view nonce) {
  return {
      {"request_hash", GetHashString(base::StrCat(
                           {request_payload, ".", timestamp, ".", nonce}))},
      {"timestamp_hash", GetHashString(timestamp)},
      {"nonce_hash", GetHashString(nonce)},
  };
}

base::ListValue RepeatedFieldptrToList(
    const google::protobuf::RepeatedPtrField<std::string>& field_values) {
  base::ListValue string_list;
  for (const std::string& field_value : field_values) {
    string_list.Append(field_value);
  }

  return string_list;
}

constexpr int kContentBindingVersionV1 = 1;
constexpr int kContentBindingVersionV0 = 0;

int GetCurrentContentBindingsVersion() {
  return enterprise_signals::features::IsContentBindingVersioningEnabledIOS()
             ? kContentBindingVersionV1
             : kContentBindingVersionV0;
}

void AddString(base::DictValue& signals_dict,
               std::string_view key,
               std::string_view value) {
  if (!value.empty()) {
    signals_dict.Set(key, value);
  }
}

void MaybeAddValue(base::DictValue& signals_dict,
                   std::string_view key,
                   bool has_value,
                   int value) {
  if (has_value) {
    signals_dict.Set(key, value);
  }
}

// DISCLAIMER: Do not update this content binding method without matching the
// changes on the server and incrementing the version number. Any changes should
// be done behind a flag and reflected on the server.
std::string GenerateV1ContentBindingString(
    const enterprise_management::ChromeProfileReportRequest& report) {
  base::DictValue signals_dict;
  std::string signals_json;

  MaybeAddValue(signals_dict, "version", true,
                GetCurrentContentBindingsVersion());

  if (report.has_os_report()) {
    const enterprise_management::OSReport& os_report = report.os_report();
    AddString(signals_dict, "device_manufacturer",
              os_report.device_manufacturer());
    AddString(signals_dict, "device_model", os_report.device_model());
    AddString(signals_dict, "operating_system", os_report.name());
    AddString(signals_dict, "os_version", os_report.version());

    if (os_report.has_ios_specific_attributes()) {
      AddString(signals_dict, "vendor_id",
                os_report.ios_specific_attributes().vendor_id());
    }
  }

  if (!report.has_browser_report()) {
    base::JSONWriter::WriteWithOptions(signals_dict, /*options=*/0,
                                       &signals_json);
    return signals_json;
  }

  const enterprise_management::BrowserReport& browser_report =
      report.browser_report();
  AddString(signals_dict, "browser_version", browser_report.browser_version());

  if (browser_report.chrome_user_profile_infos_size() == 1) {
    const enterprise_management::ChromeUserProfileInfo& profile_info =
        browser_report.chrome_user_profile_infos(0);
    AddString(signals_dict, "profile_id", profile_info.profile_id());

    if (profile_info.has_profile_signals_report()) {
      const enterprise_management::ProfileSignalsReport& profile_signals =
          profile_info.profile_signals_report();
      AddString(signals_dict, "profile_enrollment_domain",
                profile_signals.profile_enrollment_domain());
      MaybeAddValue(signals_dict, "realtime_url_check_mode",
                    profile_signals.has_realtime_url_check_mode(),
                    profile_signals.realtime_url_check_mode());
      MaybeAddValue(signals_dict, "safe_browsing_protection_level",
                    profile_signals.has_safe_browsing_protection_level(),
                    profile_signals.safe_browsing_protection_level());
      if (profile_signals.security_event_providers_size() > 0) {
        signals_dict.Set(
            "security_event_providers",
            RepeatedFieldptrToList(profile_signals.security_event_providers()));
      }
    }
  }

  base::JSONWriter::WriteWithOptions(signals_dict, /*options=*/0,
                                     &signals_json);

  return signals_json;
}

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
    std::string_view timestamp,
    std::string_view nonce,
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
        .content_binding_version = GetCurrentContentBindingsVersion()});
    return;
  }

  AttestationServiceIOS::ContentBinding content_binding;
  if (enterprise_signals::features::IsContentBindingVersioningEnabledIOS()) {
    content_binding = CreateAttestationHashes(
        GenerateV1ContentBindingString(report), timestamp, nonce);
  } else {
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
      .content_binding_version = GetCurrentContentBindingsVersion()});
}

}  // namespace enterprise
