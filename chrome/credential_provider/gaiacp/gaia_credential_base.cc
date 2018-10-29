// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of CGaiaCredentialBase class, used as the base for all
// credentials that need to show the gaia sign in page.

#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"

#include <algorithm>
#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/current_module.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/os_process_manager.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/scoped_lsa_policy.h"
#include "chrome/credential_provider/gaiacp/scoped_user_profile.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/google_api_keys.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace credential_provider {

namespace {

// Choose a suitable username for the given gaia account.  If a username has
// already been created for this gaia account, the same username is returned.
// Otherwise a new one is generated, derived from the email.
void MakeUsernameForAccount(const base::DictionaryValue* result,
                            wchar_t* username,
                            DWORD length) {
  // First try to detect if this gaia account has been used to create an OS
  // user already.  If so, return the OS username of that user.
  wchar_t sid[128];
  HRESULT hr =
      GetSidFromId(GetDictString(result, kKeyId), sid, base::size(sid));
  if (SUCCEEDED(hr)) {
    hr = OSUserManager::Get()->FindUserBySID(sid, username, length);
    if (SUCCEEDED(hr))
      return;

    LOGFN(INFO) << "FindUserBySID hr=" << putHR(hr);
  } else {
    LOGFN(INFO) << "GetSidFromId: id not found";
  }

  // Create a username based on the email address.  Usernames are more
  // restrictive than emails, so some transformations are needed.  This tries
  // to preserve the email as much as possible in the username while respecting
  // Windows username rules.  See remarks in
  // https://docs.microsoft.com/en-us/windows/desktop/api/lmaccess/ns-lmaccess-_user_info_0
  base::string16 os_username = GetDictString(result, kKeyEmail);
  std::transform(os_username.begin(), os_username.end(), os_username.begin(),
                 ::tolower);

  // If the email ends with @gmail.com or @googlemail.com, strip it.
  base::string16::size_type at = os_username.find(L"@gmail.com");
  if (at == base::string16::npos)
    at = os_username.find(L"@googlemail.com");
  if (at != base::string16::npos) {
    os_username.resize(at);
  } else {
    // Strip off well known TLDs.
    std::string username_utf8 =
        gaia::SanitizeEmail(base::UTF16ToUTF8(os_username));

    size_t tld_length =
        net::registry_controlled_domains::GetCanonicalHostRegistryLength(
            gaia::ExtractDomainName(username_utf8),
            net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

    // If an TLD is found strip it off, plus 1 to remove the separating dot too.
    if (tld_length > 0) {
      username_utf8.resize(username_utf8.length() - tld_length - 1);
      os_username = base::UTF8ToUTF16(username_utf8);
    }
  }

  // If the username is longer than 20 characters, truncate.
  if (os_username.size() > 20)
    os_username.resize(20);

  // Replace invalid characters.  While @ is not strictly invalid according to
  // MSDN docs, it causes trouble.
  for (auto& c : os_username) {
    if (wcschr(L"@\\[]:|<>+=;?*", c) != nullptr || c < 32)
      c = L'_';
  }

  wcscpy_s(username, length, os_username.c_str());
}

// Waits for the login UI to completes and returns the result of the operation.
// This function returns S_OK on success, E_UNEXPECTED on failure, and E_ABORT
// if the user aborted or timed out (or was killed during cleanup).
HRESULT WaitForLoginUIAndGetResult(
    CGaiaCredentialBase::UIProcessInfo* uiprocinfo,
    std::unique_ptr<base::DictionaryValue>* result,
    BSTR* status_text) {
  LOGFN(INFO);
  DCHECK(uiprocinfo);
  DCHECK(result);
  DCHECK(status_text);

  // Buffers used to accumulate output from UI.
  const int kBufferSize = 4096;
  static char stdout_buffer[kBufferSize];
  static char stderr_buffer[kBufferSize];

  DWORD exit_code;
  HRESULT hr = WaitForProcess(uiprocinfo->procinfo.process_handle(),
                              uiprocinfo->parent_handles, &exit_code,
                              stdout_buffer, stderr_buffer, kBufferSize);
  // stdout contains sensitive information like the password.  Don't log it.
  LOGFN(INFO) << "exit_code=" << exit_code
              << " stderr: " << stderr_buffer;

  // If the UI process did not complete successfully, nothing more to do.
  if (exit_code == kUiecEMailMissmatch) {
    LOGFN(ERROR) << "WaitForProcess hr=" << putHR(hr);
    *status_text = CGaiaCredentialBase::AllocErrorString(IDS_EMAIL_MISMATCH);
    return E_UNEXPECTED;
  } else if (exit_code != kUiecSuccess) {
    return E_ABORT;
  }

  std::unique_ptr<base::Value> parsed(
      base::JSONReader::Read(stdout_buffer, base::JSON_ALLOW_TRAILING_COMMAS));
  if (!parsed || !parsed->is_dict()) {
    LOGFN(ERROR) << "Could not parse data from logon UI";
    *status_text =
        CGaiaCredentialBase::AllocErrorString(IDS_INVALID_UI_RESPONSE);
    return E_UNEXPECTED;
  }

  *result = base::DictionaryValue::From(std::move(parsed));
  return S_OK;
}

// This function validates the response from GLS and makes sure it contained
// all the fields required to proceed with logon.  This does not necessarily
// guarantee that the logon will succeed, only that GLS response seems correct.
HRESULT ValidateAndFixResult(base::DictionaryValue* result, BSTR* status_text) {
  DCHECK(result);
  DCHECK(status_text);

  // Check that the webui returned all expected values.

  bool has_error = false;
  std::string email = GetDictStringUTF8(result, kKeyEmail);
  if (email.empty()) {
    LOGFN(ERROR) << "Email is empty";
    has_error = true;
  }

  std::string fullname = GetDictStringUTF8(result, kKeyFullname);
  if (fullname.empty()) {
    LOGFN(ERROR) << "Full name is empty";
    has_error = true;
  }

  std::string id = GetDictStringUTF8(result, kKeyId);
  if (id.empty()) {
    LOGFN(ERROR) << "Id is empty";
    has_error = true;
  }

  std::string mdm_id_token = GetDictStringUTF8(result, kKeyMdmIdToken);
  if (mdm_id_token.empty()) {
    LOGFN(ERROR) << "mdm id token is empty";
    has_error = true;
  }

  std::string password = GetDictStringUTF8(result, kKeyPassword);
  if (password.empty()) {
    LOGFN(ERROR) << "Password is empty";
    has_error = true;
  }

  std::string refresh_token = GetDictStringUTF8(result, kKeyRefreshToken);
  if (refresh_token.empty()) {
    LOGFN(ERROR) << "refresh token is empty";
    has_error = true;
  }

  std::string token_handle = GetDictStringUTF8(result, kKeyTokenHandle);
  if (token_handle.empty()) {
    LOGFN(ERROR) << "Token handle is empty";
    has_error = true;
  }

  if (has_error) {
    *status_text =
        CGaiaCredentialBase::AllocErrorString(IDS_INVALID_UI_RESPONSE);
    return E_UNEXPECTED;
  }

  // Windows supports a maximum of 20 characters plus null in username.
  wchar_t username[21];
  MakeUsernameForAccount(result, username, base::size(username));
  result->SetString(kKeyUsername, username);
  return S_OK;
}

HRESULT BuildCredPackAuthenticationBuffer(
    BSTR username,
    BSTR password,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* cpcs) {
  DCHECK(username);
  DCHECK(password);
  DCHECK(cpcs);

  HRESULT hr = GetAuthenticationPackageId(&(cpcs->ulAuthenticationPackage));
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetAuthenticationPackageId hr=" << putHR(hr);
    return hr;
  }

  // Build the full username as "domain\username".  The domain in this case
  // is the computer name since this is a local account.

  wchar_t computer_name[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD length = base::size(computer_name);
  if (!::GetComputerNameW(computer_name, &length)) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "GetComputerNameW hr=" << putHR(hr);
    return hr;
  }

  length += 1 + SysStringLen(username) + 1;  // backslash, username, null
  std::unique_ptr<wchar_t[]> domain_username(new wchar_t[length]);
  swprintf_s(domain_username.get(), length, L"%s\\%s", computer_name,
             OLE2CW(username));

  // Create the buffer needed to pass back to winlogon.  Get length first.

  cpcs->cbSerialization = 0;
  if (!::CredPackAuthenticationBufferW(0, domain_username.get(),
                                       OLE2W(password), nullptr,
                                       &(cpcs->cbSerialization))) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    if (hr != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
      LOGFN(ERROR) << "CredPackAuthenticationBufferW length"
                   << " dn=" << domain_username.get() << " hr=" << putHR(hr);
      return hr;
    }
  }

