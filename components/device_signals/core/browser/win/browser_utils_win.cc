// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/browser_utils.h"

#include <windows.h>

// SECURITY_WIN32 must be defined in order to get
// EXTENDED_NAME_FORMAT enumeration.
#define SECURITY_WIN32 1
#include <security.h>
#undef SECURITY_WIN32

#include <shobjidl.h>
#include <winsock2.h>

#include <DSRole.h>
#include <iphlpapi.h>
#include <netfw.h>
#include <powersetting.h>
#include <propsys.h>
#include <wrl/client.h>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/wincred_shim.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "net/base/network_interfaces.h"
#include "net/dns/public/win_dns_system_settings.h"

namespace device_signals {

namespace {

// Registry for device ID.
constexpr wchar_t kRegKeyCryptographyKey[] =
    L"SOFTWARE\\Microsoft\\Cryptography\\";
constexpr wchar_t kRegValueMachineGuid[] = L"MachineGuid";

bool ReadRegistryString(const std::wstring& key_path,
                        const std::wstring& name,
                        REGSAM reg_view,
                        std::string& value) {
  std::wstring data;
  const LONG result = base::win::RegKey(HKEY_LOCAL_MACHINE, key_path.c_str(),
                                        reg_view | KEY_READ)
                          .ReadValue(name.c_str(), &data);
  if (result != ERROR_SUCCESS) {
    VLOG(1) << __func__ << ": failed to read registry: " << key_path << "@"
            << name;
    return false;
  }
  value = base::SysWideToUTF8(data);
  return true;
}

}  // namespace

std::string GetHostName() {
  return policy::GetDeviceFqdn();
}

std::vector<std::string> GetSystemDnsServers() {
  std::vector<std::string> dns_addresses;
  std::optional<std::vector<net::IPEndPoint>> nameservers;
  base::expected<net::WinDnsSystemSettings, net::ReadWinSystemDnsSettingsError>
      settings = net::ReadWinSystemDnsSettings();
  if (settings.has_value()) {
    nameservers = settings->GetAllNameservers();
  }

  if (nameservers.has_value()) {
    for (const net::IPEndPoint& nameserver : nameservers.value()) {
      dns_addresses.push_back(nameserver.ToString());
    }
  }

  return dns_addresses;
}

SettingValue GetOSFirewall() {
  Microsoft::WRL::ComPtr<INetFwPolicy2> firewall_policy;
  HRESULT hr = CoCreateInstance(CLSID_NetFwPolicy2, nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&firewall_policy));
  if (FAILED(hr)) {
    DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
    return SettingValue::UNKNOWN;
  }

  long profile_types = 0;
  hr = firewall_policy->get_CurrentProfileTypes(&profile_types);
  if (FAILED(hr)) {
    return SettingValue::UNKNOWN;
  }

  // The most restrictive active profile takes precedence.
  constexpr NET_FW_PROFILE_TYPE2 kProfileTypes[] = {
      NET_FW_PROFILE2_PUBLIC, NET_FW_PROFILE2_PRIVATE, NET_FW_PROFILE2_DOMAIN};
  for (size_t i = 0; i < std::size(kProfileTypes); ++i) {
    if ((profile_types & UNSAFE_TODO(kProfileTypes[i])) != 0) {
      VARIANT_BOOL enabled = VARIANT_TRUE;
      hr = firewall_policy->get_FirewallEnabled(UNSAFE_TODO(kProfileTypes[i]),
                                                &enabled);
      if (FAILED(hr)) {
        return SettingValue::UNKNOWN;
      }
      if (enabled == VARIANT_TRUE) {
        return SettingValue::ENABLED;
      } else if (enabled == VARIANT_FALSE) {
        return SettingValue::DISABLED;
      } else {
        return SettingValue::UNKNOWN;
      }
    }
  }
  return SettingValue::UNKNOWN;
}

std::optional<std::string> GetWindowsUserDomain() {
  WCHAR username[CREDUI_MAX_USERNAME_LENGTH + 1] = {};
  DWORD username_length = sizeof(username);
  if (!::GetUserNameExW(::NameSamCompatible, username, &username_length) ||
      username_length <= 0) {
    return std::nullopt;
  }
  // The string corresponds to DOMAIN\USERNAME. If there isn't a domain, the
  // domain name is replaced by the name of the machine, so the function
  // returns nothing in that case.
  std::string username_str = base::WideToUTF8(username);
  std::string domain = username_str.substr(0, username_str.find("\\"));

  return domain == base::ToUpperASCII(GetHostName())
             ? std::nullopt
             : std::make_optional(domain);
}

std::optional<std::string> GetMachineGuid() {
  std::string machine_guid;
  if (!ReadRegistryString(kRegKeyCryptographyKey, kRegValueMachineGuid,
                          KEY_READ | KEY_WOW64_64KEY, machine_guid)) {
    return std::nullopt;
  }
  return machine_guid;
}

}  // namespace device_signals
