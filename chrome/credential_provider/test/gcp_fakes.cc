// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/credential_provider/test/gcp_fakes.h"

#include <windows.h>

#include <atlcomcli.h>
#include <atlconv.h>
#include <lm.h>
#include <ntsecapi.h>
#include <ntstatus.h>
#include <process.h>
#include <sddl.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/extension/extension_strings.h"
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
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kRegDisablePasswordSync, 1));
  DWORD disable_cloud_association = 0;
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"enable_cloud_association",
                                          disable_cloud_association));
  ASSERT_EQ(ERROR_SUCCESS,
            key.WriteValue(L"domains_allowed_to_login", L"test.com,gmail.com"));
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
  if (!token.IsValid()) {
    return E_INVALIDARG;
  }

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
  if (length < kMinPasswordLength) {
    return E_INVALIDARG;
  }

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

  if (error) {
    *error = 0;
  }

  if (failure_reasons_.find(FAILEDOPERATIONS::ADD_USER) !=
      failure_reasons_.end()) {
    return failure_reasons_[FAILEDOPERATIONS::ADD_USER];
  }

  // Username or password cannot be empty.
  if (username == nullptr || !username[0] || password == nullptr ||
      !password[0]) {
    return E_FAIL;
  }

  bool user_found = username_to_info_.count(username) > 0;

  if (user_found) {
    *sid = ::SysAllocString(W2COLE(username_to_info_[username].sid.c_str()));
    return HRESULT_FROM_WIN32(NERR_UserExists);
  }

  PSID psid = nullptr;
  HRESULT hr = CreateNewSID(&psid);
  if (FAILED(hr)) {
    return hr;
  }

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

  if (failure_reasons_.find(FAILEDOPERATIONS::CHANGE_PASSWORD) !=
      failure_reasons_.end()) {
    return failure_reasons_[FAILEDOPERATIONS::CHANGE_PASSWORD];
  }

  if (username_to_info_.count(username) > 0) {
    if (username_to_info_[username].password != old_password) {
      return HRESULT_FROM_WIN32(ERROR_INVALID_PASSWORD);
    }

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

  if (failure_reasons_.find(FAILEDOPERATIONS::SET_USER_FULLNAME) !=
      failure_reasons_.end()) {
    return failure_reasons_[FAILEDOPERATIONS::SET_USER_FULLNAME];
  }

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
    if (info.domain != domain) {
      return HRESULT_FROM_WIN32(NERR_UserNotFound);
    }

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
  if (info.domain != domain) {
    return HRESULT_FROM_WIN32(NERR_BadUsername);
  }

  // Create a token with a dummy handle value.
  base::FilePath path;
  if (!base::CreateTemporaryFile(&path)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

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
      if (!::ConvertStringSidToSid(info.sid.c_str(), sid)) {
        return HRESULT_FROM_WIN32(NERR_ProgNeedsExtraMem);
      }

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
  auto it = to_be_failed_find_user_sids_.find(sid);
  if (it != to_be_failed_find_user_sids_.end()) {
    to_be_failed_find_user_sids_[sid]--;
    if (to_be_failed_find_user_sids_[sid] == 0) {
      to_be_failed_find_user_sids_.erase(it);
    }

    return E_FAIL;
  }

  for (auto& kv : username_to_info_) {
    if (kv.second.sid == sid) {
      if (username) {
        wcscpy_s(username, username_size, kv.first.c_str());
      }
      if (domain) {
        wcscpy_s(domain, domain_size, kv.second.domain.c_str());
      }
      return S_OK;
    }
  }

  return HRESULT_FROM_WIN32(ERROR_NONE_MAPPED);
}

void FakeOSUserManager::FailFindUserBySID(const wchar_t* sid,
                                          int number_of_failures) {
  to_be_failed_find_user_sids_[sid] = number_of_failures;
}

HRESULT FakeOSUserManager::RemoveUser(const wchar_t* username,
                                      const wchar_t* password) {
  username_to_info_.erase(username);
  return S_OK;
}

HRESULT FakeOSUserManager::GetUserFullname(const wchar_t* domain,
                                           const wchar_t* username,
                                           std::wstring* fullname) {
  DCHECK(domain);
  DCHECK(username);
  DCHECK(fullname);

  if (failure_reasons_.find(FAILEDOPERATIONS::GET_USER_FULLNAME) !=
      failure_reasons_.end()) {
    return failure_reasons_[FAILEDOPERATIONS::GET_USER_FULLNAME];
  }

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

HRESULT FakeOSUserManager::SetDefaultPasswordChangePolicies(
    const wchar_t* domain,
    const wchar_t* username) {
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
HRESULT FakeOSUserManager::CreateTestOSUser(const std::wstring& username,
                                            const std::wstring& password,
                                            const std::wstring& fullname,
                                            const std::wstring& comment,
                                            const std::wstring& gaia_id,
                                            const std::wstring& email,
                                            BSTR* sid) {
  return CreateTestOSUser(username, password, fullname, comment, gaia_id, email,
                          OSUserManager::GetLocalDomain(), sid);
}

HRESULT FakeOSUserManager::CreateTestOSUser(const std::wstring& username,
                                            const std::wstring& password,
                                            const std::wstring& fullname,
                                            const std::wstring& comment,
                                            const std::wstring& gaia_id,
                                            const std::wstring& email,
                                            const std::wstring& domain,
                                            BSTR* sid) {
  DWORD error;
  HRESULT hr = AddUser(username.c_str(), password.c_str(), fullname.c_str(),
                       comment.c_str(), true, domain.c_str(), sid, &error);
  if (FAILED(hr)) {
    return hr;
  }

  if (!gaia_id.empty()) {
    hr = SetUserProperty(OLE2CW(*sid), kUserId, gaia_id);
    if (FAILED(hr)) {
      return hr;
    }

    hr = SetUserProperty(OLE2CW(*sid), kUserTokenHandle, L"token_handle");
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!email.empty()) {
    hr = SetUserProperty(OLE2CW(*sid), kUserEmail, email);
    if (FAILED(hr)) {
      return hr;
    }
  }

  return S_OK;
}

std::vector<std::pair<std::wstring, std::wstring>> FakeOSUserManager::GetUsers()
    const {
  std::vector<std::pair<std::wstring, std::wstring>> users;

  for (auto& kv : username_to_info_) {
    users.emplace_back(std::make_pair(kv.second.sid, kv.first));
  }

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
  if (private_data().count(key) == 0) {
    if (wcscmp(key, kLsaKeyGaiaSid) == 0) {
      return HRESULT_FROM_NT(STATUS_OBJECT_NAME_NOT_FOUND);
    } else {
      return E_INVALIDARG;
    }
  }

  errno_t err = wcscpy_s(value, length, private_data()[key].c_str());
  if (err != 0) {
    return E_FAIL;
  }

  return S_OK;
}

bool FakeScopedLsaPolicy::PrivateDataExists(const wchar_t* key) {
  return private_data().count(key) != 0;
}

HRESULT FakeScopedLsaPolicy::AddAccountRights(
    PSID sid,
    const std::vector<std::wstring>& rights) {
  return S_OK;
}

HRESULT FakeScopedLsaPolicy::RemoveAccountRights(
    PSID sid,
    const std::vector<std::wstring>& rights) {
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
    const std::wstring& sid,
    const std::wstring& domain,
    const std::wstring& username,
    const std::wstring& password) {
  return std::unique_ptr<ScopedUserProfile>(
      new FakeScopedUserProfile(sid, domain, username, password));
}

FakeScopedUserProfile::FakeScopedUserProfile(const std::wstring& sid,
                                             const std::wstring& domain,
                                             const std::wstring& username,
                                             const std::wstring& password) {
  is_valid_ = OSUserManager::Get()->IsWindowsPasswordValid(
                  domain.c_str(), username.c_str(), password.c_str()) == S_OK;
}

FakeScopedUserProfile::~FakeScopedUserProfile() {}

HRESULT FakeScopedUserProfile::SaveAccountInfo(
    const base::Value::Dict& properties) {
  if (!is_valid_) {
    return E_INVALIDARG;
  }

  std::wstring sid;
  std::wstring id;
  std::wstring email;
  std::wstring token_handle;
  std::wstring last_successful_online_login_millis;

  HRESULT hr = ExtractAssociationInformation(properties, &sid, &id, &email,
                                             &token_handle);
  if (FAILED(hr)) {
    return hr;
  }

  hr = RegisterAssociation(sid, id, email, token_handle,
                           last_successful_online_login_millis);

  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////

FakeWinHttpUrlFetcherFactory::RequestData::RequestData()
    : timeout_in_millis(-1) {}  // Set default timeout to an invalid value.

FakeWinHttpUrlFetcherFactory::RequestData::RequestData(const RequestData& rhs)
    : headers(rhs.headers),
      body(rhs.body),
      timeout_in_millis(rhs.timeout_in_millis) {}

FakeWinHttpUrlFetcherFactory::RequestData::~RequestData() = default;

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
  fake_creator_ = base::BindRepeating(&FakeWinHttpUrlFetcherFactory::Create,
                                      base::Unretained(this));
  *WinHttpUrlFetcher::GetCreatorFunctionStorage() = fake_creator_;
}

FakeWinHttpUrlFetcherFactory::~FakeWinHttpUrlFetcherFactory() {
  *WinHttpUrlFetcher::GetCreatorFunctionStorage() = original_creator_;
}

WinHttpUrlFetcher::CreatorCallback
FakeWinHttpUrlFetcherFactory::GetCreatorCallback() {
  return fake_creator_;
}

void FakeWinHttpUrlFetcherFactory::SetFakeResponse(
    const GURL& url,
    const WinHttpUrlFetcher::Headers& headers,
    const std::string& response,
    HANDLE send_response_event_handle /*=INVALID_HANDLE_VALUE*/) {
  fake_responses_[url].clear();
  fake_responses_[url].push_back(
      Response(headers, response, send_response_event_handle));
  remove_fake_response_when_created_ = false;
}

void FakeWinHttpUrlFetcherFactory::SetFakeResponseForSpecifiedNumRequests(
    const GURL& url,
    const WinHttpUrlFetcher::Headers& headers,
    const std::string& response,
    unsigned int num_requests,
    HANDLE send_response_event_handle /* =INVALID_HANDLE_VALUE */) {
  if (fake_responses_.find(url) == fake_responses_.end()) {
    fake_responses_[url] = std::deque<Response>();
  }
  for (unsigned int i = 0; i < num_requests; ++i) {
    fake_responses_[url].push_back(
        Response(headers, response, send_response_event_handle));
  }
  remove_fake_response_when_created_ = true;
}

void FakeWinHttpUrlFetcherFactory::SetFakeFailedResponse(const GURL& url,
                                                         HRESULT failed_hr) {
  // Make sure that the HRESULT set is a failed attempt.
  DCHECK(FAILED(failed_hr));
  failed_http_fetch_hr_[url] = failed_hr;
}

FakeWinHttpUrlFetcherFactory::RequestData
FakeWinHttpUrlFetcherFactory::GetRequestData(size_t request_index) const {
  if (request_index < requests_data_.size()) {
    return requests_data_[request_index];
  }
  return RequestData();
}

std::unique_ptr<WinHttpUrlFetcher> FakeWinHttpUrlFetcherFactory::Create(
    const GURL& url) {
  if (fake_responses_.count(url) == 0 &&
      failed_http_fetch_hr_.count(url) == 0) {
    return nullptr;
  }

  FakeWinHttpUrlFetcher* fetcher = new FakeWinHttpUrlFetcher(url);

  if (fake_responses_.count(url) != 0) {
    const Response& response = fake_responses_[url].front();

    fetcher->response_headers_ = response.headers;
    fetcher->response_ = response.response;
    fetcher->send_response_event_handle_ = response.send_response_event_handle;

    if (remove_fake_response_when_created_) {
      fake_responses_[url].pop_front();
      if (fake_responses_[url].empty()) {
        fake_responses_.erase(url);
      }
    }
  } else {
    DCHECK(failed_http_fetch_hr_.count(url) > 0);
    fetcher->response_hr_ = failed_http_fetch_hr_[url];
  }

  if (collect_request_data_) {
    requests_data_.push_back(RequestData());
    fetcher->request_data_ = &requests_data_.back();
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
  if (FAILED(response_hr_)) {
    return response_hr_;
  }

  if (send_response_event_handle_ != INVALID_HANDLE_VALUE) {
    ::WaitForSingleObject(send_response_event_handle_, INFINITE);
  }

  response->resize(response_.size());
  memcpy(response->data(), response_.c_str(), response->size());
  return S_OK;
}

HRESULT FakeWinHttpUrlFetcher::Close() {
  return S_OK;
}

HRESULT FakeWinHttpUrlFetcher::SetRequestHeader(const char* name,
                                                const char* value) {
  if (request_data_) {
    request_data_->headers[name] = value;
  }
  return S_OK;
}

HRESULT FakeWinHttpUrlFetcher::SetRequestBody(const char* body) {
  if (request_data_) {
    request_data_->body = body;
  }
  return S_OK;
}

HRESULT FakeWinHttpUrlFetcher::SetHttpRequestTimeout(
    const int timeout_in_millis) {
  if (request_data_) {
    request_data_->timeout_in_millis = timeout_in_millis;
  }
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

FakeChromeAvailabilityChecker::FakeChromeAvailabilityChecker(
    HasSupportedChromeCheckType has_supported_chrome /*=kChromeForceYes*/)
    : original_checker_(*GetInstanceStorage()),
      has_supported_chrome_(has_supported_chrome) {
  *GetInstanceStorage() = this;
}

FakeChromeAvailabilityChecker::~FakeChromeAvailabilityChecker() {
  *GetInstanceStorage() = original_checker_;
}

bool FakeChromeAvailabilityChecker::HasSupportedChromeVersion() {
  if (has_supported_chrome_ == kChromeDontForce) {
    return original_checker_->HasSupportedChromeVersion();
  }
  return has_supported_chrome_ == kChromeForceYes;
}

void FakeChromeAvailabilityChecker::SetHasSupportedChrome(
    HasSupportedChromeCheckType has_supported_chrome) {
  has_supported_chrome_ = has_supported_chrome;
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

///////////////////////////////////////////////////////////////////////////////

FakeGemDeviceDetailsManager::FakeGemDeviceDetailsManager()
    : FakeGemDeviceDetailsManager(
          GemDeviceDetailsManager::kDefaultUploadDeviceDetailsRequestTimeout) {}

FakeGemDeviceDetailsManager::FakeGemDeviceDetailsManager(
    base::TimeDelta upload_device_details_request_timeout)
    : GemDeviceDetailsManager(upload_device_details_request_timeout),
      original_manager_(*GetInstanceStorage()) {
  *GetInstanceStorage() = this;
}

FakeGemDeviceDetailsManager::~FakeGemDeviceDetailsManager() {
  *GetInstanceStorage() = original_manager_;
}

///////////////////////////////////////////////////////////////////////////////

EVT_HANDLE FakeEventLoggingApiManager::EvtQuery(EVT_HANDLE session,
                                                LPCWSTR path,
                                                LPCWSTR query,
                                                DWORD flags) {
  EXPECT_EQ(session, nullptr);  // local session only.
  EXPECT_EQ(path, nullptr);
  DCHECK(query);
  EXPECT_TRUE((flags & EvtQueryChannelPath) > 0);

  query_handle_ = reinterpret_cast<EVT_HANDLE>(&query_handle_);
  last_error_ = ERROR_SUCCESS;
  return query_handle_;
}

EVT_HANDLE FakeEventLoggingApiManager::EvtOpenPublisherMetadata(
    EVT_HANDLE session,
    LPCWSTR publisher_id,
    LPCWSTR log_file_path,
    LCID locale,
    DWORD flags) {
  EXPECT_EQ(session, nullptr);
  EXPECT_EQ(std::wstring(publisher_id), std::wstring(L"GCPW"));
  EXPECT_EQ(log_file_path, nullptr);
  EXPECT_EQ(locale, DWORD(0));  // local locale.
  EXPECT_EQ(flags, DWORD(0));

  publisher_metadata_ = reinterpret_cast<EVT_HANDLE>(&publisher_metadata_);
  last_error_ = ERROR_SUCCESS;
  return publisher_metadata_;
}

EVT_HANDLE FakeEventLoggingApiManager::EvtCreateRenderContext(
    DWORD value_paths_count,
    LPCWSTR* value_paths,
    DWORD flags) {
  EXPECT_TRUE(value_paths_count >= 2);
  DCHECK(value_paths);
  EXPECT_TRUE(std::wstring(value_paths[0]).find(L"EventRecordID") !=
              std::wstring::npos);
  EXPECT_TRUE(std::wstring(value_paths[1]).find(L"TimeCreated") !=
              std::wstring::npos);
  EXPECT_EQ(flags, EvtRenderContextValues);

  render_context_ = reinterpret_cast<EVT_HANDLE>(&render_context_);
  last_error_ = ERROR_SUCCESS;
  return render_context_;
}

BOOL FakeEventLoggingApiManager::EvtNext(EVT_HANDLE result_set,
                                         DWORD events_size,
                                         PEVT_HANDLE events,
                                         DWORD timeout,
                                         DWORD flags,
                                         PDWORD num_returned) {
  EXPECT_EQ(result_set, query_handle_);
  EXPECT_TRUE(events_size > 0);
  DCHECK(events);

  if (next_event_idx_ >= logs_->size()) {
    last_error_ = ERROR_NO_MORE_ITEMS;
    return FALSE;
  }

  *num_returned = 0;
  for (; (next_event_idx_ < logs_->size()) && (*num_returned < events_size);
       ++next_event_idx_) {
    event_handles_.push_back(EVT_HANDLE());
    size_t last_idx = event_handles_.size() - 1;
    event_handles_[last_idx] = &event_handles_[last_idx];

    events[*num_returned] = event_handles_[last_idx];
    handle_to_index_map_[event_handles_[last_idx]] = next_event_idx_;

    (*num_returned)++;
  }

  last_error_ = ERROR_SUCCESS;
  return TRUE;
}

BOOL FakeEventLoggingApiManager::EvtGetQueryInfo(
    EVT_HANDLE query,
    EVT_QUERY_PROPERTY_ID property_id,
    DWORD value_buffer_size,
    PEVT_VARIANT value_buffer,
    PDWORD value_buffer_used) {
  EXPECT_EQ(query, query_handle_);
  EXPECT_TRUE((property_id == EvtQueryStatuses) ||
              (property_id == EvtQueryNames));

  const wchar_t channel_name[] = L"Application";
  const DWORD mem_size = sizeof(channel_name) + sizeof(EVT_VARIANT);
  *value_buffer_used = mem_size;

  if (value_buffer_size == 0) {
    last_error_ = ERROR_INSUFFICIENT_BUFFER;
    return FALSE;
  }

  EXPECT_TRUE(value_buffer_size >= mem_size);
  value_buffer->Count = 1;
  char* addr = reinterpret_cast<char*>(value_buffer) + sizeof(EVT_VARIANT);

  if (property_id == EvtQueryStatuses) {
    value_buffer->UInt32Arr = reinterpret_cast<UINT32*>(addr);
    value_buffer->UInt32Arr[0] = ERROR_SUCCESS;
  } else if (property_id == EvtQueryNames) {
    value_buffer->StringArr = reinterpret_cast<LPWSTR*>(addr);
    memcpy(value_buffer->StringArr, channel_name, sizeof(channel_name));
  }
  last_error_ = ERROR_SUCCESS;
  return TRUE;
}

BOOL FakeEventLoggingApiManager::EvtRender(EVT_HANDLE context,
                                           EVT_HANDLE evt_handle,
                                           DWORD flags,
                                           DWORD buffer_size,
                                           PVOID buffer,
                                           PDWORD buffer_used,
                                           PDWORD property_count) {
  EXPECT_EQ(context, render_context_);
  EXPECT_TRUE(handle_to_index_map_.find(evt_handle) !=
              handle_to_index_map_.end());
  EXPECT_EQ(flags, EvtRenderEventValues);

  size_t idx = handle_to_index_map_.find(evt_handle)->second;
  const size_t num_properties = 2;
  const size_t mem_needed = num_properties * sizeof(EVT_VARIANT);
  *buffer_used = mem_needed;

  if (buffer_size < mem_needed) {
    last_error_ = ERROR_INSUFFICIENT_BUFFER;
    return FALSE;
  }

  EVT_VARIANT* data = reinterpret_cast<EVT_VARIANT*>(buffer);
  data[0].UInt64Val = (*logs_)[idx].event_id;

  // Convert to Windows ticks.
  ULONGLONG timestamp_ticks =
      ((*logs_)[idx].created_ts.seconds + 11644473600LL) * 10000000;
  timestamp_ticks += ((*logs_)[idx].created_ts.nanos / 100);

  data[1].FileTimeVal = timestamp_ticks;
  *property_count = num_properties;
  last_error_ = ERROR_SUCCESS;
  return TRUE;
}

BOOL FakeEventLoggingApiManager::EvtFormatMessage(EVT_HANDLE publisher_metadata,
                                                  EVT_HANDLE event,
                                                  DWORD message_id,
                                                  DWORD value_count,
                                                  PEVT_VARIANT values,
                                                  DWORD flags,
                                                  DWORD buffer_size,
                                                  LPWSTR buffer,
                                                  PDWORD buffer_used) {
  EXPECT_EQ(publisher_metadata, publisher_metadata_);
  EXPECT_TRUE(handle_to_index_map_.find(event) != handle_to_index_map_.end());
  EXPECT_EQ(value_count, DWORD(0));
  EXPECT_EQ(values, nullptr);
  EXPECT_TRUE((flags == EvtFormatMessageEvent) ||
              (flags == EvtFormatMessageLevel));
  DCHECK(buffer_used);

  size_t idx = handle_to_index_map_.find(event)->second;

  std::wstring data;
  if (flags == EvtFormatMessageEvent) {
    data = (*logs_)[idx].data;
  } else if (flags == EvtFormatMessageLevel) {
    switch ((*logs_)[idx].severity_level) {
      case 1:
        data = L"Critical";
        break;
      case 2:
        data = L"Error";
        break;
      case 3:
        data = L"Warning";
        break;
      case 4:
        data = L"Information";
        break;
      case 5:
        data = L"Verbose";
        break;
      default:
        data = L"Unknown";
        break;
    }
  }

  const size_t mem_needed =
      sizeof(std::wstring::value_type) * (data.size() + 1);

  *buffer_used = mem_needed;
  if (buffer_size < mem_needed) {
    last_error_ = ERROR_INSUFFICIENT_BUFFER;
    return FALSE;
  }

  DCHECK(buffer);
  ::memcpy(buffer, data.c_str(),
           data.size() * sizeof(std::wstring::value_type));
  last_error_ = ERROR_SUCCESS;

  return TRUE;
}

BOOL FakeEventLoggingApiManager::EvtClose(EVT_HANDLE handle) {
  DCHECK(handle);
  last_error_ = ERROR_SUCCESS;
  if (handle == &query_handle_) {
    query_handle_ = nullptr;
    return TRUE;
  } else if (handle == &publisher_metadata_) {
    publisher_metadata_ = nullptr;
    return TRUE;
  } else if (handle == &render_context_) {
    render_context_ = nullptr;
    return TRUE;
  }

  if (handle_to_index_map_.find(handle) != handle_to_index_map_.end()) {
    size_t idx = handle_to_index_map_.find(handle)->second;
    event_handles_[idx] = nullptr;
    return TRUE;
  }

  last_error_ = ERROR_INVALID_HANDLE;
  return FALSE;
}

DWORD FakeEventLoggingApiManager::GetLastError() {
  return last_error_;
}

FakeEventLoggingApiManager::FakeEventLoggingApiManager(
    const std::vector<EventLogEntry>& logs)
    : original_manager_(*GetInstanceStorage()),
      logs_(logs),
      query_handle_(nullptr),
      publisher_metadata_(nullptr),
      render_context_(nullptr),
      last_error_(ERROR_SUCCESS),
      next_event_idx_(0) {
  *GetInstanceStorage() = this;
}

FakeEventLoggingApiManager::~FakeEventLoggingApiManager() {
  *GetInstanceStorage() = original_manager_;
  EXPECT_EQ(query_handle_, nullptr);
  EXPECT_EQ(publisher_metadata_, nullptr);
  EXPECT_EQ(render_context_, nullptr);

  for (size_t i = 0; i < event_handles_.size(); ++i) {
    EXPECT_EQ(event_handles_[i], nullptr);
  }
}

FakeEventLogsUploadManager::FakeEventLogsUploadManager(
    const std::vector<EventLogEntry>& logs)
    : original_manager_(*GetInstanceStorage()), api_manager_(logs) {
  *GetInstanceStorage() = this;
}

FakeEventLogsUploadManager::~FakeEventLogsUploadManager() {
  *GetInstanceStorage() = original_manager_;
}

HRESULT FakeEventLogsUploadManager::GetUploadStatus() {
  return upload_status_;
}

uint64_t FakeEventLogsUploadManager::GetNumLogsUploaded() {
  return num_event_logs_uploaded_;
}

///////////////////////////////////////////////////////////////////////////////

FakeUserPoliciesManager::FakeUserPoliciesManager()
    : original_manager_(*GetInstanceStorage()) {
  *GetInstanceStorage() = this;
}

FakeUserPoliciesManager::FakeUserPoliciesManager(bool cloud_policies_enabled)
    : original_manager_(*GetInstanceStorage()) {
  *GetInstanceStorage() = this;
  SetCloudPoliciesEnabledForTesting(cloud_policies_enabled);
}

FakeUserPoliciesManager::~FakeUserPoliciesManager() {
  *GetInstanceStorage() = original_manager_;
}

HRESULT FakeUserPoliciesManager::FetchAndStoreCloudUserPolicies(
    const std::wstring& sid,
    const std::string& access_token) {
  ++num_times_fetch_called_;
  fetch_status_ =
      original_manager_->FetchAndStoreCloudUserPolicies(sid, access_token);
  return fetch_status_;
}

void FakeUserPoliciesManager::SetUserPolicies(const std::wstring& sid,
                                              const UserPolicies& policies) {
  user_policies_[sid] = policies;
  user_policies_stale_[sid] = false;
}

bool FakeUserPoliciesManager::GetUserPolicies(const std::wstring& sid,
                                              UserPolicies* policies) const {
  if (user_policies_.find(sid) != user_policies_.end()) {
    *policies = user_policies_.at(sid);
    return true;
  }

  return false;
}

void FakeUserPoliciesManager::SetUserPolicyStaleOrMissing(
    const std::wstring& sid,
    bool status) {
  user_policies_stale_[sid] = status;
}

bool FakeUserPoliciesManager::IsUserPolicyStaleOrMissing(
    const std::wstring& sid) const {
  if (user_policies_stale_.find(sid) != user_policies_stale_.end()) {
    return user_policies_stale_.at(sid);
  }

  return true;
}

int FakeUserPoliciesManager::GetNumTimesFetchAndStoreCalled() const {
  return num_times_fetch_called_;
}

///////////////////////////////////////////////////////////////////////////////

FakeDevicePoliciesManager::FakeDevicePoliciesManager(
    bool cloud_policies_enabled)
    : original_manager_(*GetInstanceStorage()) {
  *GetInstanceStorage() = this;
  UserPoliciesManager::Get()->SetCloudPoliciesEnabledForTesting(
      cloud_policies_enabled);
}

FakeDevicePoliciesManager::~FakeDevicePoliciesManager() {
  *GetInstanceStorage() = original_manager_;
}

void FakeDevicePoliciesManager::SetDevicePolicies(
    const DevicePolicies& policies) {
  device_policies_ = policies;
}

void FakeDevicePoliciesManager::GetDevicePolicies(
    DevicePolicies* device_policies) {
  *device_policies = device_policies_;
}

///////////////////////////////////////////////////////////////////////////////

FakeGCPWFiles::FakeGCPWFiles() : original_files(*GetInstanceStorage()) {
  *GetInstanceStorage() = this;
}

FakeGCPWFiles::~FakeGCPWFiles() {
  *GetInstanceStorage() = original_files;
}

// Installable files are sanitized for testing due to
// differences between build artifacts file location and the way they are being
// packaged. When tests are running, they are checking the build artifacts which
// doesn't reflect foldering structure within 7zip archive.
std::vector<base::FilePath::StringType>
FakeGCPWFiles::GetEffectiveInstallFiles() {
  auto effective_files = original_files->GetEffectiveInstallFiles();

  std::vector<base::FilePath::StringType> sanitized_files;
  for (auto& install_file : effective_files) {
    size_t found = install_file.find_last_of('\\');
    if (found != std::wstring::npos) {
      sanitized_files.push_back(install_file.substr(found + 1));
    } else {
      sanitized_files.push_back(install_file);
    }
  }

  return sanitized_files;
}

///////////////////////////////////////////////////////////////////////////////

FakeOSServiceManager::FakeOSServiceManager()
    : os_service_manager_(*GetInstanceStorage()) {
  *GetInstanceStorage() = this;
}

FakeOSServiceManager::~FakeOSServiceManager() {
  *GetInstanceStorage() = os_service_manager_;
}

unsigned __stdcall ServiceLauncher(void* service_main) {
  LPSERVICE_MAIN_FUNCTION sm = (LPSERVICE_MAIN_FUNCTION)service_main;
  DWORD flags = 0;
  (*sm)(flags, nullptr);
  return 0;
}

DWORD FakeOSServiceManager::StartServiceCtrlDispatcher(
    LPSERVICE_MAIN_FUNCTION service_main) {
  if (service_lookup_from_name_.find(extension::kGCPWExtensionServiceName) ==
      service_lookup_from_name_.end()) {
    return ERROR_INVALID_DATA;
  }
  LOGFN(INFO);

  uintptr_t wait_thread =
      _beginthreadex(0, 0, ServiceLauncher, (void*)service_main, 0, 0);

  while (true) {
    // Service looks for control requests so that it calls the service's control
    // handler.
    DWORD control_request = GetControlRequestForTesting();
    LOGFN(INFO) << "Received control: " << control_request;

    // This is a custom control to end the service process main when service is
    // supposed to stop.
    if (control_request == 100) {
      break;
    }

    service_lookup_from_name_[extension::kGCPWExtensionServiceName]
        .control_handler_cb_(control_request);
  }
  ::CloseHandle(reinterpret_cast<HANDLE>(wait_thread));

  return ERROR_SUCCESS;
}

DWORD FakeOSServiceManager::RegisterCtrlHandler(
    LPHANDLER_FUNCTION handler_proc,
    SERVICE_STATUS_HANDLE* service_status_handle) {
  if (service_lookup_from_name_.find(extension::kGCPWExtensionServiceName) ==
      service_lookup_from_name_.end()) {
    return ERROR_SERVICE_DOES_NOT_EXIST;
  }

  service_lookup_from_name_[extension::kGCPWExtensionServiceName]
      .control_handler_cb_ = handler_proc;
  // Set some random integer here. Not needed in the tests.
  *service_status_handle = (SERVICE_STATUS_HANDLE)1;

  return ERROR_SUCCESS;
}

DWORD FakeOSServiceManager::SetServiceStatus(
    SERVICE_STATUS_HANDLE service_status_handle,
    SERVICE_STATUS service) {
  LOGFN(INFO) << "Service state: " << service.dwCurrentState;
  if (service_lookup_from_name_.find(extension::kGCPWExtensionServiceName) ==
      service_lookup_from_name_.end()) {
    return ERROR_SERVICE_DOES_NOT_EXIST;
  }
  service_lookup_from_name_[extension::kGCPWExtensionServiceName]
      .service_status_ = service;

  if (service.dwCurrentState == SERVICE_STOPPED) {
    SendControlRequestForTesting(100);
  }
  return ERROR_SUCCESS;
}

DWORD FakeOSServiceManager::InstallService(
    const base::FilePath& service_binary_path,
    extension::ScopedScHandle* sc_handle) {
  LOGFN(INFO);

  service_lookup_from_name_[extension::kGCPWExtensionServiceName]
      .service_status_.dwCurrentState = SERVICE_STOPPED;
  return ERROR_SUCCESS;
}

DWORD FakeOSServiceManager::GetServiceStatus(SERVICE_STATUS* service_status) {
  LOGFN(INFO);
  if (service_lookup_from_name_.find(extension::kGCPWExtensionServiceName) ==
      service_lookup_from_name_.end()) {
    return ERROR_SERVICE_DOES_NOT_EXIST;
  }
  *service_status =
      service_lookup_from_name_[extension::kGCPWExtensionServiceName]
          .service_status_;
  return ERROR_SUCCESS;
}

DWORD FakeOSServiceManager::DeleteService() {
  service_lookup_from_name_.erase(extension::kGCPWExtensionServiceName);
  return ERROR_SUCCESS;
}

DWORD FakeOSServiceManager::ChangeServiceConfig(DWORD dwServiceType,
                                                DWORD dwStartType,
                                                DWORD dwErrorControl) {
  return ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////

FakeTaskManager::FakeTaskManager() : task_manager_(*GetInstanceStorage()) {
  *GetInstanceStorage() = this;
}

FakeTaskManager::~FakeTaskManager() {
  *GetInstanceStorage() = task_manager_;
}

void FakeTaskManager::ExecuteTask(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const std::string& task_name) {
  num_of_times_executed_[task_name]++;

  TaskManager::ExecuteTask(task_runner, task_name);
}

///////////////////////////////////////////////////////////////////////////////

FakeTokenGenerator::FakeTokenGenerator()
    : token_generator_(*GetInstanceStorage()) {
  *GetInstanceStorage() = this;
}

FakeTokenGenerator::~FakeTokenGenerator() {
  *GetInstanceStorage() = token_generator_;
}

std::string FakeTokenGenerator::GenerateToken() {
  auto token = test_tokens_.front();
  test_tokens_.erase(test_tokens_.begin());
  return token;
}

void FakeTokenGenerator::SetTokensForTesting(
    const std::vector<std::string>& test_tokens) {
  test_tokens_ = test_tokens;
}

}  // namespace credential_provider