  cpcs->rgbSerialization =
      static_cast<LPBYTE>(::CoTaskMemAlloc(cpcs->cbSerialization));
  if (cpcs->rgbSerialization == nullptr) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "can't alloc rgbSerialization "
                 << " dn=" << domain_username.get() << " hr=" << putHR(hr)
                 << " length=" << cpcs->cbSerialization;
    return hr;
  }

  if (!::CredPackAuthenticationBufferW(0, domain_username.get(),
                                       OLE2W(password), cpcs->rgbSerialization,
                                       &(cpcs->cbSerialization))) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "CredPackAuthenticationBufferW "
                 << " dn=" << domain_username.get() << " hr=" << putHR(hr);
    return hr;
  }

  // Caller should fill this in.
  cpcs->clsidCredentialProvider = GUID_NULL;
  return S_OK;
}

}  // namespace

CGaiaCredentialBase::UIProcessInfo::UIProcessInfo() {}

CGaiaCredentialBase::UIProcessInfo::~UIProcessInfo() {}

// static
HRESULT CGaiaCredentialBase::OnDllRegisterServer() {
  OSUserManager* manager = OSUserManager::Get();

  // Generate a random password for the gaia account.
  wchar_t password[32];
  HRESULT hr = manager->GenerateRandomPassword(password, base::size(password));
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GenerateRandomPassword hr=" << putHR(hr);
    return hr;
  }

  // Create the special Gaia account used to run the UI.

  CComBSTR sid_string;
  bool save_password = true;
  base::string16 fullname(GetStringResource(IDS_GAIA_ACCOUNT_FULLNAME));
  base::string16 comment(GetStringResource(IDS_GAIA_ACCOUNT_COMMENT));
  hr = CreateNewUser(manager, kGaiaAccountName, password, fullname.c_str(),
                     comment.c_str(), /*add_to_users_group=*/false,
                     &sid_string);
  if (hr == HRESULT_FROM_WIN32(NERR_UserExists)) {
    // If CreateNewUser() found an existing user, the password was not changed.
    // Consider this a success but don't save the newly generated password
    // in LSA.
    hr = S_OK;
    save_password = false;
  } else if (FAILED(hr)) {
    LOGFN(ERROR) << "CreateNewUser hr=" << putHR(hr);
    return hr;
  }

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  if (!policy) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }

  if (save_password) {
    // Save the password in a machine secret area.
    hr = policy->StorePrivateData(kLsaKeyGaiaPassword, password);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "policy.StorePrivateData hr=" << putHR(hr);
      return hr;
    }
  }

  PSID sid;
  if (!::ConvertStringSidToSid(sid_string, &sid)) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ConvertStringSidToSid hr=" << putHR(hr);
    return hr;
  }

  // Add "logon as batch" right.
  hr = policy->AddAccountRights(sid, SE_BATCH_LOGON_NAME);
  ::LocalFree(sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "policy.AddAccountRights hr=" << putHR(hr);
    return hr;
  }

  return S_OK;
}

