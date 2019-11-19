// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_REG_UTILS_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_REG_UTILS_H_

#include <map>
#include <vector>

#include "base/strings/string16.h"
#include "base/win/windows_types.h"

namespace credential_provider {

// Root registry key where all settings and user information for GCPW is stored.
extern const wchar_t kGcpRootKeyName[];

// Root registry key where all user association information for GCPW is stored.
extern const wchar_t kGcpUsersRootKeyName[];

// User list registry key that Windows reads to disable users from credential
// providers.
extern const wchar_t kWinlogonUserListRegKey[];

// Registry key used to determine a user's default credential provider tile.
extern const wchar_t kLogonUiUserTileRegKey[];

// Gets any HKLM registry key on the system.
HRESULT GetMachineRegDWORD(const base::string16& key_name,
                           const base::string16& name,
                           DWORD* value);
HRESULT GetMachineRegString(const base::string16& key_name,
                            const base::string16& name,
                            wchar_t* value,
                            ULONG* length);

// Gets global DWORD flag.
HRESULT GetGlobalFlag(const base::string16& name, DWORD* value);

// Gets global string flag.  Upon calling |length| contains maximum size of
// the |value| buffer in characters.  Upon return |length| contains the length
// of string.  This function guarantees that the string is null terminated, so
// the maximum number of non-null characters returned is |length-1|.
HRESULT GetGlobalFlag(const base::string16& name,
                      wchar_t* value,
                      ULONG* length);

// Gets global string flag.  Returns |default_value| if no value is set or there
// was an error fetching the flag.
base::string16 GetGlobalFlagOrDefault(const base::string16& reg_key,
                                      const base::string16& default_value);

// Gets global DWORD flag.  Returns |default_value| if no value is set or there
// was an error fetching the flag.
DWORD GetGlobalFlagOrDefault(const base::string16& reg_key,
                             const DWORD& default_value);

// Sets global flag. Used for testing purposes only.
HRESULT SetGlobalFlagForTesting(const base::string16& name,
                                const base::string16& value);
HRESULT SetGlobalFlagForTesting(const base::string16& name, DWORD value);

// Gets DWORD property set for the given user.
HRESULT GetUserProperty(const base::string16& sid,
                        const base::string16& name,
                        DWORD* value);

// Gets a string user property.  Upon calling |length| contains maximum size of
// the |value| buffer in characters.  Upon return |length| contains the length
// of string.  This function guarantees that the string is null terminated, so
// the maximum number of non-null characters returned is |length-1|.
HRESULT GetUserProperty(const base::string16& sid,
                        const base::string16& name,
                        wchar_t* value,
                        ULONG* length);

// Sets a DWORD user property.
HRESULT SetUserProperty(const base::string16& sid,
                        const base::string16& name,
                        DWORD value);

// Sets a string user property.
HRESULT SetUserProperty(const base::string16& sid,
                        const base::string16& name,
                        const base::string16& value);

// Sets the value of a particular user name under the UserList key of WinLogon.
// This value allows hiding the user from all credential provider related
// operations including sign on, unlock, password change and cred ui. This
// function will only set the registry value if visible == 0. Otherwise it will
// delete the registry value so that the default behavior is possible.
HRESULT SetUserWinlogonUserListEntry(const base::string16& username,
                                     DWORD visible);

// Sets the default credential provider for a user tile.
HRESULT SetLogonUiUserTileEntry(const base::string16& sid, CLSID cp_guid);

// Removes all properties for the user.
HRESULT RemoveAllUserProperties(const base::string16& sid);

struct UserTokenHandleInfo {
  base::string16 gaia_id;
  base::string16 token_handle;
};

// Gets basic user association info as stored in the registry. For each found
// sid under GCPW's Users registry key, this function will return:
// 1. The gaia id associated to the user (if any).
// 2. The value of the token handle (if any).
// This function does not provide any guarantee as to the validity of the
// information returned w.r.t. actual users that exist on the system.
HRESULT GetUserTokenHandles(
    std::map<base::string16, UserTokenHandleInfo>* sid_to_handle_info);

// Gets the SID associated with the given gaia id.  If none exists, returns
// HRESULT_FROM_WIN32(ERROR_NONE_MAPPED).
HRESULT GetSidFromId(const base::string16& id, wchar_t* sid, ULONG length);

// Gets the gaia id associated with the given SID.  If none exists, returns
// HRESULT_FROM_WIN32(ERROR_NONE_MAPPED).
HRESULT GetIdFromSid(const wchar_t* sid, base::string16* id);

// Gets a specific account picture registry key in HKEY_LOCAL_MACHINE
HRESULT GetAccountPictureRegString(const base::string16& user_sid,
                                   int image_size,
                                   wchar_t* value,
                                   ULONG* length);

// Sets a specific account picture registry key in HKEY_LOCAL_MACHINE
HRESULT SetAccountPictureRegString(const base::string16& user_sid,
                                   int image_size,
                                   const base::string16& value);

// Retrieves an identifier that is stored under
// HKLM\SOFTWARE\Microsoft\Cryptography\MachineGuid registry.
HRESULT GetMachineGuid(base::string16* machine_guid);

// Sets  HKLM\SOFTWARE\Microsoft\Cryptography\MachineGuid registry for testing.
HRESULT SetMachineGuidForTesting(const base::string16& machine_guid);

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_REG_UTILS_H_
