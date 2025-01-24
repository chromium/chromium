// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/user_crash_state.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/win/access_token.h"
#include "base/win/sid.h"
#include "build/branding_buildflags.h"
#include "chrome/install_static/install_util.h"
#include "chrome/windows_services/service_program/scoped_client_impersonation.h"

namespace {

std::wstring GetUserSidString(const base::Process& client_process) {
  if (auto token = base::win::AccessToken::FromProcess(client_process.Handle());
      token.has_value()) {
    if (auto sid_string = token->User().ToSddlString();
        sid_string.has_value()) {
      return *std::move(sid_string);
    }
  }
  return {};
}

base::win::RegKey GetClientStateMediumKey() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!install_static::IsSystemInstall()) {
    return {};  // Not needed for per-user installs.
  }

  return base::win::RegKey(
      HKEY_LOCAL_MACHINE, install_static::GetClientStateMediumKeyPath().c_str(),
      KEY_NOTIFY | KEY_QUERY_VALUE | KEY_WOW64_32KEY);
#else
  return {};  // No consent state to monitor for Chromium builds.
#endif
}

base::win::RegKey GetClientStateKey() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return base::win::RegKey(install_static::IsSystemInstall()
                               ? HKEY_LOCAL_MACHINE
                               : HKEY_CURRENT_USER,
                           install_static::GetClientStateKeyPath().c_str(),
                           KEY_NOTIFY | KEY_QUERY_VALUE | KEY_WOW64_32KEY);
#else
  return {};  // No consent state to monitor for Chromium builds.
#endif
}

base::win::RegKey GetProductKey() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return base::win::RegKey(HKEY_CURRENT_USER,
                           install_static::GetRegistryPath().c_str(),
                           KEY_NOTIFY | KEY_QUERY_VALUE | KEY_WOW64_32KEY);
#else
  return {};  // No consent state to monitor for Chromium builds.
#endif
}

}  // namespace

// static
std::unique_ptr<UserCrashState> UserCrashState::Create(
    const ScopedClientImpersonation& impersonation,
    const base::Process& client_process) {
  CHECK(impersonation.is_valid());
  if (auto sid_string = GetUserSidString(client_process); !sid_string.empty()) {
    return base::WrapUnique(
        new UserCrashState(std::move(sid_string), GetClientStateMediumKey(),
                           GetClientStateKey(), GetProductKey()));
  }
  return {};
}

UserCrashState::~UserCrashState() = default;

UserCrashState::UserCrashState(std::wstring user_sid,
                               base::win::RegKey client_state_medium_key,
                               base::win::RegKey client_state_key,
                               base::win::RegKey product_key)
    : user_sid_(std::move(user_sid)),
      client_state_medium_key_(std::move(client_state_medium_key)),
      client_state_key_(std::move(client_state_key)),
      product_key_(std::move(product_key)) {}