// static
HRESULT CGaiaCredentialBase::OnDllUnregisterServer() {
  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  if (policy) {
    wchar_t password[32];

    HRESULT hr = policy->RetrievePrivateData(kLsaKeyGaiaPassword, password,
                                             base::size(password));
    if (FAILED(hr))
      LOGFN(ERROR) << "policy.RetrievePrivateData hr=" << putHR(hr);

    hr = policy->RemovePrivateData(kLsaKeyGaiaPassword);
    if (FAILED(hr))
      LOGFN(ERROR) << "policy.RemovePrivateData hr=" << putHR(hr);

    OSUserManager* manager = OSUserManager::Get();
    PSID sid;

    hr = manager->GetUserSID(kGaiaAccountName, &sid);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "manager.GetUserSID hr=" << putHR(hr);
      sid = nullptr;
    }

    hr = manager->RemoveUser(kGaiaAccountName, password);
    if (FAILED(hr))
      LOGFN(ERROR) << "manager->RemoveUser hr=" << putHR(hr);

    // Remove the account from LSA after the OS account is deleted.
    if (sid != nullptr) {
      hr = policy->RemoveAccount(sid);
      ::LocalFree(sid);
      if (FAILED(hr))
        LOGFN(ERROR) << "policy.RemoveAccount hr=" << putHR(hr);
    }
  } else {
    LOGFN(ERROR) << "ScopedLsaPolicy::Create failed";
  }

  return S_OK;
}

CGaiaCredentialBase::CGaiaCredentialBase()
    : logon_ui_process_(INVALID_HANDLE_VALUE),
      result_status_(STATUS_SUCCESS),
      result_substatus_(STATUS_SUCCESS) {}

CGaiaCredentialBase::~CGaiaCredentialBase() {}

bool CGaiaCredentialBase::AreCredentialsValid() const {
  return username_.Length() > 0 && password_.Length() > 0 && sid_.Length() > 0;
}

HRESULT CGaiaCredentialBase::GetStringValueImpl(DWORD field_id,
                                                wchar_t** value) {
  HRESULT hr = E_INVALIDARG;
  switch (field_id) {
    case FID_DESCRIPTION: {
      base::string16 description(GetStringResource(IDS_AUTH_FID_DESCRIPTION));
      hr = ::SHStrDupW(description.c_str(), value);
      break;
    }
    case FID_PROVIDER_LABEL: {
      base::string16 label(GetStringResource(IDS_AUTH_FID_PROVIDER_LABEL));
      hr = ::SHStrDupW(label.c_str(), value);
      break;
    }
    default:
      break;
  }

  return hr;
}

HRESULT CGaiaCredentialBase::FinishOnUserAuthenticated(BSTR username,
                                                       BSTR password,
                                                       BSTR sid) {
  LOGFN(INFO);
  DCHECK(username);
  DCHECK(password);
  DCHECK(sid);

  username_ = username;
  password_ = password;
  sid_ = sid;

  result_status_ = STATUS_SUCCESS;
  result_substatus_ = STATUS_SUCCESS;
  result_status_text_.clear();

  return provider_->OnUserAuthenticated(static_cast<IGaiaCredential*>(this),
                                        username, password, sid);
}

void CGaiaCredentialBase::ResetInternalState() {
  LOGFN(INFO);
  username_.Empty();
  password_.Empty();
  sid_.Empty();
}

HRESULT CGaiaCredentialBase::GetEmailForReauth(wchar_t* email, size_t length) {
  if (email == nullptr)
    return E_POINTER;

  if (length < 1)
    return E_INVALIDARG;

  email[0] = 0;
  return S_OK;
}

