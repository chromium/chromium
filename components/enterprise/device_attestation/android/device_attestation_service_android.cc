// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/android/device_attestation_service_android.h"

#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/device_signals/core/common/signals_features.h"
#include "components/enterprise/device_attestation/android/android_attestation_client.h"
#include "components/enterprise/device_attestation/android/attestation_utils.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace {

base::ListValue RepeatedFieldptrToList(
    const google::protobuf::RepeatedPtrField<std::string>& field_values) {
  base::ListValue string_list;
  for (const auto& field_value : field_values) {
    string_list.Append(field_value);
  }

  return string_list;
}

int GetCurrentContentBindingsVersion() {
  if (enterprise_signals::features::IsContentBindingVersioningEnabled()) {
    return 1;
  } else {
    return 0;
  }
}

void AddString(base::DictValue& signals_dict,
               std::string_view key,
               const std::string& value) {
  if (!value.empty()) {
    signals_dict.Set(key, value);
  }
}

template <typename T>
void MaybeAddValue(base::DictValue& signals_dict,
                   std::string_view key,
                   bool has_value,
                   T value) {
  if (has_value) {
    // Cast enums to int so base::DictValue accepts them
    if constexpr (std::is_enum_v<T>) {
      signals_dict.Set(key, static_cast<int>(value));
    } else {
      signals_dict.Set(key, value);
    }
  }
}

}  // namespace

namespace enterprise {

namespace em = enterprise_management;

DeviceAttestationServiceAndroid::DeviceAttestationServiceAndroid(
    std::unique_ptr<AndroidAttestationClient> client)
    : client_(std::move(client)) {
  CHECK(client_);
}

DeviceAttestationServiceAndroid::~DeviceAttestationServiceAndroid() = default;

void DeviceAttestationServiceAndroid::GetAttestationResponse(
    std::string_view flow_name,
    const enterprise_management::ChromeProfileReportRequest& report,
    std::string_view legacy_request_payload,
    std::string_view timestamp,
    std::string_view nonce,
    DeviceAttestationCallback callback) {
  std::string request_payload;
  if (enterprise_signals::features::IsContentBindingVersioningEnabled()) {
    request_payload = GenerateV1ContentBindingString(report);
  } else {
    request_payload = std::string(legacy_request_payload);
  }

  AttestationHashes hashes =
      CreateAttestationHashes(request_payload, timestamp, nonce);

  client_->GenerateAttestationBlob(
      flow_name, hashes.request_hash, hashes.timestamp_hash, hashes.nonce_hash,
      base::BindOnce(&DeviceAttestationServiceAndroid::OnAttestationResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DeviceAttestationServiceAndroid::OnAttestationResponse(
    DeviceAttestationCallback callback,
    BlobGenerationResult blob_generation_result) {
  std::move(callback).Run(AttestationResult{
      .blob_generation_result = std::move(blob_generation_result),
      .content_binding_version = GetCurrentContentBindingsVersion()});
}

// DISCLAIMER: Do not update this content binding method without matching the
// changes on the server and incrementing the version number. Any changes should
// be done behind a flag and reflected on the server.
std::string DeviceAttestationServiceAndroid::GenerateV1ContentBindingString(
    const enterprise_management::ChromeProfileReportRequest& report) {
  base::DictValue signals_dict;
  std::string signals_json;

  MaybeAddValue(signals_dict, "version", true,
                GetCurrentContentBindingsVersion());
  if (report.has_browser_device_identifier()) {
    const auto& device_identifier = report.browser_device_identifier();
    AddString(signals_dict, "display_name", device_identifier.computer_name());
  }

  if (report.has_os_report()) {
    const auto& os_report = report.os_report();
    AddString(signals_dict, "device_enrollment_domain",
              os_report.device_enrollment_domain());
    AddString(signals_dict, "device_manufacturer",
              os_report.device_manufacturer());
    AddString(signals_dict, "device_model", os_report.device_model());
    AddString(signals_dict, "operating_system", os_report.name());
    AddString(signals_dict, "os_version", os_report.version());

#if BUILDFLAG(IS_ANDROID)
    MaybeAddValue(signals_dict, "has_potentially_harmful_apps",
                  os_report.has_has_potentially_harmful_apps(),
                  os_report.has_potentially_harmful_apps());
    MaybeAddValue(signals_dict, "verified_apps_enabled",
                  os_report.has_verified_apps_enabled(),
                  os_report.verified_apps_enabled());
    if (os_report.has_security_patch_ms()) {
      signals_dict.Set("security_patch_ms",
                       base::NumberToString(os_report.security_patch_ms()));
    }
#endif
  }

  if (!report.has_browser_report()) {
    base::JSONWriter::WriteWithOptions(signals_dict, /*options=*/0,
                                       &signals_json);
    return signals_json;
  }

  const auto& browser_report = report.browser_report();
  AddString(signals_dict, "browser_version", browser_report.browser_version());

  if (browser_report.chrome_user_profile_infos_size() == 1) {
    const auto& profile_info = browser_report.chrome_user_profile_infos(0);
    AddString(signals_dict, "profile_id", profile_info.profile_id());

    if (profile_info.has_profile_signals_report()) {
      const auto& profile_signals = profile_info.profile_signals_report();
      MaybeAddValue(signals_dict, "built_in_dns_client_enabled",
                    profile_signals.has_built_in_dns_client_enabled(),
                    profile_signals.built_in_dns_client_enabled());
      AddString(signals_dict, "profile_enrollment_domain",
                profile_signals.profile_enrollment_domain());
      MaybeAddValue(signals_dict, "realtime_url_check_mode",
                    profile_signals.has_realtime_url_check_mode(),
                    profile_signals.realtime_url_check_mode());
      MaybeAddValue(signals_dict, "safe_browsing_protection_level",
                    profile_signals.has_safe_browsing_protection_level(),
                    profile_signals.safe_browsing_protection_level());
      MaybeAddValue(signals_dict, "site_isolation_enabled",
                    profile_signals.has_site_isolation_enabled(),
                    profile_signals.site_isolation_enabled());
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

}  // namespace enterprise
