// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_REG_UTILS_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_REG_UTILS_H_

#include <map>
#include <string>
#include <vector>

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

// Registry key where user device resource ID is stored.
extern const wchar_t kRegUserDeviceResourceId[];

// Specifies custom Chrome path to use for GLS.
extern const wchar_t kRegGlsPath[];

// Mdm registry value key name.

// Enables verbose logging in GCPW.
extern const wchar_t kRegEnableVerboseLogging[];

// Enables logging to a specified file path.
extern const wchar_t kRegLogFilePath[];

// If logging to a file, indicates whether the data should be appended to the
// existing file or not.
extern const wchar_t kRegLogFileAppend[];

// Determines if crash reporting is initialized for credential provider DLL.
extern const wchar_t kRegInitializeCrashReporting[];

// The url used to register the machine to MDM. If specified and non-empty
// additional user access restrictions will be applied to users associated
// to GCPW that have invalid token handles.
extern const wchar_t kRegMdmUrl[];

// The registry entry is used to control whether to enable enrollment
// Google device management solution.
extern const wchar_t kRegEnableDmEnrollment[];

// Disables password escrowing feature in GCPW.
extern const wchar_t kRegDisablePasswordSync[];

// Determines if multiple users can be added to a system managed by MDM.
extern const wchar_t kRegMdmSupportsMultiUser[];

// Allow sign in using normal consumer accounts.
extern const wchar_t kRegMdmAllowConsumerAccounts[];

// Enables force password reset option in forgot password flow.
extern const wchar_t kRegMdmEnableForcePasswordReset[];

// Upload status for device details.
extern const wchar_t kRegDeviceDetailsUploadStatus[];

// Number of consecutive failures encountered when uploading device details.
extern const wchar_t kRegDeviceDetailsUploadFailures[];

// The URL part that is used when constructing the developer complete URL. When
// it is empty, developer mode isn't enabled.
extern const wchar_t kRegDeveloperMode[];

// Enables updating credentials on login UI when the enforcement of any GCPW
// associated account changes.
extern const wchar_t kRegUpdateCredentialsOnChange[];

// Registry key that indicates account name for an unassociated Windows account
// should be in shorter form.
extern const wchar_t kRegUseShorterAccountName[];

// Registry keys where list of domains allowed to login is specified.
extern const wchar_t kEmailDomainsKey[];  // Older deprecated key.
extern const wchar_t kEmailDomainsKeyNew[];

// Registry key where the the last time the policy is refreshed for the user is
// stored.
extern const wchar_t kLastUserPolicyRefreshTimeRegKey[];

// Registry key where the the last time the experiments is refreshed for the
// user is stored.
extern const wchar_t kLastUserExperimentsRefreshTimeRegKey[];

// Gets any HKLM registry key on the system.
HRESULT GetMachineRegDWORD(const std::wstring& key_name,
                           const std::wstring& name,
                           DWORD* value);
HRESULT GetMachineRegString(const std::wstring& key_name,
                            const std::wstring& name,
                            wchar_t* value,
                            ULONG* length);

// Gets global DWORD flag.
HRESULT GetGlobalFlag(const std::wstring& name, DWORD* value);

// Gets global string flag.  Upon calling |length| contains maximum size of
// the |value| buffer in characters.  Upon return |length| contains the length
// of string.  This function guarantees that the string is null terminated, so
// the maximum number of non-null characters returned is |length-1|.
HRESULT GetGlobalFlag(const std::wstring& name, wchar_t* value, ULONG* length);

// Gets global string flag.  Returns |default_value| if no value is set or there
// was an error fetching the flag.
std::wstring GetGlobalFlagOrDefault(const std::wstring& reg_key,
                                    const std::wstring& default_value);

// Gets global DWORD flag.  Returns |default_value| if no value is set or there
// was an error fetching the flag.
DWORD GetGlobalFlagOrDefault(const std::wstring& reg_key,
                             const DWORD& default_value);

// Sets global DWORD flag.
HRESULT SetGlobalFlag(const std::wstring& name, DWORD value);

// Sets the client flag under |kRegUpdaterClientsAppPath|.
HRESULT SetUpdaterClientsAppPathFlag(const std::wstring&, DWORD value);

// Gets client DWORD flag from |kRegUpdaterClientsAppPath|.
HRESULT GetUpdaterClientsAppPathFlag(const std::wstring& name, DWORD* value);

// Gets client DWORD flag under |kRegUpdaterClientsAppPath|.  Returns
// |default_value| if no value is set or there was an error fetching the flag.
DWORD GetUpdaterClientsAppPathFlagOrDefault(const std::wstring& reg_key,
                                            const DWORD& default_value);

// Sets global std::wstring flag.
HRESULT SetGlobalFlag(const std::wstring& name, const std::wstring& value);

// Sets global flag. Used for testing purposes only.
HRESULT SetGlobalFlagForTesting(const std::wstring& name,
                                const std::wstring& value);
HRESULT SetGlobalFlagForTesting(const std::wstring& name, DWORD value);

// Gets DWORD property set for the given user.
HRESULT GetUserProperty(const std::wstring& sid,
                        const std::wstring& name,
                        DWORD* value);