HRESULT CGaiaCredentialBase::GetGlsCommandline(
    const wchar_t* email,
    base::CommandLine* command_line) {
  DCHECK(email);
  DCHECK(command_line);

  // Get the application name.

  base::FilePath install_path;
  HRESULT hr = GetInstallDirectory(&install_path);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetInstallDirectory hr=" << putHR(hr);
    return hr;
  }

  // TODO(crbug.com/887444): Replace this with path to chrome.
  command_line->SetProgram(
      install_path.Append(
          FILE_PATH_LITERAL("weblogin-win32-ia32\\weblogin.exe")));

  LOGFN(INFO) << "App exe: " << command_line->GetProgram().value();

  // Get the command line.

  // If an email pattern is specified, pass it to the webui.
  wchar_t email_pattern[64];
  ULONG length = base::size(email_pattern);
  hr = GetGlobalFlag(L"ep", email_pattern, &length);
  if (FAILED(hr))
    email_pattern[0] = 0;

  // TODO: these arguments will not be needed once the electron app is replaced
  // with chrome.
  std::string id = google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  std::string secret =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);

  if (wcslen(email_pattern) > 0)
    command_line->AppendSwitchNative("pattern", email_pattern);

  if (email && wcslen(email) > 0)
    command_line->AppendSwitchNative("email", email);

  command_line->AppendSwitchASCII("client-id", id);
  command_line->AppendSwitchASCII("client-secret", secret);

  LOGFN(INFO) << "Command line: " << command_line->GetCommandLineString();
  return S_OK;
}

void CGaiaCredentialBase::DisplayErrorInUI(LONG status,
                                           LONG substatus,
                                           BSTR status_text) {
  if (status != STATUS_SUCCESS) {
    base::string16 title(GetStringResource(IDS_ERROR_DIALOG_TITLE));
    ::MessageBoxW(nullptr, OLE2CW(status_text), title.c_str(),
                  MB_TOPMOST | MB_SETFOREGROUND | MB_ICONERROR | MB_OK);
  }
}

HRESULT CGaiaCredentialBase::HandleAutologon(
    CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* cpgsr,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* cpcs) {
  LOGFN(INFO) << "user-sid=" << get_sid().m_str;
  DCHECK(cpgsr);
  DCHECK(cpcs);

  if (!AreCredentialsValid())
    return S_FALSE;

  // The OS user has already been created, so return all the information needed
  // to log them in.
  HRESULT hr =
      BuildCredPackAuthenticationBuffer(get_username(), get_password(), cpcs);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "BuildCredPackAuthenticationBuffer hr=" << putHR(hr);
    return hr;
  }

  cpcs->clsidCredentialProvider = CLSID_GaiaCredentialProvider;

  hr = S_OK;
  *cpgsr = CPGSR_RETURN_CREDENTIAL_FINISHED;

  return hr;
}

