// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/platform_utils.h"

#include <windows.h>

// SECURITY_WIN32 must be defined in order to get
// EXTENDED_NAME_FORMAT enumeration.
#define SECURITY_WIN32 1
#include <security.h>
#undef SECURITY_WIN32

#include <shobjidl.h>

#include <DSRole.h>
#include <iphlpapi.h>
#include <powersetting.h>
#include <propsys.h>

#include <optional>

#include "base/base_paths_win.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "base/win/wincred_shim.h"
#include "base/win/windows_version.h"
#include "base/win/wmi.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "components/device_signals/core/common/signals_constants.h"

namespace device_signals {

namespace {

constexpr wchar_t kCSAgentRegPath[] =
    L"SYSTEM\\CurrentControlSet\\services\\CSAgent\\Sim";

// CU is the registry value containing the customer ID.
constexpr wchar_t kCSCURegKey[] = L"CU";

// AG is the registry value containing the agent ID.
constexpr wchar_t kCSAGRegKey[] = L"AG";
constexpr wchar_t kSecureBootRegPath[] =
    L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State";
constexpr wchar_t kSecureBootRegKey[] = L"UEFISecureBootEnabled";

// Possible results of the "System.Volume.BitLockerProtection" shell property.
// These values are undocumented but were directly validated on a Windows 10
// machine. See the comment above the GetDiskEncryption() method.
// The values in this enum should be kep in sync with the analogous definiotion
// in the native app implementation.
enum class BitLockerStatus {
  // Encryption is on, and the volume is unlocked
  kOn = 1,
  // Encryption is off
  kOff = 2,
  // Encryption is in progress
  kEncryptionInProgress = 3,
  // Decryption is in progress
  kDecryptionInProgress = 4,
  // Encryption is on, but temporarily suspended
  kSuspended = 5,
  // Encryption is on, and the volume is locked
  kLocked = 6,
};

std::optional<std::string> GetHexStringRegValue(
    const base::win::RegKey& key,
    const std::wstring& reg_key_name) {
  DWORD type = REG_NONE;
  DWORD size = 0;
  auto res = key.ReadValue(reg_key_name.c_str(), nullptr, &size, &type);
  if (res == ERROR_SUCCESS && type == REG_BINARY) {
    std::vector<uint8_t> raw_bytes(size);
    res = key.ReadValue(reg_key_name.c_str(), raw_bytes.data(), &size, &type);

    if (res == ERROR_SUCCESS) {
      // Converting the values to lowercase specifically for CrowdStrike as
      // some of their APIs only accept the lowercase version.
      return base::HexEncodeLower(raw_bytes);
    }
  }

  return std::nullopt;
}

// Retrieves the state of the screen locking feature from the screen saver
// settings.
std::optional<bool> GetScreenLockStatus() {
  std::optional<bool> status;
  BOOL value = FALSE;
  if (::SystemParametersInfo(SPI_GETSCREENSAVESECURE, 0, &value, 0)) {
    status = static_cast<bool>(value);
  }
  return status;
}

// Checks if locking is enabled at the currently active power scheme.
std::optional<bool> GetConsoleLockStatus() {
  std::optional<bool> status;
  SYSTEM_POWER_STATUS sps;
  // https://docs.microsoft.com/en-us/windows/desktop/api/winbase/nf-winbase-getsystempowerstatus
  // Retrieves the power status of the system. The status indicates whether the
  // system is running on AC or DC power.
  if (!::GetSystemPowerStatus(&sps)) {
    return status;
  }

  LPGUID p_active_policy = nullptr;
  // https://docs.microsoft.com/en-us/windows/desktop/api/powersetting/nf-powersetting-powergetactivescheme
  // Retrieves the active power scheme and returns a GUID that identifies the
  // scheme.
  if (::PowerGetActiveScheme(nullptr, &p_active_policy) == ERROR_SUCCESS) {
    constexpr GUID kConsoleLock = {
        0x0E796BDB,
        0x100D,
        0x47D6,
        {0xA2, 0xD5, 0xF7, 0xD2, 0xDA, 0xA5, 0x1F, 0x51}};
    const GUID active_policy = *p_active_policy;
    ::LocalFree(p_active_policy);

    auto const power_read_current_value_func =
        sps.ACLineStatus != 0U ? &PowerReadACValue : &PowerReadDCValue;
    ULONG type;
    DWORD value;
    DWORD value_size = sizeof(value);
    // https://docs.microsoft.com/en-us/windows/desktop/api/powersetting/nf-powersetting-powerreadacvalue
    // Retrieves the AC/DC power value for the specified power setting.
    // NO_SUBGROUP_GUID to retrieve the setting for the default power scheme.
    // LPBYTE case is safe and is needed as the function expects generic byte
    // array buffer regardless of the exact value read as it is a generic
    // interface.
    if (power_read_current_value_func(
            nullptr, &active_policy, &NO_SUBGROUP_GUID, &kConsoleLock, &type,
            reinterpret_cast<LPBYTE>(&value), &value_size) == ERROR_SUCCESS) {
      status = value != 0U;
    }
  }

  return status;
}

// Returns the volume where the Windows OS is installed.
std::optional<std::wstring> GetOsVolume() {
  std::optional<std::wstring> volume;
  base::FilePath windows_dir;
  if (base::PathService::Get(base::DIR_WINDOWS, &windows_dir) &&
      windows_dir.IsAbsolute()) {
    std::vector<std::wstring> components = windows_dir.GetComponents();
    DCHECK(components.size());
    volume = components[0];
  }
  return volume;
}

bool GetPropVariantAsInt64(PROPVARIANT variant, int64_t* out_value) {
  switch (variant.vt) {
    case VT_I2:
      *out_value = variant.iVal;
      return true;
    case VT_UI2:
      *out_value = variant.uiVal;
      return true;
    case VT_I4:
      *out_value = variant.lVal;
      return true;
    case VT_UI4:
      *out_value = variant.ulVal;
      return true;
    case VT_INT:
      *out_value = variant.intVal;
      return true;
    case VT_UINT:
      *out_value = variant.uintVal;
      return true;
  }
  return false;
}

}  // namespace

bool ResolvePath(const base::FilePath& file_path,
                 base::FilePath* resolved_file_path) {
  auto expanded_path_wstring =
      base::win::ExpandEnvironmentVariables(file_path.value());
  if (!expanded_path_wstring) {
    return false;
  }

  auto expanded_file_path = base::FilePath(expanded_path_wstring.value());
  if (!base::PathExists(expanded_file_path)) {
    return false;
  }
  *resolved_file_path = base::MakeAbsoluteFilePath(expanded_file_path);
  return true;
}

std::optional<base::FilePath> GetProcessExePath(base::ProcessId pid) {
  base::Process process(
      ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
  if (!process.IsValid()) {
    return std::nullopt;
  }

  DWORD path_len = MAX_PATH;
  wchar_t path_string[MAX_PATH];
  if (!::QueryFullProcessImageName(process.Handle(), 0, path_string,
                                   &path_len)) {
    return std::nullopt;
  }

  return base::FilePath(path_string);
}

std::optional<CrowdStrikeSignals> GetCrowdStrikeSignals() {
  base::win::RegKey key;
  auto result = key.Open(HKEY_LOCAL_MACHINE, kCSAgentRegPath,
                         KEY_QUERY_VALUE | KEY_WOW64_64KEY);

  if (result == ERROR_SUCCESS && key.Valid()) {
    base::Value::Dict crowdstrike_info;

    auto customer_id = GetHexStringRegValue(key, kCSCURegKey);
    if (customer_id) {
      crowdstrike_info.Set(names::kCustomerId, customer_id.value());
    }

    auto agent_id = GetHexStringRegValue(key, kCSAGRegKey);
    if (agent_id) {
      crowdstrike_info.Set(names::kAgentId, agent_id.value());
    }

    if (customer_id || agent_id) {
      return CrowdStrikeSignals{customer_id.value_or(std::string()),
                                agent_id.value_or(std::string())};
    }
  }

  return std::nullopt;
}

base::FilePath GetCrowdStrikeZtaFilePath() {
  static constexpr base::FilePath::CharType kZtaFilePathSuffix[] =
      FILE_PATH_LITERAL("CrowdStrike\\ZeroTrustAssessment\\data.zta");

  base::FilePath app_data_dir;
  if (!base::PathService::Get(base::DIR_COMMON_APP_DATA, &app_data_dir)) {
    // Returning the empty path when failing.
    return app_data_dir;
  }
  return app_data_dir.Append(kZtaFilePathSuffix);
}

std::string GetDeviceModel() {
  return base::SysInfo::HardwareModelName();
}

// Retrieves the computer serial number from WMI.
std::string GetSerialNumber() {
  base::win::WmiComputerSystemInfo sys_info =
      base::win::WmiComputerSystemInfo::Get();
  return base::WideToUTF8(sys_info.serial_number());
}

// Gets cumulative screen locking policy based on the screen saver and console
// lock status.
SettingValue GetScreenlockSecured() {
  const std::optional<bool> screen_lock_status = GetScreenLockStatus();
  if (screen_lock_status.value_or(false)) {
    return SettingValue::ENABLED;
  }

  const std::optional<bool> console_lock_status = GetConsoleLockStatus();
  if (console_lock_status.value_or(false)) {
    return SettingValue::ENABLED;
  }

  if (screen_lock_status.has_value() || console_lock_status.has_value()) {
    return SettingValue::DISABLED;
  }

  return SettingValue::UNKNOWN;
}

// The ideal solution to check the disk encryption (BitLocker) status is to
// use the WMI APIs (Win32_EncryptableVolume). However, only programs running
// with elevated priledges can access those APIs.
//
// Our alternative solution is based on the value of the undocumented (shell)
// property: "System.Volume.BitLockerProtection". That property is essentially
// an enum containing the current BitLocker status for a given volume. This
// approached was suggested here:
// https://stackoverflow.com/questions/41308245/detect-bitlocker-programmatically-from-c-sharp-without-admin/41310139
//
// Note that the link above doesn't give any explanation / meaning for the
// enum values, it simply says that 1, 3 or 5 means the disk is encrypted.
//
// I directly tested and validated this strategy on a Windows 10 machine.
// The values given in the BitLockerStatus enum contain the relevant values
// for the shell property. I also directly validated them.
SettingValue GetDiskEncrypted() {
  // |volume| has to be a |wstring| because SHCreateItemFromParsingName() only
  // accepts |PCWSTR| which is |wchar_t*|.
  std::optional<std::wstring> volume = GetOsVolume();
  if (!volume.has_value()) {
    return SettingValue::UNKNOWN;
  }

  PROPERTYKEY key;
  const HRESULT property_key_result =
      PSGetPropertyKeyFromName(L"System.Volume.BitLockerProtection", &key);
  if (FAILED(property_key_result)) {
    return SettingValue::UNKNOWN;
  }

  Microsoft::WRL::ComPtr<IShellItem2> item;
  const HRESULT create_item_result = SHCreateItemFromParsingName(
      volume.value().c_str(), nullptr, IID_IShellItem2, &item);
  if (FAILED(create_item_result) || !item) {
    return SettingValue::UNKNOWN;
  }

  PROPVARIANT prop_status;
  const HRESULT property_result = item->GetProperty(key, &prop_status);
  int64_t status;
  if (FAILED(property_result) || !GetPropVariantAsInt64(prop_status, &status)) {
    return SettingValue::UNKNOWN;
  }

  // Note that we are not considering BitLockerStatus::Suspended as ENABLED.
  if (status == static_cast<int64_t>(BitLockerStatus::kOn) ||
      status == static_cast<int64_t>(BitLockerStatus::kEncryptionInProgress) ||
      status == static_cast<int64_t>(BitLockerStatus::kLocked)) {
    return SettingValue::ENABLED;
  }

  return SettingValue::DISABLED;
}

std::vector<std::string> internal::GetMacAddressesImpl() {
  std::vector<std::string> mac_addresses;
  ULONG adapter_info_size = 0;
  // Get the right buffer size in case of overflow
  if (::GetAdaptersInfo(nullptr, &adapter_info_size) != ERROR_BUFFER_OVERFLOW ||
      adapter_info_size == 0) {
    return mac_addresses;
  }

  std::vector<byte> adapters(adapter_info_size);
  if (::GetAdaptersInfo(reinterpret_cast<PIP_ADAPTER_INFO>(adapters.data()),
                        &adapter_info_size) != ERROR_SUCCESS) {
    return mac_addresses;
  }

  // The returned value is not an array of IP_ADAPTER_INFO elements but a linked
  // list of such
  PIP_ADAPTER_INFO adapter =
      reinterpret_cast<PIP_ADAPTER_INFO>(adapters.data());
  while (adapter) {
    if (adapter->AddressLength == 6) {
      mac_addresses.push_back(
          base::StringPrintf("%02X-%02X-%02X-%02X-%02X-%02X",
                             static_cast<unsigned int>(adapter->Address[0]),
                             static_cast<unsigned int>(adapter->Address[1]),
                             static_cast<unsigned int>(adapter->Address[2]),
                             static_cast<unsigned int>(adapter->Address[3]),
                             static_cast<unsigned int>(adapter->Address[4]),
                             static_cast<unsigned int>(adapter->Address[5])));
    }
    adapter = adapter->Next;
  }
  return mac_addresses;
}

SettingValue GetSecureBootEnabled() {
  base::win::RegKey key;
  auto result = key.Open(HKEY_LOCAL_MACHINE, kSecureBootRegPath,
                         KEY_QUERY_VALUE | KEY_WOW64_64KEY);

  if (result != ERROR_SUCCESS || !key.Valid()) {
    return SettingValue::UNKNOWN;
  }

  DWORD secure_boot_dw;
  result = key.ReadValueDW(kSecureBootRegKey, &secure_boot_dw);

  if (result != ERROR_SUCCESS) {
    return SettingValue::UNKNOWN;
  }

  return secure_boot_dw == 1 ? SettingValue::ENABLED : SettingValue::DISABLED;
}

std::optional<std::string> GetWindowsMachineDomain() {
  if (!base::win::IsEnrolledToDomain()) {
    return std::nullopt;
  }
  std::string domain;
  ::DSROLE_PRIMARY_DOMAIN_INFO_BASIC* info = nullptr;
  if (::DsRoleGetPrimaryDomainInformation(nullptr,
                                          ::DsRolePrimaryDomainInfoBasic,
                                          (PBYTE*)&info) == ERROR_SUCCESS) {
    if (info->DomainNameFlat) {
      domain = base::WideToUTF8(info->DomainNameFlat);
    }
    ::DsRoleFreeMemory(info);
  }
  return domain.empty() ? std::nullopt : std::make_optional(domain);
}

}  // namespace device_signals