// Gets a string user property.  Upon calling |length| contains maximum size of
// the |value| buffer in characters.  Upon return |length| contains the length
// of string.  This function guarantees that the string is null terminated, so
// the maximum number of non-null characters returned is |length-1|.
HRESULT GetUserProperty(const std::wstring& sid,
                        const std::wstring& name,
                        wchar_t* value,
                        ULONG* length);

// Sets a DWORD user property.
HRESULT SetUserProperty(const std::wstring& sid,
                        const std::wstring& name,
                        DWORD value);

// Sets a string user property.
HRESULT SetUserProperty(const std::wstring& sid,
                        const std::wstring& name,
                        const std::wstring& value);

// Sets the value of a particular user name under the UserList key of WinLogon.
// This value allows hiding the user from all credential provider related
// operations including sign on, unlock, password change and cred ui. This
// function will only set the registry value if visible == 0. Otherwise it will
// delete the registry value so that the default behavior is possible.
HRESULT SetUserWinlogonUserListEntry(const std::wstring& username,
                                     DWORD visible);

// Sets the default credential provider for a user tile.
HRESULT SetLogonUiUserTileEntry(const std::wstring& sid, CLSID cp_guid);

// Removes all properties for the user.
HRESULT RemoveAllUserProperties(const std::wstring& sid);

struct UserTokenHandleInfo {
  std::wstring gaia_id;
  std::wstring email_address;
  std::wstring token_handle;
};

// Gets basic user association info as stored in the registry. For each found
// sid under GCPW's Users registry key, this function will return:
// 1. The gaia id associated to the user (if any).
// 2. The value of the token handle (if any).
// This function does not provide any guarantee as to the validity of the
// information returned w.r.t. actual users that exist on the system.
HRESULT GetUserTokenHandles(
    std::map<std::wstring, UserTokenHandleInfo>* sid_to_handle_info);

// Gets the SID associated with the given gaia id.  If none exists, returns
// HRESULT_FROM_WIN32(ERROR_NONE_MAPPED).
HRESULT GetSidFromId(const std::wstring& id, wchar_t* sid, ULONG length);

// Gets the SID associated with the given email.  If none exists, returns
// HRESULT_FROM_WIN32(ERROR_NONE_MAPPED).
HRESULT GetSidFromEmail(const std::wstring& email, wchar_t* sid, ULONG length);

// Gets the SID associated with the given input key. If none exists, returns
// HRESULT_FROM_WIN32(ERROR_NONE_MAPPED).
HRESULT GetSidFromKey(const wchar_t* key,
                      const std::wstring& value,
                      wchar_t* sid,
                      ULONG length);

// Returns the SID corresponding to given domain user from the user properties.
HRESULT GetSidFromDomainAccountInfo(const std::wstring& domain,
                                    const std::wstring& username,
                                    wchar_t* sid,
                                    ULONG length);

// Gets the gaia id associated with the given SID.  If none exists, returns
// HRESULT_FROM_WIN32(ERROR_NONE_MAPPED).
HRESULT GetIdFromSid(const wchar_t* sid, std::wstring* id);

// Get the email ID associated with the user with |sid|.
std::string GetUserEmailFromSid(const std::wstring& sid);

// Get children at a reg path.
void GetChildrenAtPath(const wchar_t* path,
                       std::vector<std::wstring>& children);

// Gets a specific account picture registry key in HKEY_LOCAL_MACHINE
HRESULT GetAccountPictureRegString(const std::wstring& user_sid,
                                   int image_size,
                                   wchar_t* value,
                                   ULONG* length);

// Sets a specific account picture registry key in HKEY_LOCAL_MACHINE
HRESULT SetAccountPictureRegString(const std::wstring& user_sid,
                                   int image_size,
                                   const std::wstring& value);

// Retrieves an identifier that is stored under
// HKLM\SOFTWARE\Microsoft\Cryptography\MachineGuid registry.
HRESULT GetMachineGuid(std::wstring* machine_guid);

// Sets  HKLM\SOFTWARE\Microsoft\Cryptography\MachineGuid registry for testing.
HRESULT SetMachineGuidForTesting(const std::wstring& machine_guid);

// Sets String value at registry path key_name/name.
HRESULT SetMachineRegString(const std::wstring& key_name,
                            const std::wstring& name,
                            const std::wstring& value);

// Sets DWORD value at registry path key_name/name.
HRESULT SetMachineRegDWORD(const std::wstring& key_name,
                           const std::wstring& name,
                           DWORD value);

// Set corresponding registry entry that would make GCPW as the default
// credential provider.
HRESULT MakeGcpwDefaultCP();

// Get device resource ID for the user with given |sid|. Returns an empty string
// if one has not been set for the user.
std::wstring GetUserDeviceResourceId(const std::wstring& sid);

// The token which is written to Windows registry as a result of exchanging
// enrollment token. The value returned here is the Base64 encoded version of
// the binary value present in the registry.
HRESULT GetDmToken(std::string* dm_token);

// Sets HKLM\SOFTWARE\Google\Enrollment\dm_token registry for testing. Note
// here that the value specified in |dm_token| will be the binary value stored
// in registry. The value read in GetDmToken() is the Base64 encoded version of
// this.
HRESULT SetDmTokenForTesting(const std::string& dm_token);

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_REG_UTILS_H_