// static
void CGaiaCredentialBase::TellOmahaDidRun() {
#if defined(GOOGLE_CHROME_BUILD)
  // Tell omaha that product was used.  Best effort only.
  //
  // This code always runs as LocalSystem, which means that HKCU maps to
  // HKU\.Default.  This is OK because omaha reads the "dr" value from subkeys
  // of HKEY_USERS.
  base::win::RegKey key;
  LONG sts = key.Create(HKEY_CURRENT_USER, kRegUpdaterClientStateAppPath,
                        KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (sts != ERROR_SUCCESS) {
    LOGFN(INFO) << "Unable to open omaha key sts=" << sts;
  } else {
    sts = key.WriteValue(L"dr", L"1");
    if (sts != ERROR_SUCCESS)
      LOGFN(INFO) << "Unable to write omaha dr value sts=" << sts;
  }
#endif  // defined(GOOGLE_CHROME_BUILD)
}

// static
HRESULT CGaiaCredentialBase::CreateNewUser(OSUserManager* manager,
                                           const wchar_t* username,
                                           const wchar_t* password,
                                           const wchar_t* fullname,
                                           const wchar_t* comment,
                                           bool add_to_users_group,
                                           BSTR* sid) {
  DWORD error;
  HRESULT hr = manager->AddUser(username, password, fullname, comment,
                                add_to_users_group, sid, &error);
  LOGFN(INFO) << "hr=" << putHR(hr) << " username=" << username;
  return hr;
}

// static
BSTR CGaiaCredentialBase::AllocErrorString(UINT id) {
  CComBSTR str;
  str.LoadStringW(CURRENT_MODULE(), id);
  return str.Detach();
}

// static
HRESULT CGaiaCredentialBase::GetInstallDirectory(base::FilePath* path) {
  DCHECK(path);

  if (!base::PathService::Get(base::FILE_MODULE, path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "Get(FILE_MODULE) hr=" << putHR(hr);
    return hr;
  }

  *path = path->DirName();
  return S_OK;
}

// ICredentialProviderCredential //////////////////////////////////////////////

HRESULT CGaiaCredentialBase::Advise(ICredentialProviderCredentialEvents* cpce) {
  LOGFN(INFO);
  events_ = cpce;
  return S_OK;
}

HRESULT CGaiaCredentialBase::UnAdvise(void) {
  LOGFN(INFO);
  events_.Release();
  return S_OK;
}

HRESULT CGaiaCredentialBase::SetSelected(BOOL* auto_login) {
  *auto_login = AreCredentialsValid();
  LOGFN(INFO) << "auto-login=" << *auto_login;
  return S_OK;
}

HRESULT CGaiaCredentialBase::SetDeselected(void) {
  LOGFN(INFO);

  // Terminate login UI process if started.  This is best effort since it may
  // have already terminated.
  if (logon_ui_process_ != INVALID_HANDLE_VALUE) {
    LOGFN(INFO) << "Attempting to kill logon UI process";
    ::TerminateProcess(logon_ui_process_, kUiecKilled);
    logon_ui_process_ = INVALID_HANDLE_VALUE;
  }

  // Do not reset the internal state here, otherwise auto-logon will not work.

  return S_OK;
}

HRESULT CGaiaCredentialBase::GetFieldState(
    DWORD field_id,
    CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs,
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis) {
  HRESULT hr = E_INVALIDARG;
  switch (field_id) {
    case FID_DESCRIPTION:
    case FID_SUBMIT:
      *pcpfs = CPFS_DISPLAY_IN_SELECTED_TILE;
      *pcpfis = CPFIS_NONE;
      hr = S_OK;
      break;
    case FID_PROVIDER_LOGO:
      *pcpfs = ::IsWindows8OrGreater() ? CPFS_HIDDEN : CPFS_DISPLAY_IN_BOTH;
      *pcpfis = CPFIS_NONE;
      hr = S_OK;
      break;
    case FID_PROVIDER_LABEL:
      *pcpfs = ::IsWindows8OrGreater() ? CPFS_HIDDEN
                                       : CPFS_DISPLAY_IN_DESELECTED_TILE;
      *pcpfis = CPFIS_NONE;
      hr = S_OK;
      break;
    default:
      break;
  }
  LOGFN(INFO) << "hr=" << putHR(hr) << " field=" << field_id
              << " state=" << *pcpfs << " inter-state=" << *pcpfis;
  return hr;
}

HRESULT CGaiaCredentialBase::GetStringValue(DWORD field_id, wchar_t** value) {
  return GetStringValueImpl(field_id, value);
}

HRESULT CGaiaCredentialBase::GetBitmapValue(DWORD field_id, HBITMAP* phbmp) {
  HRESULT hr = E_INVALIDARG;
  switch (field_id) {
    case FID_PROVIDER_LOGO:
      *phbmp = ::LoadBitmap(CURRENT_MODULE(),
                            MAKEINTRESOURCE(IDB_GOOGLE_LOGO_SMALL));
      if (*phbmp)
        hr = S_OK;
      break;
    default:
      break;
  }

  return hr;
}

HRESULT CGaiaCredentialBase::GetCheckboxValue(DWORD field_id,
                                              BOOL* pbChecked,
                                              wchar_t** ppszLabel) {
  // No checkboxes.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::GetSubmitButtonValue(DWORD field_id,
                                                  DWORD* adjacent_to) {
  HRESULT hr = E_INVALIDARG;
  switch (field_id) {
    case FID_SUBMIT:
      *adjacent_to = FID_DESCRIPTION;
      hr = S_OK;
      break;
    default:
      break;
  }

  return hr;
}

HRESULT CGaiaCredentialBase::GetComboBoxValueCount(DWORD field_id,
                                                   DWORD* pcItems,
                                                   DWORD* pdwSelectedItem) {
  // No comboboxes.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::GetComboBoxValueAt(DWORD field_id,
                                                DWORD dwItem,
                                                wchar_t** ppszItem) {
  // No comboboxes.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::SetStringValue(DWORD field_id,
                                            const wchar_t* psz) {
  // No editable strings.
  HRESULT hr = E_INVALIDARG;
  return hr;
}

HRESULT CGaiaCredentialBase::SetCheckboxValue(DWORD field_id, BOOL bChecked) {
  // No checkboxes.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::SetComboBoxSelectedValue(DWORD field_id,
                                                      DWORD dwSelectedItem) {
  // No comboboxes.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::CommandLinkClicked(DWORD dwFieldID) {
  // No links.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::GetSerialization(
    CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* cpgsr,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* cpcs,
    wchar_t** status_text,
    CREDENTIAL_PROVIDER_STATUS_ICON* status_icon) {
  LOGFN(INFO);
  DCHECK(status_text);
  DCHECK(status_icon);

  *status_text = nullptr;
  *status_icon = CPSI_NONE;

  HRESULT hr = HandleAutologon(cpgsr, cpcs);

  // Clear the state of the credential on error or on autologon.
  if (hr != S_FALSE)
    ResetInternalState();

  if (FAILED(hr)) {
    LOGFN(ERROR) << "HandleAutologon hr=" << putHR(hr);
    return hr;
  }

  // If HandleAutologon returns S_FALSE, then there was not enough information
  // to log the user on.  Display the Gaia sign in page.
  if (hr == S_FALSE) {
    LOGFN(INFO) << "HandleAutologon hr=" << putHR(hr);
    TellOmahaDidRun();

    // The account creation is async so we are not done yet.
    *cpgsr = CPGSR_NO_CREDENTIAL_NOT_FINISHED;

    // The expectation is that the UI will eventually return the username,
    // password, and auth to this CGaiaCredentialBase object, so that
    // OnUserAuthenticated() can be called, followed by
    // provider_->OnUserAuthenticated().
    hr = CreateAndRunLogonStub();
  }

  return hr;
}

HRESULT CGaiaCredentialBase::CreateAndRunLogonStub() {
  LOGFN(INFO);

  wchar_t email[64];
  HRESULT hr = GetEmailForReauth(email, base::size(email));
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetEmailForReauth hr=" << putHR(hr);
    return hr;
  }

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  hr = GetGlsCommandline(email, &command_line);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetGlsCommandline hr=" << putHR(hr);
    return hr;
  }

  // The process should start on the interactive window station (since it
  // needs to show a UI) but on its own desktop so that it cannot interact
  // with winlogon on user windows.
  std::unique_ptr<UIProcessInfo> uiprocinfo(new UIProcessInfo);
  PSID logon_sid;
  hr = CreateGaiaLogonToken(&uiprocinfo->logon_token, &logon_sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "CreateGaiaLogonToken hr=" << putHR(hr);
    return hr;
  }

  OSProcessManager* process_manager = OSProcessManager::Get();
  hr = process_manager->SetupPermissionsForLogonSid(logon_sid);
  LocalFree(logon_sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetupPermissionsForLogonSid hr=" << putHR(hr);
    return hr;
  }

  hr = ForkGaiaLogonStub(process_manager, command_line, uiprocinfo.get());
  if (FAILED(hr)) {
    LOGFN(ERROR) << "ForkGaiaLogonStub hr=" << putHR(hr);
    return hr;
  }

  // Save the handle to the logon UI process so that it can be killed should
  // the credential be Unadvise()d.
  logon_ui_process_ = uiprocinfo->procinfo.process_handle();

  uiprocinfo->credential = this;

  // Background thread takes ownership of |uiprocinfo|.
  unsigned int wait_thread_id;
  uintptr_t wait_thread = _beginthreadex(
      nullptr, 0, WaitForLoginUI, uiprocinfo.release(), 0, &wait_thread_id);
  if (wait_thread != 0) {
    LOGFN(INFO) << "Started wait thread id=" << wait_thread_id;
    ::CloseHandle((HANDLE)wait_thread);
  } else {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "Unable to start wait thread hr=" << putHR(hr);
    ::TerminateProcess(uiprocinfo->procinfo.process_handle(), kUiecKilled);
    return hr;
  }

  // This function returns success, which means that GetSerialization() will
  // return success.  CGaiaCredentialBase is now committed to telling
  // CGaiaCredentialProvider whether the serialization eventually succeeds or
  // fails, so that CGaiaCredentialProvider can in turn inform winlogon about
  // what happened.
  LOGFN(INFO) << "cleaning up";
  return S_OK;
}

