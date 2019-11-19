// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/test/gcp_fakes.h"

#include <windows.h>

#include <lm.h>
#include <sddl.h>

#include <atlcomcli.h>
#include <atlconv.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace {

HRESULT CreateArbitrarySid(DWORD subauth0, PSID* sid) {
  SID_IDENTIFIER_AUTHORITY Authority = {SECURITY_NON_UNIQUE_AUTHORITY};
  if (!::AllocateAndInitializeSid(&Authority, 1, subauth0, 0, 0, 0, 0, 0, 0, 0,
                                  sid)) {
    return (HRESULT_FROM_WIN32(::GetLastError()));
  }
  return S_OK;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////

void InitializeRegistryOverrideForTesting(
    registry_util::RegistryOverrideManager* registry_override) {
  ASSERT_NO_FATAL_FAILURE(
      registry_override->OverrideRegistry(HKEY_LOCAL_MACHINE));
  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_LOCAL_MACHINE, kGcpRootKeyName, KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kRegMdmUrl, L""));
  ASSERT_EQ(ERROR_SUCCESS,
            SetMachineGuidForTesting(L"f418a124-4d92-469b-afa5-0f8af537b965"));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kRegEscrowServiceServerUrl, L""));
}

///////////////////////////////////////////////////////////////////////////////

FakeOSProcessManager::FakeOSProcessManager()
    : original_manager_(*GetInstanceStorage()) {
  *GetInstanceStorage() = this;
}

FakeOSProcessManager::~FakeOSProcessManager() {
  *GetInstanceStorage() = original_manager_;
}

HRESULT FakeOSProcessManager::GetTokenLogonSID(
    const base::win::ScopedHandle& token,
    PSID* sid) {
  // Make sure the token is valid, but otherwise ignore it.
  if (!token.IsValid())
    return E_INVALIDARG;

  return CreateArbitrarySid(++next_rid_, sid);
}

HRESULT FakeOSProcessManager::SetupPermissionsForLogonSid(PSID sid) {
  // Ignore.
  return S_OK;
}

