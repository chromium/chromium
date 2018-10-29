// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_OS_PROCESS_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_OS_PROCESS_MANAGER_H_

#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"

struct _STARTUPINFOW;

namespace base {

class CommandLine;

namespace win {

class ScopedProcessInformation;

}  // namespace win
}  // namespace base

namespace credential_provider {

// Manages OS processes and process attributes.
class OSProcessManager {
 public:
  static OSProcessManager* Get();

  virtual ~OSProcessManager();

  // Gets the logon SID from the specified logon token.  The call must release
  // the returned |sid| by calling LocalFree().
  virtual HRESULT GetTokenLogonSID(const base::win::ScopedHandle& token,
                                   PSID* sid);

  // Sets up permissions for the given logon SID so that it can access the
  // interactive desktop of the window station.
  virtual HRESULT SetupPermissionsForLogonSid(PSID sid);

  // Creates a process with the specified logon token.  The process is initially
  // suspend and must be resumed by the caller.
  virtual HRESULT CreateProcessWithToken(
      const base::win::ScopedHandle& logon_token,
      const base::CommandLine& command_line,
      _STARTUPINFOW* startupinfo,
      base::win::ScopedProcessInformation* procinfo);

  // Creates a running process using the same security context as the caller.
  virtual HRESULT CreateRunningProcess(
      const base::CommandLine& command_line,
      _STARTUPINFOW* startupinfo,
      base::win::ScopedProcessInformation* procinfo);

 protected:
  OSProcessManager() {}

  // Returns the storage used for the instance pointer.
  static OSProcessManager** GetInstanceStorage();
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_OS_PROCESS_MANAGER_H_