// static
HRESULT CGaiaCredentialBase::CreateGaiaLogonToken(
    base::win::ScopedHandle* token,
    PSID* sid) {
  DCHECK(token);
  DCHECK(sid);

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  if (!policy) {
    LOGFN(ERROR) << "LsaOpenPolicy failed";
    return E_UNEXPECTED;
  }

  wchar_t password[32];
  HRESULT hr = policy->RetrievePrivateData(kLsaKeyGaiaPassword, password,
                                           base::size(password));
  if (FAILED(hr)) {
    LOGFN(ERROR) << "policy.RetrievePrivateData hr=" << putHR(hr);
    return hr;
  }

  hr = OSUserManager::Get()->CreateLogonToken(kGaiaAccountName, password,
                                              /*interactive=*/false, token);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "CreateLogonToken hr=" << putHR(hr);
    return hr;
  }

  hr = OSProcessManager::Get()->GetTokenLogonSID(*token, sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetTokenLogonSID hr=" << putHR(hr);
    token->Close();
    return hr;
  }

  wchar_t* sid_string;
  if (::ConvertSidToStringSid(*sid, &sid_string)) {
    LOGFN(INFO) << "logon-sid=" << sid_string;
    LocalFree(sid_string);
  } else {
    LOGFN(ERROR) << "logon-sid=<can't get string>";
  }

  return S_OK;
}

// static
HRESULT CGaiaCredentialBase::ForkGaiaLogonStub(
    OSProcessManager* process_manager,
    const base::CommandLine& command_line,
    UIProcessInfo* uiprocinfo) {
  LOGFN(INFO);
  DCHECK(process_manager);
  DCHECK(uiprocinfo);

  ScopedStartupInfo startupinfo(kDesktopFullName);
  HRESULT hr = InitializeStdHandles(CommDirection::kChildToParentOnly,
                                    &startupinfo, &uiprocinfo->parent_handles);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "InitializeStdHandles hr=" << putHR(hr);
    return hr;
  }

  // The process is created suspended so that we can adjust its environment
  // before it starts.  Also, it must not run before it is added to the job
  // object.
  hr = process_manager->CreateProcessWithToken(
      uiprocinfo->logon_token, command_line, startupinfo.GetInfo(),
      &uiprocinfo->procinfo);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "CreateProcessWithTokenW hr=" << putHR(hr);
    return hr;
  }

  LOGFN(INFO) << "pid=" << uiprocinfo->procinfo.process_id()
              << " tid=" << uiprocinfo->procinfo.thread_id();

  // Don't create a job here with UI restrictions, since win10 does not allow
  // nested jobs unless all jobs don't specify UI restrictions.  Since chrome
  // will set a job with UI restrictions for renderer/gpu/etc processes, setting
  // one here causes chrome to fail.

  // Environment is fully set up for UI, so let it go.
  if (::ResumeThread(uiprocinfo->procinfo.thread_handle()) ==
      static_cast<DWORD>(-1)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ResumeThread hr=" << putHR(hr);
    ::TerminateProcess(uiprocinfo->procinfo.process_handle(), kUiecKilled);
    return hr;
  }

  // Don't close the desktop until after the process has started and acquired
  // a handle to it.  Otherwise, the desktop will be destroyed and the process
  // will fail to start.
  //
  // WaitForInputIdle() return immediately with an error if the process created
  // is a console app.  In production this will not be the case, however in
  // tests this may happen.  However, tests are not concerned with the
  // destruction of the desktop since one is not created.
  DWORD ret = ::WaitForInputIdle(uiprocinfo->procinfo.process_handle(), 10000);
  if (ret != 0)
    LOGFN(INFO) << "WaitForInputIdle, ret=" << ret;

  return S_OK;
}

