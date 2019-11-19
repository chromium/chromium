// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/configuration.h"

#include <shellapi.h>  // NOLINT
#include <stddef.h>
#include <windows.h>

#include "build/branding_buildflags.h"
#include "chrome/installer/mini_installer/appid.h"
#include "chrome/installer/mini_installer/mini_installer_constants.h"
#include "chrome/installer/mini_installer/mini_installer_resource.h"
#include "chrome/installer/mini_installer/mini_string.h"
#include "chrome/installer/mini_installer/regkey.h"

namespace mini_installer {

namespace {

// Returns true if GoogleUpdateIsMachine=1 is present in the environment.
bool GetGoogleUpdateIsMachineEnvVar() {
  const DWORD kBufferSize = 2;
  StackString<kBufferSize> value;
  DWORD length = ::GetEnvironmentVariableW(L"GoogleUpdateIsMachine",
                                           value.get(), kBufferSize);
  return length == 1 && *value.get() == L'1';
}

}  // namespace

Configuration::Configuration() : args_(NULL) {
  Clear();
}

Configuration::~Configuration() {
  Clear();
}

bool Configuration::Initialize(HMODULE module) {
  Clear();
  ReadResources(module);
  return ParseCommandLine(::GetCommandLine());
}

const wchar_t* Configuration::program() const {
  return args_ == NULL || argument_count_ < 1 ? NULL : args_[0];
}

void Configuration::Clear() {
  if (args_ != NULL) {
    ::LocalFree(args_);
    args_ = NULL;
  }
  chrome_app_guid_ = google_update::kAppGuid;
  command_line_ = NULL;
  operation_ = INSTALL_PRODUCT;
  argument_count_ = 0;
  is_system_level_ = false;
  is_updating_multi_chrome_ = false;
  has_invalid_switch_ = false;
  previous_version_ = NULL;
}

// |command_line| is shared with this instance in the sense that this
// instance may refer to it at will throughout its lifetime, yet it will
// not release it.
bool Configuration::ParseCommandLine(const wchar_t* command_line) {
  command_line_ = command_line;
  args_ = ::CommandLineToArgvW(command_line_, &argument_count_);
  if (!args_)
    return false;

  for (int i = 1; i < argument_count_; ++i) {
    if (0 == ::lstrcmpi(args_[i], L"--system-level"))
      is_system_level_ = true;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    else if (0 == ::lstrcmpi(args_[i], L"--chrome-beta"))
      chrome_app_guid_ = google_update::kBetaAppGuid;
    else if (0 == ::lstrcmpi(args_[i], L"--chrome-dev"))
      chrome_app_guid_ = google_update::kDevAppGuid;
    else if (0 == ::lstrcmpi(args_[i], L"--chrome-sxs"))
      chrome_app_guid_ = google_update::kSxSAppGuid;
#endif
    else if (0 == ::lstrcmpi(args_[i], L"--cleanup"))
      operation_ = CLEANUP;
    else if (0 == ::lstrcmpi(args_[i], L"--chrome-frame"))
      has_invalid_switch_ = true;
  }

  if (!is_system_level_)
    is_system_level_ = GetGoogleUpdateIsMachineEnvVar();

  is_updating_multi_chrome_ = IsUpdatingMultiChrome();

  return true;
}

void Configuration::ReadResources(HMODULE module) {
  HRSRC resource_info_block =
      FindResource(module, MAKEINTRESOURCE(ID_PREVIOUS_VERSION), RT_RCDATA);
  if (!resource_info_block)
    return;

  HGLOBAL data_handle = LoadResource(module, resource_info_block);
  if (!data_handle)
    return;

  // The data is a Unicode string, so it must be a multiple of two bytes.
  DWORD version_size = SizeofResource(module, resource_info_block);
  if (!version_size || (version_size & 0x01) != 0)
    return;

  void* version_data = LockResource(data_handle);
  if (!version_data)
    return;

  const wchar_t* version_string = reinterpret_cast<wchar_t*>(version_data);
  size_t version_len = version_size / sizeof(wchar_t);

  // The string must be terminated.
  if (version_string[version_len - 1])
    return;

  previous_version_ = version_string;
}

bool Configuration::IsUpdatingMultiChrome() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Only primary Chrome installs supported multi-install (not canary/SxS).
  if (chrome_app_guid_ != google_update::kAppGuid)
    return false;

  // Is Chrome already installed as multi-install?
  const HKEY root = is_system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  StackString<128> value;
  RegKey key;
  return (OpenClientsKey(root, chrome_app_guid_, KEY_QUERY_VALUE, &key) ==
              ERROR_SUCCESS &&
          key.ReadSZValue(kPvRegistryValue, value.get(), value.capacity()) ==
              ERROR_SUCCESS &&
          value.length() != 0 &&
          OpenClientStateKey(root, chrome_app_guid_, KEY_QUERY_VALUE, &key) ==
              ERROR_SUCCESS &&
          key.ReadSZValue(kUninstallArgumentsRegistryValue, value.get(),
                          value.capacity()) == ERROR_SUCCESS &&
          value.findi(L"--multi-install") != nullptr);
#else
  return false;
#endif
}

}  // namespace mini_installer
