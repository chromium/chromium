// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_USER_CRASH_STATE_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_USER_CRASH_STATE_H_

#include <memory>
#include <string>

#include "base/win/registry.h"

namespace base {
class Process;
}

class ScopedClientImpersonation;

// State for crash reporting that is associated with the user on behalf of which
// the service is running. An instance of this class must be created while
// impersonating the client.
class UserCrashState {
 public:
  // Creates the crash reporting state for the user running `client_process`.
  static std::unique_ptr<UserCrashState> Create(
      const ScopedClientImpersonation& impersonation,
      const base::Process& client_process);

  UserCrashState(const UserCrashState&) = delete;
  UserCrashState& operator=(const UserCrashState&) = delete;
  ~UserCrashState();

  const std::wstring& user_sid() const { return user_sid_; }
  base::win::RegKey& client_state_medium_key() {
    return client_state_medium_key_;
  }
  base::win::RegKey& client_state_key() { return client_state_key_; }
  base::win::RegKey& product_key() { return product_key_; }

 private:
  UserCrashState(std::wstring user_sid,
                 base::win::RegKey client_state_medium_key,
                 base::win::RegKey client_state_key,
                 base::win::RegKey product_key);

  const std::wstring user_sid_;
  base::win::RegKey client_state_medium_key_;
  base::win::RegKey client_state_key_;
  base::win::RegKey product_key_;
};

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_USER_CRASH_STATE_H_
