// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UTIL_H_
#define CHROME_UPDATER_WIN_UTIL_H_

#include <winerror.h>

#include <string>

#include "base/strings/string16.h"
#include "base/win/atl.h"
#include "base/win/windows_types.h"

namespace updater {

// Returns the last error as an HRESULT or E_FAIL if last error is NO_ERROR.
// This is not a drop in replacement for the HRESULT_FROM_WIN32 macro.
// The macro maps a NO_ERROR to S_OK, whereas the HRESULTFromLastError maps a
// NO_ERROR to E_FAIL.
HRESULT HRESULTFromLastError();

// Returns an HRESULT with a custom facility code representing an updater error.
template <typename Error>
HRESULT HRESULTFromUpdaterError(Error error) {
  constexpr ULONG kCustomerBit = 0x20000000;
  constexpr ULONG kFacilityOmaha = 67;
  return static_cast<HRESULT>(static_cast<ULONG>(SEVERITY_ERROR) |
                              kCustomerBit | (kFacilityOmaha << 16) |
                              static_cast<ULONG>(error));
}

// Checks whether a process is running with the image |executable|. Returns true
// if a process is found.
bool IsProcessRunning(const wchar_t* executable);

// Waits until every running instance of |executable| is stopped.
// Returns true if every running processes are stopped.
bool WaitForProcessesStopped(const wchar_t* executable);

bool InitializeCOMSecurity();

// Gets the handle to the module containing the given executing address.
HMODULE GetModuleHandleFromAddress(void* address);

// Gets the handle to the currently executing module.
HMODULE GetCurrentModuleHandle();

// Creates a unique event name and stores it in the specified environment var.
HRESULT CreateUniqueEventInEnvironment(const base::string16& var_name,
                                       bool is_machine,
                                       HANDLE* unique_event);

// Obtains a unique event name from specified environment var and opens it.
HRESULT OpenUniqueEventFromEnvironment(const base::string16& var_name,
                                       bool is_machine,
                                       HANDLE* unique_event);

struct NamedObjectAttributes {
  NamedObjectAttributes();
  ~NamedObjectAttributes();
  base::string16 name;
  CSecurityAttributes sa;
};

// For machine and local system, the prefix would be "Global\G{obj_name}".
// For user, the prefix would be "Global\G{user_sid}{obj_name}".
// For machine objects, returns a security attributes that gives permissions to
// both Admins and SYSTEM. This allows for cases where SYSTEM creates the named
// object first. The default DACL for SYSTEM will not allow Admins access.
void GetNamedObjectAttributes(const base::char16* base_name,
                              bool is_machine,
                              NamedObjectAttributes* attr);

// Creates an event based on the provided attributes.
HRESULT CreateEvent(NamedObjectAttributes* event_attr, HANDLE* event_handle);

// Gets the security descriptor with the default DACL for the current process
// user. The owner is the current user, the group is the current primary group.
// Returns true and populates sec_attr on success, false on failure.
bool GetCurrentUserDefaultSecurityAttributes(CSecurityAttributes* sec_attr);

// Get security attributes containing a DACL that grant the ACCESS_MASK access
// to admins and system.
void GetAdminDaclSecurityAttributes(CSecurityAttributes* sec_attr,
                                    ACCESS_MASK accessmask);

// Get security descriptor containing a DACL that grants the ACCESS_MASK access
// to admins and system.
void GetAdminDaclSecurityDescriptor(CSecurityDesc* sd, ACCESS_MASK accessmask);

// Returns the registry path for the Updater app id under the |Clients| subkey.
// The path does not include the registry root hive prefix.
base::string16 GetRegistryKeyClientsUpdater();

// Returns the registry path for the Updater app id under the |ClientState|
// subkey. The path does not include the registry root hive prefix.
base::string16 GetRegistryKeyClientStateUpdater();

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UTIL_H_