HRESULT FakeOSProcessManager::CreateProcessWithToken(
    const base::win::ScopedHandle& logon_token,
    const base::CommandLine& command_line,
    _STARTUPINFOW* startupinfo,
    base::win::ScopedProcessInformation* procinfo) {
  // Ignore the logon token and create a process as the current user.
  // If the startupinfo includes a desktop name, make sure to ignore.  In tests
  // the desktop has not been configured to allow a newly created process to
  // to access it.
  _STARTUPINFOW local_startupinfo = *startupinfo;
  local_startupinfo.lpDesktop = nullptr;

  PROCESS_INFORMATION new_procinfo = {};
  // Pass a copy of the command line string to CreateProcessW() because this
  // function could change the string.
  std::unique_ptr<wchar_t, void (*)(void*)> cmdline(
      _wcsdup(command_line.GetCommandLineString().c_str()), std::free);
  if (!::CreateProcessW(command_line.GetProgram().value().c_str(),
                        cmdline.get(), nullptr, nullptr, TRUE, CREATE_SUSPENDED,
                        nullptr, nullptr, &local_startupinfo, &new_procinfo)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    return hr;
  }
  procinfo->Set(new_procinfo);
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////

FakeOSUserManager::FakeOSUserManager()
    : original_manager_(*GetInstanceStorage()) {
  *GetInstanceStorage() = this;
}

FakeOSUserManager::~FakeOSUserManager() {
  *GetInstanceStorage() = original_manager_;
}

HRESULT FakeOSUserManager::GenerateRandomPassword(wchar_t* password,
                                                  int length) {
  if (length < kMinPasswordLength)
    return E_INVALIDARG;

  // Make sure to generate a different password each time.  Actually randomness
  // is not important for tests.
  static int nonce = 0;
  EXPECT_NE(-1, swprintf_s(password, length, L"bad-password-%d", ++nonce));
  return S_OK;
}

HRESULT FakeOSUserManager::AddUser(const wchar_t* username,
                                   const wchar_t* password,
                                   const wchar_t* fullname,
                                   const wchar_t* comment,
                                   bool add_to_users_group,
                                   BSTR* sid,
                                   DWORD* error) {
  return AddUser(username, password, fullname, comment, add_to_users_group,
                 OSUserManager::GetLocalDomain().c_str(), sid, error);
}

HRESULT FakeOSUserManager::AddUser(const wchar_t* username,
                                   const wchar_t* password,
                                   const wchar_t* fullname,
                                   const wchar_t* comment,
                                   bool add_to_users_group,
                                   const wchar_t* domain,
                                   BSTR* sid,
                                   DWORD* error) {
  USES_CONVERSION;

  DCHECK(sid);

  if (error)
    *error = 0;

  if (should_fail_user_creation_)
    return E_FAIL;

  // Username or password cannot be empty.
  if (username == nullptr || !username[0] || password == nullptr ||
      !password[0])
    return E_FAIL;

  bool user_found = username_to_info_.count(username) > 0;

  if (user_found) {
    *sid = ::SysAllocString(W2COLE(username_to_info_[username].sid.c_str()));
    return HRESULT_FROM_WIN32(NERR_UserExists);
  }

  PSID psid = nullptr;
  HRESULT hr = CreateNewSID(&psid);
  if (FAILED(hr))
    return hr;

  wchar_t* sidstr = nullptr;
  bool ok = ::ConvertSidToStringSid(psid, &sidstr);
  ::FreeSid(psid);
  if (!ok) {
    *sid = nullptr;
    return HRESULT_FROM_WIN32(NERR_ProgNeedsExtraMem);
  }

  *sid = ::SysAllocString(W2COLE(sidstr));
  username_to_info_.emplace(
      username, UserInfo(domain, password, fullname, comment, sidstr));
  ::LocalFree(sidstr);

  return S_OK;
}

HRESULT FakeOSUserManager::ChangeUserPassword(const wchar_t* domain,
                                              const wchar_t* username,
                                              const wchar_t* old_password,
                                              const wchar_t* new_password) {
  DCHECK(domain);
  DCHECK(username);
  DCHECK(old_password);
  DCHECK(new_password);

  if (username_to_info_.count(username) > 0) {
    if (username_to_info_[username].password != old_password)
      return HRESULT_FROM_WIN32(ERROR_INVALID_PASSWORD);

    username_to_info_[username].password = new_password;
    return S_OK;
  }

  return HRESULT_FROM_WIN32(NERR_UserNotFound);
}

HRESULT FakeOSUserManager::SetUserPassword(const wchar_t* domain,
                                           const wchar_t* username,
                                           const wchar_t* new_password) {
  DCHECK(domain);
  DCHECK(username);
  DCHECK(new_password);

  if (username_to_info_.count(username) > 0) {
    username_to_info_[username].password = new_password;
    return S_OK;
  }

  return HRESULT_FROM_WIN32(NERR_UserNotFound);
}

HRESULT FakeOSUserManager::SetUserFullname(const wchar_t* domain,
                                           const wchar_t* username,
                                           const wchar_t* full_name) {
  DCHECK(domain);
  DCHECK(username);
  DCHECK(full_name);

  if (username_to_info_.count(username) > 0) {
    username_to_info_[username].fullname = full_name;
    return S_OK;
  }

  return HRESULT_FROM_WIN32(NERR_UserNotFound);
}

HRESULT FakeOSUserManager::IsWindowsPasswordValid(const wchar_t* domain,
                                                  const wchar_t* username,
                                                  const wchar_t* password) {
  DCHECK(domain);
  DCHECK(username);
  DCHECK(password);

  if (username_to_info_.count(username) > 0) {
    const UserInfo& info = username_to_info_[username];
    if (info.domain != domain)
      return HRESULT_FROM_WIN32(NERR_UserNotFound);

    return info.password == password ? S_OK : S_FALSE;
  }

  return HRESULT_FROM_WIN32(NERR_UserNotFound);
}

HRESULT FakeOSUserManager::CreateLogonToken(const wchar_t* domain,
                                            const wchar_t* username,
                                            const wchar_t* password,
                                            bool /*interactive*/,
                                            base::win::ScopedHandle* token) {
  DCHECK(domain);
  DCHECK(username);
  DCHECK(password);

  if (username_to_info_.count(username) == 0) {
    return HRESULT_FROM_WIN32(NERR_BadUsername);
  } else if (username_to_info_[username].password != password) {
    return HRESULT_FROM_WIN32(NERR_UserExists);
  }

  const UserInfo& info = username_to_info_[username];
  if (info.domain != domain)
    return HRESULT_FROM_WIN32(NERR_BadUsername);

  // Create a token with a dummy handle value.
  base::FilePath path;
  if (!base::CreateTemporaryFile(&path))
    return HRESULT_FROM_WIN32(::GetLastError());

  token->Set(CreateFile(path.value().c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                        nullptr, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
                        nullptr));
  return token->IsValid() ? S_OK : HRESULT_FROM_WIN32(::GetLastError());
}

bool FakeOSUserManager::IsDeviceDomainJoined() {
  return is_device_domain_joined_;
}

HRESULT FakeOSUserManager::GetUserSID(const wchar_t* domain,
                                      const wchar_t* username,
                                      PSID* sid) {
  DCHECK(domain);
  DCHECK(username);
  DCHECK(sid);
  if (username_to_info_.count(username) > 0) {
    const UserInfo& info = username_to_info_[username];
    if (info.domain == domain) {
      if (!::ConvertStringSidToSid(info.sid.c_str(), sid))
        return HRESULT_FROM_WIN32(NERR_ProgNeedsExtraMem);

      return S_OK;
    }
  }

  return HRESULT_FROM_WIN32(NERR_UserNotFound);
}

HRESULT FakeOSUserManager::FindUserBySID(const wchar_t* sid,
                                         wchar_t* username,
                                         DWORD username_size,
                                         wchar_t* domain,
                                         DWORD domain_size) {
  for (auto& kv : username_to_info_) {
    if (kv.second.sid == sid) {
      if (username)
        wcscpy_s(username, username_size, kv.first.c_str());
      if (domain)
        wcscpy_s(domain, domain_size, kv.second.domain.c_str());
      return S_OK;
    }
  }

  return HRESULT_FROM_WIN32(ERROR_NONE_MAPPED);
}

HRESULT FakeOSUserManager::RemoveUser(const wchar_t* username,
                                      const wchar_t* password) {
  username_to_info_.erase(username);
  return S_OK;
}

HRESULT FakeOSUserManager::GetUserFullname(const wchar_t* domain,
                                           const wchar_t* username,
                                           base::string16* fullname) {
  DCHECK(domain);
  DCHECK(username);
  DCHECK(fullname);
  if (username_to_info_.count(username) > 0) {
    const UserInfo& info = username_to_info_[username];
    if (info.domain == domain) {
      *fullname = info.fullname;
      return S_OK;
    }
  }

  return HRESULT_FROM_WIN32(NERR_UserNotFound);
}

HRESULT FakeOSUserManager::ModifyUserAccessWithLogonHours(
    const wchar_t* domain,
    const wchar_t* username,
    bool allow) {
  return S_OK;
}

FakeOSUserManager::UserInfo::UserInfo(const wchar_t* domain,
                                      const wchar_t* password,
                                      const wchar_t* fullname,
                                      const wchar_t* comment,
                                      const wchar_t* sid)
    : domain(domain),
      password(password),
      fullname(fullname),
      comment(comment),
      sid(sid) {}

FakeOSUserManager::UserInfo::UserInfo() {}

FakeOSUserManager::UserInfo::UserInfo(const UserInfo& other) = default;

FakeOSUserManager::UserInfo::~UserInfo() {}

bool FakeOSUserManager::UserInfo::operator==(const UserInfo& other) const {
  return domain == other.domain && password == other.password &&
         fullname == other.fullname && comment == other.comment &&
         sid == other.sid;
}

const FakeOSUserManager::UserInfo FakeOSUserManager::GetUserInfo(
    const wchar_t* username) {
  return (username_to_info_.count(username) > 0) ? username_to_info_[username]
                                                 : UserInfo();
}

HRESULT FakeOSUserManager::CreateNewSID(PSID* sid) {
  return CreateArbitrarySid(++next_rid_, sid);
}

// Creates a test OS user using the local domain.
HRESULT FakeOSUserManager::CreateTestOSUser(const base::string16& username,
                                            const base::string16& password,
                                            const base::string16& fullname,
                                            const base::string16& comment,
                                            const base::string16& gaia_id,
                                            const base::string16& email,
                                            BSTR* sid) {
  return CreateTestOSUser(username, password, fullname, comment, gaia_id, email,
                          OSUserManager::GetLocalDomain(), sid);
}

HRESULT FakeOSUserManager::CreateTestOSUser(const base::string16& username,
                                            const base::string16& password,
                                            const base::string16& fullname,
                                            const base::string16& comment,
                                            const base::string16& gaia_id,
                                            const base::string16& email,
                                            const base::string16& domain,
                                            BSTR* sid) {
  DWORD error;
  HRESULT hr = AddUser(username.c_str(), password.c_str(), fullname.c_str(),
                       comment.c_str(), true, domain.c_str(), sid, &error);
  if (FAILED(hr))
    return hr;

  if (!gaia_id.empty()) {
    hr = SetUserProperty(OLE2CW(*sid), kUserId, gaia_id);
    if (FAILED(hr))
      return hr;

    hr = SetUserProperty(OLE2CW(*sid), kUserTokenHandle, L"token_handle");
    if (FAILED(hr))
      return hr;
  }

  if (!email.empty()) {
    hr = SetUserProperty(OLE2CW(*sid), kUserEmail, email);
    if (FAILED(hr))
      return hr;
  }

  return S_OK;
}

std::vector<std::pair<base::string16, base::string16>>
FakeOSUserManager::GetUsers() const {
  std::vector<std::pair<base::string16, base::string16>> users;

  for (auto& kv : username_to_info_)
    users.emplace_back(std::make_pair(kv.second.sid, kv.first));

  return users;
}

///////////////////////////////////////////////////////////////////////////////

FakeScopedLsaPolicyFactory::FakeScopedLsaPolicyFactory()
    : original_creator_(*ScopedLsaPolicy::GetCreatorCallbackStorage()) {
  *ScopedLsaPolicy::GetCreatorCallbackStorage() = GetCreatorCallback();
}

FakeScopedLsaPolicyFactory::~FakeScopedLsaPolicyFactory() {
  *ScopedLsaPolicy::GetCreatorCallbackStorage() = original_creator_;
}

ScopedLsaPolicy::CreatorCallback
FakeScopedLsaPolicyFactory::GetCreatorCallback() {
  return base::BindRepeating(&FakeScopedLsaPolicyFactory::Create,
                             base::Unretained(this));
}

std::unique_ptr<ScopedLsaPolicy> FakeScopedLsaPolicyFactory::Create(
    ACCESS_MASK mask) {
  return std::unique_ptr<ScopedLsaPolicy>(new FakeScopedLsaPolicy(this));
}

FakeScopedLsaPolicy::FakeScopedLsaPolicy(FakeScopedLsaPolicyFactory* factory)
    : ScopedLsaPolicy(STANDARD_RIGHTS_READ), factory_(factory) {
  // The base class ctor will fail to initialize because these tests are not
  // running elevated.  That's OK, everything is faked out anyway.
}

FakeScopedLsaPolicy::~FakeScopedLsaPolicy() {}

HRESULT FakeScopedLsaPolicy::StorePrivateData(const wchar_t* key,
                                              const wchar_t* value) {
  private_data()[key] = value;
  return S_OK;
}

HRESULT FakeScopedLsaPolicy::RemovePrivateData(const wchar_t* key) {
  private_data().erase(key);
  return S_OK;
}

HRESULT FakeScopedLsaPolicy::RetrievePrivateData(const wchar_t* key,
                                                 wchar_t* value,
                                                 size_t length) {
  if (private_data().count(key) == 0)
    return E_INVALIDARG;

  errno_t err = wcscpy_s(value, length, private_data()[key].c_str());
  if (err != 0)
    return E_FAIL;

  return S_OK;
}

bool FakeScopedLsaPolicy::PrivateDataExists(const wchar_t* key) {
  return private_data().count(key) != 0;
}

HRESULT FakeScopedLsaPolicy::AddAccountRights(PSID sid, const wchar_t* right) {
  return S_OK;
}

HRESULT FakeScopedLsaPolicy::RemoveAccount(PSID sid) {
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////

FakeScopedUserProfileFactory::FakeScopedUserProfileFactory()
    : original_creator_(*ScopedUserProfile::GetCreatorFunctionStorage()) {
  *ScopedUserProfile::GetCreatorFunctionStorage() = base::BindRepeating(
      &FakeScopedUserProfileFactory::Create, base::Unretained(this));
}

FakeScopedUserProfileFactory::~FakeScopedUserProfileFactory() {
  *ScopedUserProfile::GetCreatorFunctionStorage() = original_creator_;
}

std::unique_ptr<ScopedUserProfile> FakeScopedUserProfileFactory::Create(
    const base::string16& sid,
    const base::string16& domain,
    const base::string16& username,
    const base::string16& password) {
  return std::unique_ptr<ScopedUserProfile>(
      new FakeScopedUserProfile(sid, domain, username, password));
}

FakeScopedUserProfile::FakeScopedUserProfile(const base::string16& sid,
                                             const base::string16& domain,
                                             const base::string16& username,
                                             const base::string16& password) {
  is_valid_ = OSUserManager::Get()->IsWindowsPasswordValid(
                  domain.c_str(), username.c_str(), password.c_str()) == S_OK;
}

FakeScopedUserProfile::~FakeScopedUserProfile() {}

HRESULT FakeScopedUserProfile::SaveAccountInfo(const base::Value& properties) {
  if (!is_valid_)
    return E_INVALIDARG;

  base::string16 sid;
  base::string16 id;
  base::string16 email;
  base::string16 token_handle;
  base::string16 last_successful_online_login_millis;

  HRESULT hr = ExtractAssociationInformation(
      properties, &sid, &id, &email, &token_handle,
      &last_successful_online_login_millis);
  if (FAILED(hr))
    return hr;

  hr = RegisterAssociation(sid, id, email, token_handle,
                           last_successful_online_login_millis);

  if (FAILED(hr))
    return hr;

  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////

FakeWinHttpUrlFetcherFactory::Response::Response() {}

FakeWinHttpUrlFetcherFactory::Response::Response(const Response& rhs)
    : headers(rhs.headers),
      response(rhs.response),
      send_response_event_handle(rhs.send_response_event_handle) {}

FakeWinHttpUrlFetcherFactory::Response::Response(
    const WinHttpUrlFetcher::Headers& new_headers,
    const std::string& new_response,
    HANDLE new_send_response_event_handle)
    : headers(new_headers),
      response(new_response),
      send_response_event_handle(new_send_response_event_handle) {}

FakeWinHttpUrlFetcherFactory::Response::~Response() = default;

FakeWinHttpUrlFetcherFactory::FakeWinHttpUrlFetcherFactory()
    : original_creator_(*WinHttpUrlFetcher::GetCreatorFunctionStorage()) {
  *WinHttpUrlFetcher::GetCreatorFunctionStorage() = base::BindRepeating(
      &FakeWinHttpUrlFetcherFactory::Create, base::Unretained(this));
}

FakeWinHttpUrlFetcherFactory::~FakeWinHttpUrlFetcherFactory() {
  *WinHttpUrlFetcher::GetCreatorFunctionStorage() = original_creator_;
}

void FakeWinHttpUrlFetcherFactory::SetFakeResponse(
    const GURL& url,
    const WinHttpUrlFetcher::Headers& headers,
    const std::string& response,
    HANDLE send_response_event_handle /*=INVALID_HANDLE_VALUE*/) {
  fake_responses_[url] =
      Response(headers, response, send_response_event_handle);
}

void FakeWinHttpUrlFetcherFactory::SetFakeFailedResponse(const GURL& url,
                                                         HRESULT failed_hr) {
  // Make sure that the HRESULT set is a failed attempt.
  DCHECK(FAILED(failed_hr));
  failed_http_fetch_hr_[url] = failed_hr;
}

std::unique_ptr<WinHttpUrlFetcher> FakeWinHttpUrlFetcherFactory::Create(
    const GURL& url) {
  if (fake_responses_.count(url) == 0 && failed_http_fetch_hr_.count(url) == 0)
    return nullptr;

  FakeWinHttpUrlFetcher* fetcher = new FakeWinHttpUrlFetcher(std::move(url));

  if (fake_responses_.count(url) != 0) {
    const Response& response = fake_responses_[url];

    fetcher->response_headers_ = response.headers;
    fetcher->response_ = response.response;
    fetcher->send_response_event_handle_ = response.send_response_event_handle;
  } else {
    DCHECK(failed_http_fetch_hr_.count(url) > 0);
    fetcher->response_hr_ = failed_http_fetch_hr_[url];
  }
  ++requests_created_;

  return std::unique_ptr<WinHttpUrlFetcher>(fetcher);
}

FakeWinHttpUrlFetcher::FakeWinHttpUrlFetcher(const GURL& url)
    : WinHttpUrlFetcher() {}

FakeWinHttpUrlFetcher::~FakeWinHttpUrlFetcher() {}

bool FakeWinHttpUrlFetcher::IsValid() const {
  return true;
}

HRESULT FakeWinHttpUrlFetcher::Fetch(std::vector<char>* response) {
  if (FAILED(response_hr_))
    return response_hr_;

  if (send_response_event_handle_ != INVALID_HANDLE_VALUE)
    ::WaitForSingleObject(send_response_event_handle_, INFINITE);

  response->resize(response_.size());
  memcpy(response->data(), response_.c_str(), response->size());
  return S_OK;
}

HRESULT FakeWinHttpUrlFetcher::Close() {
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////

FakeAssociatedUserValidator::FakeAssociatedUserValidator()
    : AssociatedUserValidator(
          AssociatedUserValidator::kDefaultTokenHandleValidationTimeout),
      original_validator_(*GetInstanceStorage()) {
  *GetInstanceStorage() = this;
}

FakeAssociatedUserValidator::FakeAssociatedUserValidator(
    base::TimeDelta validation_timeout)
    : AssociatedUserValidator(validation_timeout),
      original_validator_(*GetInstanceStorage()) {
  *GetInstanceStorage() = this;
}

FakeAssociatedUserValidator::~FakeAssociatedUserValidator() {
  *GetInstanceStorage() = original_validator_;
}

///////////////////////////////////////////////////////////////////////////////

FakeInternetAvailabilityChecker::FakeInternetAvailabilityChecker(
    HasInternetConnectionCheckType has_internet_connection /*=kHicForceYes*/)
    : original_checker_(*GetInstanceStorage()),
      has_internet_connection_(has_internet_connection) {
  *GetInstanceStorage() = this;
}

FakeInternetAvailabilityChecker::~FakeInternetAvailabilityChecker() {
  *GetInstanceStorage() = original_checker_;
}

bool FakeInternetAvailabilityChecker::HasInternetConnection() {
  return has_internet_connection_ == kHicForceYes;
}

void FakeInternetAvailabilityChecker::SetHasInternetConnection(
    HasInternetConnectionCheckType has_internet_connection) {
  has_internet_connection_ = has_internet_connection;
}

///////////////////////////////////////////////////////////////////////////////

FakePasswordRecoveryManager::FakePasswordRecoveryManager()
    : FakePasswordRecoveryManager(
          PasswordRecoveryManager::
              kDefaultEscrowServiceEncryptionKeyRequestTimeout,
          PasswordRecoveryManager::
              kDefaultEscrowServiceDecryptionKeyRequestTimeout) {}

FakePasswordRecoveryManager::FakePasswordRecoveryManager(
    base::TimeDelta encryption_key_request_timeout,
    base::TimeDelta decryption_key_request_timeout)
    : PasswordRecoveryManager(encryption_key_request_timeout,
                              decryption_key_request_timeout),
      original_validator_(*GetInstanceStorage()) {
  *GetInstanceStorage() = this;
}

FakePasswordRecoveryManager::~FakePasswordRecoveryManager() {
  *GetInstanceStorage() = original_validator_;
}

}  // namespace credential_provider
