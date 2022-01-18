// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/connectors_internals/zero_trust_utils.h"

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/signature_verifier.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

namespace enterprise_connectors {
namespace utils {

namespace {

using google::protobuf::RepeatedPtrField;

void TrySetSignal(base::flat_map<std::string, std::string>& map,
                  const std::string& key,
                  bool has_value,
                  const std::string& string_value) {
  if (has_value) {
    map[key] = string_value;
  }
}

void TrySetSignal(base::flat_map<std::string, std::string>& map,
                  const std::string& key,
                  bool has_value,
                  bool bool_value) {
  if (has_value) {
    map[key] = bool_value ? "true" : "false";
  }
}

void TrySetSignal(base::flat_map<std::string, std::string>& map,
                  const std::string& key,
                  bool has_value,
                  int int_value) {
  if (has_value) {
    map[key] = base::NumberToString(int_value);
  }
}

// Encodes repeated fields into a single string with values separated by commas.
void TrySetSignal(base::flat_map<std::string, std::string>& map,
                  const std::string& key,
                  const RepeatedPtrField<std::string>& values) {
  if (values.empty()) {
    return;
  }

  map[key] = base::JoinString(
      std::vector<base::StringPiece>(values.begin(), values.end()), ", ");
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

connectors_internals::mojom::KeyTrustLevel ParseTrustLevel(
    BPKUR::KeyTrustLevel trust_level) {
  switch (trust_level) {
    case BPKUR::CHROME_BROWSER_TPM_KEY:
      return connectors_internals::mojom::KeyTrustLevel::TPM;
    case BPKUR::CHROME_BROWSER_OS_KEY:
      return connectors_internals::mojom::KeyTrustLevel::OS;
    default:
      return connectors_internals::mojom::KeyTrustLevel::UNSPECIFIED;
  }
}

connectors_internals::mojom::KeyType AlgorithmToType(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm) {
  switch (algorithm) {
    case crypto::SignatureVerifier::RSA_PKCS1_SHA1:
    case crypto::SignatureVerifier::RSA_PKCS1_SHA256:
    case crypto::SignatureVerifier::RSA_PSS_SHA256:
      return connectors_internals::mojom::KeyType::RSA;
    case crypto::SignatureVerifier::ECDSA_SHA256:
      return connectors_internals::mojom::KeyType::EC;
  }
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

}  // namespace

base::flat_map<std::string, std::string> SignalsToMap(
    std::unique_ptr<SignalsType> signals) {
  base::flat_map<std::string, std::string> map;

  if (!signals) {
    return map;
  }

  TrySetSignal(map, "device_id", signals->has_device_id(),
               signals->device_id());
  TrySetSignal(map, "obfuscated_customer_id",
               signals->has_obfuscated_customer_id(),
               signals->obfuscated_customer_id());
  TrySetSignal(map, "serial_number", signals->has_serial_number(),
               signals->serial_number());
  TrySetSignal(map, "display_name", signals->has_display_name(),
               signals->display_name());
  TrySetSignal(map, "os", signals->has_os(), signals->os());
  TrySetSignal(map, "os_version", signals->has_os_version(),
               signals->os_version());
  TrySetSignal(map, "device_manufacturer", signals->has_device_manufacturer(),
               signals->device_manufacturer());
  TrySetSignal(map, "device_model", signals->has_device_model(),
               signals->device_model());
  TrySetSignal(map, "imei", signals->imei());
  TrySetSignal(map, "meid", signals->meid());
  TrySetSignal(map, "tpm_hash", signals->has_tpm_hash(), signals->tpm_hash());
  TrySetSignal(map, "is_disk_encrypted", signals->has_is_disk_encrypted(),
               signals->is_disk_encrypted());
  TrySetSignal(map, "allow_screen_lock", signals->has_allow_screen_lock(),
               signals->allow_screen_lock());
  TrySetSignal(map, "is_protected_by_password",
               signals->has_is_protected_by_password(),
               signals->is_protected_by_password());
  TrySetSignal(map, "is_jailbroken", signals->has_is_jailbroken(),
               signals->is_jailbroken());
  TrySetSignal(map, "enrollment_domain", signals->has_enrollment_domain(),
               signals->enrollment_domain());
  TrySetSignal(map, "browser_version", signals->has_browser_version(),
               signals->browser_version());
  TrySetSignal(map, "safe_browsing_protection_level",
               signals->has_safe_browsing_protection_level(),
               signals->safe_browsing_protection_level());
  TrySetSignal(map, "site_isolation_enabled",
               signals->has_site_isolation_enabled(),
               signals->site_isolation_enabled());
  TrySetSignal(map, "third_party_blocking_enabled",
               signals->has_third_party_blocking_enabled(),
               signals->third_party_blocking_enabled());
  TrySetSignal(map, "remote_desktop_available",
               signals->has_remote_desktop_available(),
               signals->remote_desktop_available());
  TrySetSignal(map, "signed_in_profile_name",
               signals->has_signed_in_profile_name(),
               signals->signed_in_profile_name());
  TrySetSignal(map, "chrome_cleanup_enabled",
               signals->has_chrome_cleanup_enabled(),
               signals->chrome_cleanup_enabled());
  TrySetSignal(map, "password_protection_warning_trigger",
               signals->has_password_protection_warning_trigger(),
               signals->password_protection_warning_trigger());
  TrySetSignal(map, "dns_address", signals->has_dns_address(),
               signals->dns_address());
  TrySetSignal(map, "built_in_dns_client_enabled",
               signals->has_built_in_dns_client_enabled(),
               signals->built_in_dns_client_enabled());
  TrySetSignal(map, "firewall_on", signals->has_firewall_on(),
               signals->firewall_on());
  TrySetSignal(map, "windows_domain", signals->has_windows_domain(),
               signals->windows_domain());

  return map;
}

connectors_internals::mojom::KeyInfoPtr GetKeyInfo() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  auto* key_manager = g_browser_process->browser_policy_connector()
                          ->chrome_browser_cloud_management_controller()
                          ->GetDeviceTrustKeyManager();
  if (key_manager) {
    auto metadata = key_manager->GetLoadedKeyMetadata();
    if (metadata) {
      return connectors_internals::mojom::KeyInfo::New(
          connectors_internals::mojom::KeyManagerInitializedValue::KEY_LOADED,
          ParseTrustLevel(metadata->trust_level),
          AlgorithmToType(metadata->algorithm));
    } else {
      return connectors_internals::mojom::KeyInfo::New(
          connectors_internals::mojom::KeyManagerInitializedValue::NO_KEY,
          connectors_internals::mojom::KeyTrustLevel::UNSPECIFIED,
          connectors_internals::mojom::KeyType::UNKNOWN);
    }
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return connectors_internals::mojom::KeyInfo::New(
      connectors_internals::mojom::KeyManagerInitializedValue::UNSUPPORTED,
      connectors_internals::mojom::KeyTrustLevel::UNSPECIFIED,
      connectors_internals::mojom::KeyType::UNKNOWN);
}

}  // namespace utils
}  // namespace enterprise_connectors
