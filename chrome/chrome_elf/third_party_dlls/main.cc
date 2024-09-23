// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/third_party_dlls/main.h"

#include <windows.h>

#include <assert.h>
#include <versionhelpers.h>

#include <limits>

#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/chrome_elf/third_party_dlls/hook.h"
#include "chrome/chrome_elf/third_party_dlls/logs.h"
#include "chrome/chrome_elf/third_party_dlls/packed_list_file.h"
#include "chrome/chrome_elf/third_party_dlls/packed_list_format.h"
#include "chrome/chrome_elf/third_party_dlls/status_codes.h"
#include "chrome/install_static/install_util.h"

namespace third_party_dlls {
namespace {

// Record if all the third-party DLL management code was successfully
// initialized, so processes can easily determine if it is enabled for them.
bool g_third_party_initialized = false;

//------------------------------------------------------------------------------
// Private functions
//------------------------------------------------------------------------------

// Clear all status codes.
bool ResetStatusCodes() {
  HANDLE key_handle = nullptr;

  // If the ThirdParty registry key does not exist, it will be created now.
  if (!nt::CreateRegKey(nt::HKCU,
                        install_static::GetRegistryPath()
                            .append(kThirdPartyRegKeyName)
                            .c_str(),
                        KEY_WRITE, &key_handle)) {
    return false;
  }

  bool success = nt::SetRegKeyValue(key_handle, kStatusCodesRegValue,
                                    REG_BINARY, nullptr, 0);
  nt::CloseRegKey(key_handle);

  return success;
}

// Store a status code for later consumption.
void AddStatusCode(ThirdPartyStatus code) {
  HANDLE key_handle = nullptr;

  if (!nt::CreateRegKey(nt::HKCU,
                        install_static::GetRegistryPath()
                            .append(kThirdPartyRegKeyName)
                            .c_str(),
                        KEY_WRITE | KEY_READ, &key_handle)) {
    return;
  }

  std::vector<BYTE> value_bytes;
  ULONG value_type = REG_NONE;
  // Query for the existing value and sanity check any existing content.
  // Note: If non-existent, or corrupt, carry on and overwrite.
  if (!nt::QueryRegKeyValue(key_handle, kStatusCodesRegValue, &value_type,
                            &value_bytes) ||
      value_type != REG_BINARY) {
    value_bytes.clear();
  }

  AddStatusCodeToBuffer(code, &value_bytes);

  assert(value_bytes.size() < std::numeric_limits<DWORD>::max());
  nt::SetRegKeyValue(key_handle, kStatusCodesRegValue, REG_BINARY,
                     value_bytes.data(),
                     static_cast<DWORD>(value_bytes.size()));
  nt::CloseRegKey(key_handle);

  return;
}

}  // namespace

//------------------------------------------------------------------------------
// Public defines & functions
//------------------------------------------------------------------------------

bool Init() {
  // Debug check: Init should not be called more than once.
  assert(!g_third_party_initialized);

  // Sanity check: third_party_dlls should only be enabled in the browser
  // process at this time.
  if (!install_static::IsBrowserProcess())
    return false;

  // Zero tolerance for unsupported versions of Windows.  Third-party control
  // is too entwined with the operating system.
  if (!::IsWindows7OrGreater())
    return false;

  if (!ResetStatusCodes())
    AddStatusCode(ThirdPartyStatus::kStatusCodeResetFailure);

  // 1) Initialize the blocklist from file
  ThirdPartyStatus status = InitFromFile();
  if (status != ThirdPartyStatus::kSuccess) {
    AddStatusCode(status);
    // A few status codes are considered acceptable here.
    if (!IsStatusCodeSuccessful(status))
      return false;
  }

  // 2) InitLogs
  status = InitLogs();
  if (status != ThirdPartyStatus::kSuccess) {
    AddStatusCode(status);
    DeinitFromFile();
    return false;
  }

  // 3) Apply the hook only after everything else is successfully set up.
  status = ApplyHook();
  if (status != ThirdPartyStatus::kSuccess) {
    AddStatusCode(status);
    DeinitLogs();
    DeinitFromFile();
    return false;
  }

  // Record initialization.
  g_third_party_initialized = true;

  return true;
}

//------------------------------------------------------------------------------
// Testing-only access to status code APIs.
//------------------------------------------------------------------------------
bool ResetStatusCodesForTesting() {
  return ResetStatusCodes();
}

void AddStatusCodeForTesting(ThirdPartyStatus code) {
  AddStatusCode(code);
}

}  // namespace third_party_dlls

bool IsThirdPartyInitialized() {
  return third_party_dlls::g_third_party_initialized;
}
