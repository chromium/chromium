// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/connectors_internals/zero_trust_utils.h"

#include "base/strings/string_number_conversions.h"

namespace enterprise_connectors {
namespace utils {

namespace {

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
  TrySetSignal(map, "device_manufacturer", signals->has_device_manufacturer(),
               signals->device_manufacturer());
  TrySetSignal(map, "device_model", signals->has_device_model(),
               signals->device_model());
  TrySetSignal(map, "imei", signals->has_imei(), signals->imei());
  TrySetSignal(map, "meid", signals->has_meid(), signals->meid());
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

}  // namespace utils
}  // namespace enterprise_connectors
