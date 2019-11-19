// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/util.h"

#include <aclapi.h>
#include <shlobj.h>
#include <windows.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/process/process_iterator.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/updater/updater_constants.h"
#include "chrome/updater/win/constants.h"
#include "chrome/updater/win/user_info.h"

namespace updater {

namespace {

// The number of iterations to poll if a process is stopped correctly.
const unsigned int kMaxProcessQueryIterations = 50;

// The sleep time in ms between each poll.
const unsigned int kProcessQueryWaitTimeMs = 100;

}  // namespace

NamedObjectAttributes::NamedObjectAttributes() = default;
NamedObjectAttributes::~NamedObjectAttributes() = default;

HRESULT HRESULTFromLastError() {
  const auto error_code = ::GetLastError();
  return (error_code != NO_ERROR) ? HRESULT_FROM_WIN32(error_code) : E_FAIL;
}

bool IsProcessRunning(const wchar_t* executable) {
  base::NamedProcessIterator iter(executable, nullptr);
  const base::ProcessEntry* entry = iter.NextProcessEntry();
  return entry != nullptr;
}

bool WaitForProcessesStopped(const wchar_t* executable) {
  DCHECK(executable);
  VLOG(1) << "Wait for processes '" << executable << "'.";

  // Wait until the process is completely stopped.
  for (unsigned int iteration = 0; iteration < kMaxProcessQueryIterations;
       ++iteration) {
    if (!IsProcessRunning(executable))
      return true;
    ::Sleep(kProcessQueryWaitTimeMs);
  }

  // The process didn't terminate.
  LOG(ERROR) << "Cannot stop process '" << executable << "', timeout.";
  return false;
}

// This sets up COM security to allow NetworkService, LocalService, and System
// to call back into the process. It is largely inspired by
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa378987.aspx
// static
bool InitializeCOMSecurity() {
  // Create the security descriptor explicitly as follows because
  // CoInitializeSecurity() will not accept the relative security descriptors
  // returned by ConvertStringSecurityDescriptorToSecurityDescriptor().
  const size_t kSidCount = 5;
  uint64_t* sids[kSidCount][(SECURITY_MAX_SID_SIZE + sizeof(uint64_t) - 1) /
                            sizeof(uint64_t)] = {
      {}, {}, {}, {}, {},
  };

  // These are ordered by most interesting ones to try first.
  WELL_KNOWN_SID_TYPE sid_types[kSidCount] = {
      WinBuiltinAdministratorsSid,  // administrator group security identifier
      WinLocalServiceSid,           // local service security identifier
      WinNetworkServiceSid,         // network service security identifier
      WinSelfSid,                   // personal account security identifier
      WinLocalSystemSid,            // local system security identifier
  };

  // This creates a security descriptor that is equivalent to the following
  // security descriptor definition language (SDDL) string:
  //   O:BAG:BAD:(A;;0x1;;;LS)(A;;0x1;;;NS)(A;;0x1;;;PS)
  //   (A;;0x1;;;SY)(A;;0x1;;;BA)

  // Initialize the security descriptor.
  SECURITY_DESCRIPTOR security_desc = {};
  if (!::InitializeSecurityDescriptor(&security_desc,
                                      SECURITY_DESCRIPTOR_REVISION))
    return false;

  DCHECK_EQ(kSidCount, base::size(sids));
  DCHECK_EQ(kSidCount, base::size(sid_types));
  for (size_t i = 0; i < kSidCount; ++i) {
    DWORD sid_bytes = sizeof(sids[i]);
    if (!::CreateWellKnownSid(sid_types[i], nullptr, sids[i], &sid_bytes))
      return false;
  }

  // Setup the access control entries (ACE) for COM. You may need to modify
  // the access permissions for your application. COM_RIGHTS_EXECUTE and
  // COM_RIGHTS_EXECUTE_LOCAL are the minimum access rights required.
  EXPLICIT_ACCESS explicit_access[kSidCount] = {};
  DCHECK_EQ(kSidCount, base::size(sids));
  DCHECK_EQ(kSidCount, base::size(explicit_access));
  for (size_t i = 0; i < kSidCount; ++i) {
    explicit_access[i].grfAccessPermissions =
        COM_RIGHTS_EXECUTE | COM_RIGHTS_EXECUTE_LOCAL;
    explicit_access[i].grfAccessMode = SET_ACCESS;
    explicit_access[i].grfInheritance = NO_INHERITANCE;
    explicit_access[i].Trustee.pMultipleTrustee = nullptr;
    explicit_access[i].Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    explicit_access[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    explicit_access[i].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    explicit_access[i].Trustee.ptstrName = reinterpret_cast<LPTSTR>(sids[i]);
  }

  // Create an access control list (ACL) using this ACE list, if this succeeds
  // make sure to ::LocalFree(acl).
  ACL* acl = nullptr;
  DWORD acl_result = ::SetEntriesInAcl(base::size(explicit_access),
                                       explicit_access, nullptr, &acl);
  if (acl_result != ERROR_SUCCESS || acl == nullptr)
    return false;

  HRESULT hr = E_FAIL;

  // Set the security descriptor owner and group to Administrators and set the
  // discretionary access control list (DACL) to the ACL.
  if (::SetSecurityDescriptorOwner(&security_desc, sids[0], FALSE) &&
      ::SetSecurityDescriptorGroup(&security_desc, sids[0], FALSE) &&
      ::SetSecurityDescriptorDacl(&security_desc, TRUE, acl, FALSE)) {
    // Initialize COM. You may need to modify the parameters of
    // CoInitializeSecurity() for your application. Note that an
    // explicit security descriptor is being passed down.
    hr = ::CoInitializeSecurity(
        &security_desc, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        RPC_C_IMP_LEVEL_IDENTIFY, nullptr,
        EOAC_DISABLE_AAA | EOAC_NO_CUSTOM_MARSHAL, nullptr);
  }

  ::LocalFree(acl);
  return SUCCEEDED(hr);
}

HMODULE GetModuleHandleFromAddress(void* address) {
  MEMORY_BASIC_INFORMATION mbi = {0};
  size_t result = ::VirtualQuery(address, &mbi, sizeof(mbi));
  DCHECK_EQ(result, sizeof(mbi));
  return static_cast<HMODULE>(mbi.AllocationBase);
}

HMODULE GetCurrentModuleHandle() {
  return GetModuleHandleFromAddress(
      reinterpret_cast<void*>(&GetCurrentModuleHandle));
}

// The event name saved to the environment variable does not contain the
// decoration added by GetNamedObjectAttributes.
HRESULT CreateUniqueEventInEnvironment(const base::string16& var_name,
                                       bool is_machine,
                                       HANDLE* unique_event) {
  DCHECK(unique_event);

  const base::string16 event_name = base::ASCIIToUTF16(base::GenerateGUID());
  NamedObjectAttributes attr;
  GetNamedObjectAttributes(event_name.c_str(), is_machine, &attr);

  HRESULT hr = CreateEvent(&attr, unique_event);
  if (FAILED(hr))
    return hr;

  if (!::SetEnvironmentVariable(var_name.c_str(), event_name.c_str())) {
    DWORD error = ::GetLastError();
    return HRESULT_FROM_WIN32(error);
  }

  return S_OK;
}

HRESULT OpenUniqueEventFromEnvironment(const base::string16& var_name,
                                       bool is_machine,
                                       HANDLE* unique_event) {
  DCHECK(unique_event);

  base::char16 event_name[MAX_PATH] = {0};
  if (!::GetEnvironmentVariable(var_name.c_str(), event_name,
                                base::size(event_name))) {
    DWORD error = ::GetLastError();
    return HRESULT_FROM_WIN32(error);
  }

  NamedObjectAttributes attr;
  GetNamedObjectAttributes(event_name, is_machine, &attr);
  *unique_event = ::OpenEvent(EVENT_ALL_ACCESS, false, attr.name.c_str());

  if (!*unique_event) {
    DWORD error = ::GetLastError();
    return HRESULT_FROM_WIN32(error);
  }

  return S_OK;
}

HRESULT CreateEvent(NamedObjectAttributes* event_attr, HANDLE* event_handle) {
  DCHECK(event_handle);
  DCHECK(event_attr);
  DCHECK(!event_attr->name.empty());
  *event_handle = ::CreateEvent(&event_attr->sa,
                                true,   // manual reset
                                false,  // not signaled
                                event_attr->name.c_str());

  if (!*event_handle) {
    DWORD error = ::GetLastError();
    return HRESULT_FROM_WIN32(error);
  }

  return S_OK;
}

void GetNamedObjectAttributes(const base::char16* base_name,
                              bool is_machine,
                              NamedObjectAttributes* attr) {
  DCHECK(base_name);
  DCHECK(attr);

  attr->name = kGlobalPrefix;

  if (!is_machine) {
    base::string16 user_sid;
    GetProcessUser(nullptr, nullptr, &user_sid);
    attr->name += user_sid;
    GetCurrentUserDefaultSecurityAttributes(&attr->sa);
  } else {
    // Grant access to administrators and system.
    GetAdminDaclSecurityAttributes(&attr->sa, GENERIC_ALL);
  }

  attr->name += base_name;
}

bool GetCurrentUserDefaultSecurityAttributes(CSecurityAttributes* sec_attr) {
  DCHECK(sec_attr);

  CAccessToken token;
  if (!token.GetProcessToken(TOKEN_QUERY))
    return false;

  CSecurityDesc security_desc;
  CSid sid_owner;
  if (!token.GetOwner(&sid_owner))
    return false;

  security_desc.SetOwner(sid_owner);
  CSid sid_group;
  if (!token.GetPrimaryGroup(&sid_group))
    return false;

  security_desc.SetGroup(sid_group);

  CDacl dacl;
  if (!token.GetDefaultDacl(&dacl))
    return false;

  CSid sid_user;
  if (!token.GetUser(&sid_user))
    return false;
  if (!dacl.AddAllowedAce(sid_user, GENERIC_ALL))
    return false;

  security_desc.SetDacl(dacl);
  sec_attr->Set(security_desc);

  return true;
}

void GetAdminDaclSecurityDescriptor(CSecurityDesc* sd, ACCESS_MASK accessmask) {
  DCHECK(sd);

  CDacl dacl;
  dacl.AddAllowedAce(Sids::System(), accessmask);
  dacl.AddAllowedAce(Sids::Admins(), accessmask);

  sd->SetOwner(Sids::Admins());
  sd->SetGroup(Sids::Admins());
  sd->SetDacl(dacl);
  sd->MakeAbsolute();
}

void GetAdminDaclSecurityAttributes(CSecurityAttributes* sec_attr,
                                    ACCESS_MASK accessmask) {
  DCHECK(sec_attr);
  CSecurityDesc sd;
  GetAdminDaclSecurityDescriptor(&sd, accessmask);
  sec_attr->Set(sd);
}

base::string16 GetRegistryKeyClientsUpdater() {
  return base::ASCIIToUTF16(base::StrCat({CLIENTS_KEY, kUpdaterAppId}));
}

base::string16 GetRegistryKeyClientStateUpdater() {
  return base::ASCIIToUTF16(base::StrCat({CLIENT_STATE_KEY, kUpdaterAppId}));
}

}  // namespace updater