// static
HRESULT CGaiaCredentialBase::ForkSaveAccountInfoStub(
    const std::unique_ptr<base::DictionaryValue>& dict,
    BSTR* status_text) {
  LOGFN(INFO);
  DCHECK(status_text);

  ScopedStartupInfo startupinfo;
  StdParentHandles parent_handles;
  HRESULT hr = InitializeStdHandles(CommDirection::kParentToChildOnly,
                                    &startupinfo, &parent_handles);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "InitializeStdHandles hr=" << putHR(hr);
    *status_text = AllocErrorString(IDS_INTERNAL_ERROR);
    return hr;
  }

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  hr = GetCommandLineForEntrypoint(CURRENT_MODULE(), L"SaveAccountInfo",
                                   &command_line);
  if (hr == S_FALSE) {
    // This happens in tests.  It means this code is running inside the
    // unittest exe and not the credential provider dll.  Just ignore saving
    // the account info.
    LOGFN(INFO) << "Not running SAIS";
    return S_OK;
  } else if (FAILED(hr)) {
    LOGFN(ERROR) << "GetCommandLineForEntryPoint hr=" << putHR(hr);
    *status_text = AllocErrorString(IDS_INTERNAL_ERROR);
    return hr;
  }

  base::win::ScopedProcessInformation procinfo;
  hr = OSProcessManager::Get()->CreateRunningProcess(
      command_line, startupinfo.GetInfo(), &procinfo);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "CreateProcessWithTokenW hr=" << putHR(hr);
    return hr;
  }

  // Write account info to stdin of child process.  This buffer is read by
  // SaveAccountInfoW() in dllmain.cpp.
  std::string json;
  if (base::JSONWriter::Write(*dict, &json)) {
    DWORD written;
    if (!::WriteFile(parent_handles.hstdin_write.Get(), json.c_str(),
                     json.length() + 1, &written, /*lpOverlapped=*/nullptr)) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "WriteFile hr=" << putHR(hr);
      *status_text = AllocErrorString(IDS_INTERNAL_ERROR);
    }
  } else {
    LOGFN(ERROR) << "base::JSONWriter::Write failed";
  }

  return S_OK;
}

// static
unsigned __stdcall CGaiaCredentialBase::WaitForLoginUI(void* param) {
  DCHECK(param);
  std::unique_ptr<UIProcessInfo> uiprocinfo(
      reinterpret_cast<UIProcessInfo*>(param));

  // If WaitForLoginUIImpl() returns successfully, it is assumed that the
  // returned property list |properties| is valid and contains all properties
  // needed to continue with logon.
  std::unique_ptr<base::DictionaryValue> properties;
  CComBSTR status_text;
  HRESULT hr = WaitForLoginUIImpl(uiprocinfo.get(), &properties, &status_text);
  if (SUCCEEDED(hr)) {
    // Notify that the new user is created.
    // TODO(rogerta): Docs say this should not be called on a background thread,
    // but on the thread that received the CGaiaCredentialBase::Advise() call.
    // Seems to work for now though, but I suspect there could be a problem if
    // this call races with a call to CGaiaCredentialBase::Unadvise().
    base::string16 username = GetDictString(properties, kKeyUsername);
    base::string16 password = GetDictString(properties, kKeyPassword);
    base::string16 sid = GetDictString(properties, kKeySID);

    hr = uiprocinfo->credential->OnUserAuthenticated(
        CComBSTR(W2COLE(username.c_str())), CComBSTR(W2COLE(password.c_str())),
        CComBSTR(W2COLE(sid.c_str())));
    if (FAILED(hr)) {
      LOGFN(ERROR) << "uiprocinfo->credential->OnUserAuthenticated hr="
                   << putHR(hr);
    }
  } else {
    // If hr is E_ABORT, this is a user initiated cancel.  Don't consider this
    // an error.
    LONG sts = hr == E_ABORT ? STATUS_SUCCESS : HRESULT_CODE(hr);
    hr = uiprocinfo->credential->ReportError(sts, STATUS_SUCCESS, status_text);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "uiprocinfo->credential->ReportError hr=" << putHR(hr);
    }
  }

  LOGFN(INFO) << "done";
  return 0;
}

