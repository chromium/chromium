// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/credential_provider/gaiacp/reg_utils.h"

#include "base/base64.h"
#include "base/strings/strcat_win.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/atl.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "build/branding_buildflags.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/logging.h"

namespace credential_provider {

// Root registry key for GCPW configuration and state.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define CREDENTIAL_PROVIDER_REGISTRY_KEY L"Software\\Google\\GCPW"
#else
#define CREDENTIAL_PROVIDER_REGISTRY_KEY L"Software\\Chromium\\GCPW"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

const wchar_t kGcpRootKeyName[] = CREDENTIAL_PROVIDER_REGISTRY_KEY;

const wchar_t kGcpUsersRootKeyName[] =
    CREDENTIAL_PROVIDER_REGISTRY_KEY L"\\Users";

const wchar_t kWinlogonUserListRegKey[] =
    L"SOFTWARE\\Microsoft\\Windows NT"
    L"\\CurrentVersion\\Winlogon\\SpecialAccounts\\UserList";

const wchar_t kLogonUiUserTileRegKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Authentication\\LogonUI"
    L"\\UserTile";

const wchar_t kMicrosoftCryptographyRegKey[] =
    L"SOFTWARE\\Microsoft\\Cryptography";
const wchar_t kMicrosoftCryptographyMachineGuidRegKey[] = L"MachineGuid";

constexpr wchar_t kRegUserDeviceResourceId[] = L"device_resource_id";
constexpr wchar_t kRegGlsPath[] = L"gls_path";
constexpr wchar_t kRegEnableVerboseLogging[] = L"enable_verbose_logging";
constexpr wchar_t kRegLogFilePath[] = L"log_file_path";
constexpr wchar_t kRegLogFileAppend[] = L"log_file_append";
constexpr wchar_t kRegInitializeCrashReporting[] = L"enable_crash_reporting";
constexpr wchar_t kRegMdmUrl[] = L"mdm";
constexpr wchar_t kRegEnableDmEnrollment[] = L"enable_dm_enrollment";
constexpr wchar_t kRegDisablePasswordSync[] = L"disable_password_sync";
constexpr wchar_t kRegMdmSupportsMultiUser[] = L"enable_multi_user_login";
constexpr wchar_t kRegMdmAllowConsumerAccounts[] = L"enable_consumer_accounts ";
constexpr wchar_t kRegMdmEnableForcePasswordReset[] =
    L"enable_force_reset_password_option";
constexpr wchar_t kRegDeviceDetailsUploadStatus[] =
    L"device_details_upload_status";
constexpr wchar_t kRegDeviceDetailsUploadFailures[] =
    L"device_details_upload_failures";
constexpr wchar_t kRegDeveloperMode[] = L"developer_mode";
constexpr wchar_t kRegUpdateCredentialsOnChange[] =
    L"update_credentials_on_change";
constexpr wchar_t kRegUseShorterAccountName[] = L"use_shorter_account_name";
constexpr wchar_t kEmailDomainsKey[] = L"ed";  // deprecated.
constexpr wchar_t kEmailDomainsKeyNew[] = L"domains_allowed_to_login";

const wchar_t kLastUserPolicyRefreshTimeRegKey[] = L"last_policy_refresh_time";
const wchar_t kLastUserExperimentsRefreshTimeRegKey[] =
    L"last_experiments_refresh_time";

namespace {

constexpr wchar_t kAccountPicturesRootRegKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AccountPicture\\Users";
constexpr wchar_t kImageRegKey[] = L"Image";

// Registry entry that controls whether GCPW is the default
// Credential Provider or not.
constexpr wchar_t kMakeGcpwDefaultCredProvider[] = L"set_gcpw_as_default_cp";
// Windows OS defined registry entry used to configure the
// default credential provider CLSID.
constexpr wchar_t kDefaultCredProviderPath[] =
    L"Software\\Policies\\Microsoft\\Windows\\System";
constexpr wchar_t kDefaultCredProviderKey[] = L"DefaultCredentialProvider";

constexpr wchar_t kEnrollmentRegKey[] = L"SOFTWARE\\Google\\Enrollment";
constexpr wchar_t kDmTokenRegKey[] = L"dmtoken";

HRESULT SetMachineRegBinaryInternal(const std::wstring& key_name,
                                    const std::wstring& name,
                                    const std::string& value,
                                    REGSAM sam_desired) {
  base::win::RegKey key;
  LONG sts = key.Create(HKEY_LOCAL_MACHINE, key_name.c_str(), sam_desired);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  if (value.empty()) {
    sts = key.DeleteValue(name.c_str());
    if (sts == ERROR_FILE_NOT_FOUND)
      sts = ERROR_SUCCESS;
  } else {
    sts =
        key.WriteValue(name.c_str(), value.c_str(), value.length(), REG_BINARY);
  }

  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  return S_OK;
}

std::wstring GetImageRegKeyForSpecificSize(int image_size) {
  return kImageRegKey + base::NumberToWString(image_size);
}

std::wstring GetAccountPictureRegPathForUSer(const std::wstring& user_sid) {
  return base::StrCat({kAccountPicturesRootRegKey, L"\\", user_sid});
}

}  // namespace

HRESULT SetMachineRegDWORD(const std::wstring& key_name,
                           const std::wstring& name,
                           DWORD value) {
  base::win::RegKey key;
  LONG sts = key.Create(HKEY_LOCAL_MACHINE, key_name.c_str(), KEY_WRITE);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  sts = key.WriteValue(name.c_str(), value);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  return S_OK;
}

HRESULT MakeGcpwDefaultCP() {
  if (GetGlobalFlagOrDefault(kMakeGcpwDefaultCredProvider, 1))
    return SetMachineRegString(
        kDefaultCredProviderPath, kDefaultCredProviderKey,
        base::win::WStringFromGUID(CLSID_GaiaCredentialProvider));

  return S_OK;
}

HRESULT SetMachineRegString(const std::wstring& key_name,
                            const std::wstring& name,
                            const std::wstring& value) {
  base::win::RegKey key;
  LONG sts = key.Create(HKEY_LOCAL_MACHINE, key_name.c_str(), KEY_WRITE);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  if (value.empty()) {
    sts = key.DeleteValue(name.c_str());
    if (sts == ERROR_FILE_NOT_FOUND)
      sts = ERROR_SUCCESS;
  } else {
    sts = key.WriteValue(name.c_str(), value.c_str());
  }

  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  return S_OK;
}

HRESULT GetMachineRegDWORD(const std::wstring& key_name,
                           const std::wstring& name,
                           DWORD* value) {
  base::win::RegKey key;
  LONG sts = key.Open(HKEY_LOCAL_MACHINE, key_name.c_str(), KEY_READ);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  sts = key.ReadValueDW(name.c_str(), value);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  return S_OK;
}

HRESULT GetMachineRegString(const std::wstring& key_name,
                            const std::wstring& name,
                            wchar_t* value,
                            ULONG* length) {
  DCHECK(value);
  DCHECK(length);
  DCHECK_GT(*length, 0u);

  base::win::RegKey key;
  LONG sts = key.Open(HKEY_LOCAL_MACHINE, key_name.c_str(), KEY_READ);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  // read one less character that specified in |length| so that the returned
  // string can always be null terminated.  Note that string registry values
  // are not guaranteed to be null terminated.
  DWORD type;
  ULONG local_length = (*length - 1) * sizeof(decltype(value[0]));
  sts = key.ReadValue(name.c_str(), value, &local_length, &type);
  if (type != REG_SZ)
    return HRESULT_FROM_WIN32(ERROR_CANTREAD);

  // When using this overload of the ReadValue() method, the returned length
  // is in bytes.  The caller expects the length in characters.
  local_length /= sizeof(decltype(value[0]));

  if (sts != ERROR_SUCCESS) {
    if (sts == ERROR_MORE_DATA)
      *length = local_length;
    return HRESULT_FROM_WIN32(sts);
  }

  value[local_length] = 0;
  *length = local_length;
  return S_OK;
}

HRESULT GetMachineRegBinaryInternal(const std::wstring& key_name,
                                    const std::wstring& name,
                                    std::string* val,
                                    REGSAM sam_desired) {
  DCHECK(val);

  base::win::RegKey key;
  LONG sts = key.Open(HKEY_LOCAL_MACHINE, key_name.c_str(), sam_desired);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  DWORD type;
  DWORD size = 0;

  sts = key.ReadValue(name.c_str(), nullptr, &size, &type);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  if (type != REG_BINARY)
    return HRESULT_FROM_WIN32(ERROR_CANTREAD);

  std::vector<char> buffer(size);

  sts = key.ReadValue(name.c_str(), const_cast<char*>(buffer.data()), &size,
                      &type);
  if (sts != ERROR_SUCCESS) {
    if (sts == ERROR_MORE_DATA)
      return HRESULT_FROM_WIN32(sts);
  }

  val->assign(buffer.data(), buffer.size());

  return S_OK;
}

HRESULT GetAccountPictureRegString(const std::wstring& user_sid,
                                   int image_size,
                                   wchar_t* value,
                                   ULONG* length) {
  return GetMachineRegString(GetAccountPictureRegPathForUSer(user_sid),
                             GetImageRegKeyForSpecificSize(image_size), value,
                             length);
}

// Sets a specific account picture registry key in HKEY_LOCAL_MACHINE
HRESULT SetAccountPictureRegString(const std::wstring& user_sid,
                                   int image_size,
                                   const std::wstring& value) {
  return SetMachineRegString(GetAccountPictureRegPathForUSer(user_sid),
                             GetImageRegKeyForSpecificSize(image_size), value);
}

HRESULT GetGlobalFlag(const std::wstring& name, DWORD* value) {
  return GetMachineRegDWORD(kGcpRootKeyName, name, value);
}

HRESULT GetGlobalFlag(const std::wstring& name, wchar_t* value, ULONG* length) {
  return GetMachineRegString(kGcpRootKeyName, name, value, length);
}

std::wstring GetGlobalFlagOrDefault(const std::wstring& reg_key,
                                    const std::wstring& default_value) {
  wchar_t reg_value_buffer[256];
  ULONG length = std::size(reg_value_buffer);
  HRESULT hr = GetGlobalFlag(reg_key, reg_value_buffer, &length);
  if (FAILED(hr))
    return default_value;

  return reg_value_buffer;
}

DWORD GetGlobalFlagOrDefault(const std::wstring& reg_key,
                             const DWORD& default_value) {
  DWORD value;
  HRESULT hr = GetGlobalFlag(reg_key, &value);
  return SUCCEEDED(hr) ? value : default_value;
}

HRESULT SetGlobalFlag(const std::wstring& name, DWORD value) {
  return SetMachineRegDWORD(kGcpRootKeyName, name, value);
}

HRESULT SetGlobalFlag(const std::wstring& name, const std::wstring& value) {
  return SetMachineRegString(kGcpRootKeyName, name, value);
}

HRESULT SetGlobalFlagForTesting(const std::wstring& name,
                                const std::wstring& value) {
  return SetMachineRegString(kGcpRootKeyName, name, value);
}

HRESULT SetGlobalFlagForTesting(const std::wstring& name, DWORD value) {
  return SetMachineRegDWORD(kGcpRootKeyName, name, value);
}

HRESULT SetUpdaterClientsAppPathFlag(const std::wstring& name, DWORD value) {
  base::win::RegKey key;
  LONG sts = key.Create(HKEY_LOCAL_MACHINE, kRegUpdaterClientsAppPath,
                        KEY_WRITE | KEY_WOW64_32KEY);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  sts = key.WriteValue(name.c_str(), value);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  return S_OK;
}

HRESULT GetUpdaterClientsAppPathFlag(const std::wstring& name, DWORD* value) {
  base::win::RegKey key;
  LONG sts = key.Open(HKEY_LOCAL_MACHINE, kRegUpdaterClientsAppPath,
                      KEY_READ | KEY_WOW64_32KEY);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  sts = key.ReadValueDW(name.c_str(), value);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  return S_OK;
}

DWORD GetUpdaterClientsAppPathFlagOrDefault(const std::wstring& reg_key,
                                            const DWORD& default_value) {
  DWORD value;
  HRESULT hr = GetUpdaterClientsAppPathFlag(reg_key, &value);
  return SUCCEEDED(hr) ? value : default_value;
}

HRESULT GetUserProperty(const std::wstring& sid,
                        const std::wstring& name,
                        DWORD* value) {
  wchar_t key_name[128];
  swprintf_s(key_name, std::size(key_name), L"%s\\%s", kGcpUsersRootKeyName,
             sid.c_str());
  return GetMachineRegDWORD(key_name, name, value);
}

HRESULT GetUserProperty(const std::wstring& sid,
                        const std::wstring& name,
                        wchar_t* value,
                        ULONG* length) {
  wchar_t key_name[128];
  swprintf_s(key_name, std::size(key_name), L"%s\\%s", kGcpUsersRootKeyName,
             sid.c_str());
  return GetMachineRegString(key_name, name, value, length);
}

HRESULT SetUserProperty(const std::wstring& sid,
                        const std::wstring& name,
                        DWORD value) {
  wchar_t key_name[128];
  swprintf_s(key_name, std::size(key_name), L"%s\\%s", kGcpUsersRootKeyName,
             sid.c_str());
  return SetMachineRegDWORD(key_name, name, value);
}

HRESULT SetUserProperty(const std::wstring& sid,
                        const std::wstring& name,
                        const std::wstring& value) {
  wchar_t key_name[128];
  swprintf_s(key_name, std::size(key_name), L"%s\\%s", kGcpUsersRootKeyName,
             sid.c_str());
  return SetMachineRegString(key_name, name, value);
}

HRESULT RemoveAllUserProperties(const std::wstring& sid) {
  base::win::RegKey key;
  LONG sts = key.Open(HKEY_LOCAL_MACHINE, kGcpUsersRootKeyName, KEY_WRITE);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  sts = key.DeleteKey(sid.c_str());
  return sts != ERROR_SUCCESS ? HRESULT_FROM_WIN32(sts) : S_OK;
}

HRESULT GetUserTokenHandles(
    std::map<std::wstring, UserTokenHandleInfo>* sid_to_handle_info) {
  DCHECK(sid_to_handle_info);
  sid_to_handle_info->clear();

  base::win::RegistryKeyIterator iter(HKEY_LOCAL_MACHINE, kGcpUsersRootKeyName);
  for (; iter.Valid(); ++iter) {
    const wchar_t* sid = iter.Name();
    wchar_t gaia_id[256];
    ULONG length = std::size(gaia_id);
    HRESULT gaia_id_hr = GetUserProperty(sid, kUserId, gaia_id, &length);
    wchar_t token_handle[256];
    length = std::size(token_handle);
    HRESULT token_handle_hr =
        GetUserProperty(sid, kUserTokenHandle, token_handle, &length);
    wchar_t email_address[256];
    length = std::size(email_address);
    HRESULT email_address_hr =
        GetUserProperty(sid, kUserEmail, email_address, &length);
    sid_to_handle_info->emplace(
        sid,
        UserTokenHandleInfo{SUCCEEDED(gaia_id_hr) ? gaia_id : L"",
                            SUCCEEDED(email_address_hr) ? email_address : L"",
                            SUCCEEDED(token_handle_hr) ? token_handle : L""});
  }
  return S_OK;
}

HRESULT GetSidFromKey(const wchar_t* key,
                      const std::wstring& value,
                      wchar_t* sid,
                      ULONG length) {
  DCHECK(sid);
  bool result_found = false;
  base::win::RegistryKeyIterator iter(HKEY_LOCAL_MACHINE, kGcpUsersRootKeyName);
  for (; iter.Valid(); ++iter) {
    const wchar_t* user_sid = iter.Name();
    wchar_t result[256];
    ULONG result_length = std::size(result);
    HRESULT hr = GetUserProperty(user_sid, key, result, &result_length);
    if (SUCCEEDED(hr) && value == result) {
      // Make sure there are not 2 users with the same SID.
      if (result_found)
        return HRESULT_FROM_WIN32(ERROR_USER_EXISTS);

      wcsncpy_s(sid, length, user_sid, wcslen(user_sid));
      result_found = true;
    }
  }

  return result_found ? S_OK : HRESULT_FROM_WIN32(ERROR_NONE_MAPPED);
}

HRESULT GetSidFromEmail(const std::wstring& email, wchar_t* sid, ULONG length) {
  return GetSidFromKey(kUserEmail, email, sid, length);
}

HRESULT GetSidFromId(const std::wstring& id, wchar_t* sid, ULONG length) {
  return GetSidFromKey(kUserId, id, sid, length);
}

HRESULT GetSidFromDomainAccountInfo(const std::wstring& domain,
                                    const std::wstring& username,
                                    wchar_t* sid,
                                    ULONG length) {
  // Max SID length is 256 characters.
  // https://docs.microsoft.com/en-us/windows-hardware/customize/desktop/unattend/microsoft-windows-shell-setup-offlineuseraccounts-offlinedomainaccounts-offlinedomainaccount-sid
  wchar_t sid1[256];
  wchar_t sid2[256];

  if (SUCCEEDED(GetSidFromKey(base::UTF8ToWide(kKeyDomain).c_str(), domain,
                              sid1, length)) &&
      SUCCEEDED(GetSidFromKey(base::UTF8ToWide(kKeyUsername).c_str(), username,
                              sid2, length)) &&
      wcsicmp(sid1, sid2) == 0) {
    wcscpy_s(sid, length, sid1);
    return S_OK;
  } else {
    return E_FAIL;
  }
}

HRESULT GetIdFromSid(const wchar_t* sid, std::wstring* id) {
  DCHECK(id);

  base::win::RegistryKeyIterator iter(HKEY_LOCAL_MACHINE, kGcpUsersRootKeyName);
  for (; iter.Valid(); ++iter) {
    const wchar_t* user_sid = iter.Name();

    if (wcscmp(sid, user_sid) == 0) {
      wchar_t user_id[256];
      ULONG user_length = std::size(user_id);
      HRESULT hr = GetUserProperty(user_sid, kUserId, user_id, &user_length);
      if (SUCCEEDED(hr)) {
        *id = user_id;
        return S_OK;
      }
    }
  }
  return HRESULT_FROM_WIN32(ERROR_NONE_MAPPED);
}

std::string GetUserEmailFromSid(const std::wstring& sid) {
  wchar_t email_id[512];
  ULONG email_id_size = std::size(email_id);
  HRESULT hr = GetUserProperty(sid, kUserEmail, email_id, &email_id_size);

  std::wstring email_id_str;
  if (SUCCEEDED(hr) && email_id_size > 0)
    email_id_str = std::wstring(email_id, email_id_size - 1);

  return base::WideToUTF8(email_id_str);
}

void GetChildrenAtPath(const wchar_t* path,
                       std::vector<std::wstring>& children) {
  base::win::RegistryKeyIterator iter(HKEY_LOCAL_MACHINE, path);
  for (; iter.Valid(); ++iter) {
    const wchar_t* child = iter.Name();
    children.push_back(std::wstring(child));
  }
}

HRESULT SetUserWinlogonUserListEntry(const std::wstring& username,
                                     DWORD visible) {
  // Sets the value of the key that will hide the user from all credential
  // providers.
  base::win::RegKey key;
  LONG sts = key.Create(HKEY_LOCAL_MACHINE, kWinlogonUserListRegKey, KEY_WRITE);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  // The key is only set if we want to make a user invisible. The default
  // behavior is for the user to be visible so delete the key in this case.
  if (!visible) {
    sts = key.WriteValue(username.c_str(), visible);
  } else {
    sts = key.DeleteValue(username.c_str());
    if (sts == ERROR_FILE_NOT_FOUND)
      sts = ERROR_SUCCESS;
  }

  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  return S_OK;
}

HRESULT SetLogonUiUserTileEntry(const std::wstring& sid, CLSID cp_guid) {
  return SetMachineRegString(kLogonUiUserTileRegKey, sid,
                             base::win::WStringFromGUID(cp_guid));
}

HRESULT GetMachineGuid(std::wstring* machine_guid) {
  // The machine guid is a unique identifier assigned to a computer on every
  // install of Windows. This guid can be used to uniquely identify this device
  // to various management services. The same guid is used to identify the
  // device to Chrome Browser Cloud Management. It is fetched in this file:
  // chrome/browser/policy/browser_dm_token_storage_win.cc:InitClientId.
  DCHECK(machine_guid);
  wchar_t machine_guid_buffer[64];
  ULONG guid_length = std::size(machine_guid_buffer);
  HRESULT hr = GetMachineRegString(kMicrosoftCryptographyRegKey,
                                   kMicrosoftCryptographyMachineGuidRegKey,
                                   machine_guid_buffer, &guid_length);

  if (SUCCEEDED(hr))
    *machine_guid = machine_guid_buffer;

  return hr;
}

HRESULT SetMachineGuidForTesting(const std::wstring& machine_guid) {
  // Set a debug guid for the machine so that unit tests that override the
  // registry can run properly.
  return SetMachineRegString(kMicrosoftCryptographyRegKey,
                             kMicrosoftCryptographyMachineGuidRegKey,
                             machine_guid);
}

std::wstring GetUserDeviceResourceId(const std::wstring& sid) {
  wchar_t known_resource_id[512];
  ULONG known_resource_id_size = std::size(known_resource_id);
  HRESULT hr = GetUserProperty(sid, kRegUserDeviceResourceId, known_resource_id,
                               &known_resource_id_size);

  if (SUCCEEDED(hr) && known_resource_id_size > 0)
    return std::wstring(known_resource_id, known_resource_id_size - 1);

  return std::wstring();
}

HRESULT GetDmToken(std::string* dm_token) {
  DCHECK(dm_token);

  std::string binary_dm_token;
  HRESULT hr =
      GetMachineRegBinaryInternal(kEnrollmentRegKey, kDmTokenRegKey,
                                  &binary_dm_token, KEY_READ | KEY_WOW64_32KEY);
  if (SUCCEEDED(hr)) {
    *dm_token = base::Base64Encode(binary_dm_token);
  }
  return hr;
}

HRESULT SetDmTokenForTesting(const std::string& dm_token) {
  // Set a debug dm token for the machine so that unit tests that override the
  // registry can run properly.
  return SetMachineRegBinaryInternal(kEnrollmentRegKey, kDmTokenRegKey,
                                     dm_token, KEY_WRITE | KEY_WOW64_32KEY);
}

}  // namespace credential_provider
