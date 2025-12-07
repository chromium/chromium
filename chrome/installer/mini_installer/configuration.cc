// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/mini_installer/configuration.h"

#include <windows.h>

#include <shellapi.h>
#include <stddef.h>
#include <stdlib.h>

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

Configuration::Configuration() : args_(nullptr) {
  Clear();
}

Configuration::~Configuration() {
  Clear();
}

bool Configuration::Initialize() {
  Clear();
  ReadRegistry();
  return ParseCommandLine(::GetCommandLine());
}

const wchar_t* Configuration::program() const {
  return args_ == nullptr || argument_count_ < 1 ? nullptr : args_[0];
}

void Configuration::Clear() {
  if (args_ != nullptr) {
    ::LocalFree(args_);
    args_ = nullptr;
  }
  chrome_app_guid_ = google_update::kAppGuid;
  command_line_ = nullptr;
  argument_count_ = 0;
  is_system_level_ = false;
  has_invalid_switch_ = false;
  should_delete_extracted_files_ = true;
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
      has_invalid_switch_ = true;
    else if (0 == ::lstrcmpi(args_[i], L"--chrome-frame"))
      has_invalid_switch_ = true;
  }

  if (!is_system_level_)
    is_system_level_ = GetGoogleUpdateIsMachineEnvVar();

  return true;
}

void Configuration::ReadRegistry() {
  // Extracted files should not be deleted iff the user has manually created a
  // ChromeInstallerCleanup string value in the registry under
  // HKCU\Software\[Google|Chromium] and set its value to "0".
  wchar_t value[2] = {};
  should_delete_extracted_files_ =
      !RegKey::ReadSZValue(HKEY_CURRENT_USER, kCleanupRegistryKey,
                           kCleanupRegistryValue, value, _countof(value)) ||
      value[0] != L'0';
}

}  // namespace mini_installer