// static
HRESULT CGaiaCredentialBase::WaitForLoginUIImpl(
    UIProcessInfo* uiprocinfo,
    std::unique_ptr<base::DictionaryValue>* properties,
    BSTR* status_text) {
  USES_CONVERSION;
  LOGFN(INFO);
  DCHECK(uiprocinfo);
  DCHECK(properties);
  DCHECK(status_text);

  properties->reset();
  *status_text = nullptr;
  std::unique_ptr<base::DictionaryValue> dict;

  HRESULT hr = WaitForLoginUIAndGetResult(uiprocinfo, &dict, status_text);
  if (FAILED(hr)) {
    if (hr != E_ABORT)
      LOGFN(ERROR) << "WaitForLoginUIAndGetResult hr=" << putHR(hr);
    return hr;
  }

  hr = ValidateAndFixResult(dict.get(), status_text);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "ValidateAndFixResult hr=" << putHR(hr);
    return hr;
  }

  // From this point the code assume the dictionary |dict| is valid.

  CComBSTR sid;
  hr = uiprocinfo->credential->FinishAuthentication(
      CComBSTR(W2COLE(GetDictString(dict, kKeyUsername).c_str())),
      CComBSTR(W2COLE(GetDictString(dict, kKeyPassword).c_str())),
      CComBSTR(W2COLE(GetDictString(dict, kKeyFullname).c_str())), &sid,
      status_text);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "credential->FinishAuthentication hr=" << putHR(hr);
    return hr;
  }

  dict->SetString(kKeySID, OLE2CA(sid));

  if (SUCCEEDED(hr)) {
    // Fire off a process to call SaveAccountInfo().
    //
    // The eventual call to OnUserAuthenticated() will tell winlogon that
    // logging in is finished. It seems that winlogon will kill this process
    // after a short time, which races with an attempt to save the account info
    // to the registry if done here.  For this reason a child pocess is used.
    hr = ForkSaveAccountInfoStub(dict, status_text);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "ForkSaveAccountInfoStub hr=" << putHR(hr);
      return hr;
    }

    *properties = std::move(dict);
  }

  // When this function returns, winlogon will be told to logon to the newly
  // created account.  This is important, as the save account info process
  // can't actually save the info until the user's profile is created, which
  // happens on first logon.

  return hr;
}

// static
HRESULT CGaiaCredentialBase::SaveAccountInfo(
    const base::DictionaryValue& properties) {
  LOGFN(INFO);

  base::string16 sid = GetDictString(&properties, kKeySID);
  if (sid.empty()) {
    LOGFN(ERROR) << "SID is empty";
    return E_INVALIDARG;
  }

  base::string16 username = GetDictString(&properties, kKeyUsername);
  if (username.empty()) {
    LOGFN(ERROR) << "Username is empty";
    return E_INVALIDARG;
  }

  base::string16 password = GetDictString(&properties, kKeyPassword);
  if (password.empty()) {
    LOGFN(ERROR) << "Password is empty";
    return E_INVALIDARG;
  }

  // Load the user's profile so that their regsitry hive is available.
  auto profile = ScopedUserProfile::Create(sid, username, password);
  if (!profile) {
    LOGFN(ERROR) << "Could not load user profile";
    return E_UNEXPECTED;
  }

  HRESULT hr = profile->SaveAccountInfo(properties);
  if (FAILED(hr))
    LOGFN(ERROR) << "profile.SaveAccountInfo failed (cont) hr=" << putHR(hr);

  return hr;
}

HRESULT CGaiaCredentialBase::ReportResult(
    NTSTATUS status,
    NTSTATUS substatus,
    wchar_t** ppszOptionalStatusText,
    CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon) {
  LOGFN(INFO) << "status=" << putHR(status)
              << " substatus=" << putHR(substatus);

  *ppszOptionalStatusText = nullptr;
  *pcpsiOptionalStatusIcon = CPSI_NONE;
  ResetInternalState();
  return S_OK;
}

HRESULT CGaiaCredentialBase::GetUserSid(wchar_t** sid) {
  *sid = nullptr;
  return S_FALSE;
}

HRESULT CGaiaCredentialBase::Initialize(IGaiaCredentialProvider* provider) {
  LOGFN(INFO);
  DCHECK(provider);

  provider_ = provider;
  return S_OK;
}

HRESULT CGaiaCredentialBase::Terminate() {
  LOGFN(INFO);
  SetDeselected();
  provider_.Release();
  return S_OK;
}

HRESULT CGaiaCredentialBase::FinishAuthentication(BSTR username,
                                                  BSTR password,
                                                  BSTR fullname,
                                                  BSTR* sid,
                                                  BSTR* error_text) {
  // Derived classes need to implement this.
  return E_NOTIMPL;
}

HRESULT CGaiaCredentialBase::OnUserAuthenticated(BSTR username,
                                                 BSTR password,
                                                 BSTR sid) {
  return FinishOnUserAuthenticated(username, password, sid);
}

HRESULT CGaiaCredentialBase::ReportError(LONG status,
                                         LONG substatus,
                                         BSTR status_text) {
  USES_CONVERSION;
  LOGFN(INFO);

  result_status_ = status;
  result_substatus_ = substatus;

  if (status_text != nullptr)
    result_status_text_.assign(OLE2CW(status_text));

  // TODO(rogerta): for some reason the error info saved by ReportError()
  // never gets used because ReportResult() is never called by winlogon.exe
  // when the logon fails.  Not sure what I'm doing wrong here.  This
  // message box does show the error at the appropriate time though.
  DisplayErrorInUI(status, STATUS_SUCCESS, status_text);

  return provider_->OnUserAuthenticated(nullptr, CComBSTR(), CComBSTR(),
                                        CComBSTR());
}

}  // namespace credential_provider

