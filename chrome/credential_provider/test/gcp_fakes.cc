// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/test/gcp_fakes.h"

#include <windows.h>
#include <lm.h>
#include <sddl.h>

#include <atlconv.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace {

HRESULT CreateArbitrarySid(DWORD subauth0, PSID* sid) {
  SID_IDENTIFIER_AUTHORITY Authority = {SECURITY_NON_UNIQUE_AUTHORITY};
  if (!::AllocateAndInitializeSid(&Authority, 1, subauth0, 0, 0, 0, 0, 0,
                                  0, 0, sid)) {
    return(HRESULT_FROM_WIN32(::GetLastError()));
  }
  return S_OK;
}

}  // namespace

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
  std::unique_ptr<wchar_t, void (*)(void*)>
      cmdline(_wcsdup(command_line.GetCommandLineString().c_str()), std::free);
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
  USES_CONVERSION;

  if (error)
    *error = 0;

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
  username_to_info_.emplace(username,
                            UserInfo(password, fullname, comment, sidstr));
  ::LocalFree(sidstr);

  return S_OK;
}

HRESULT FakeOSUserManager::SetUserPassword(const wchar_t* username,
                                           const wchar_t* password,
                                           DWORD* error) {
  if (error)
    *error = 0;

  if (username_to_info_.count(username) > 0) {
    username_to_info_[username].password = password;
    return S_OK;
  }

  return HRESULT_FROM_WIN32(NERR_UserNotFound);
}

HRESULT FakeOSUserManager::CreateLogonToken(const wchar_t* username,
                                            const wchar_t* password,
                                            bool /*interactive*/,
                                            base::win::ScopedHandle* token) {
  if (username_to_info_.count(username) == 0) {
    return HRESULT_FROM_WIN32(NERR_BadUsername);
  } else if (username_to_info_[username].password != password) {
    return HRESULT_FROM_WIN32(NERR_UserExists);
  }

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

HRESULT FakeOSUserManager::GetUserSID(const wchar_t* username, PSID* sid) {
  if (username_to_info_.count(username) > 0) {
    if (!::ConvertStringSidToSid(username_to_info_[username].sid.c_str(), sid))
      return HRESULT_FROM_WIN32(NERR_ProgNeedsExtraMem);

    return S_OK;
  }

  return HRESULT_FROM_WIN32(NERR_UserNotFound);
}

HRESULT FakeOSUserManager::FindUserBySID(const wchar_t* sid,
                                         wchar_t* username,
                                         DWORD length) {
  for (auto& kv : username_to_info_) {
    if (kv.second.sid == sid) {
      if (username)
        wcscpy_s(username, length, kv.first.c_str());
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

FakeOSUserManager::UserInfo::UserInfo(const wchar_t* password,
                                      const wchar_t* fullname,
                                      const wchar_t* comment,
                                      const wchar_t* sid)
    : password(password), fullname(fullname), comment(comment), sid(sid) {}

FakeOSUserManager::UserInfo::UserInfo() {}

FakeOSUserManager::UserInfo::UserInfo(const UserInfo& other) = default;

FakeOSUserManager::UserInfo::~UserInfo() {}

bool FakeOSUserManager::UserInfo::operator==(const UserInfo& other) const {
  return password == other.password && fullname == other.fullname &&
      comment == other.comment && sid == other.sid;
}

const FakeOSUserManager::UserInfo FakeOSUserManager::GetUserInfo(
    const wchar_t* username) {
  return (username_to_info_.count(username) > 0) ? username_to_info_[username]
                                                 : UserInfo();
}

HRESULT FakeOSUserManager::CreateNewSID(PSID* sid) {
  return CreateArbitrarySid(++next_rid_, sid);
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
    const base::string16& username,
    const base::string16& password) {
  return std::unique_ptr<ScopedUserProfile>(
      new FakeScopedUserProfile(sid, username, password));
}

FakeScopedUserProfile::FakeScopedUserProfile(const base::string16& sid,
                                             const base::string16& username,
                                             const base::string16& password) {}

FakeScopedUserProfile::~FakeScopedUserProfile() {}

///////////////////////////////////////////////////////////////////////////////

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
    const std::string& response) {
  fake_responses_.emplace(url, std::make_pair(headers, response));
}

std::unique_ptr<WinHttpUrlFetcher> FakeWinHttpUrlFetcherFactory::Create(
    const GURL& url) {
  if (fake_responses_.count(url) == 0)
    return nullptr;

  const Response& response = fake_responses_[url];

  FakeWinHttpUrlFetcher* fetcher = new FakeWinHttpUrlFetcher(std::move(url));
  fetcher->response_headers_ = response.first;
  fetcher->response_ = response.second;
  return std::unique_ptr<WinHttpUrlFetcher>(fetcher);
}

FakeWinHttpUrlFetcher::FakeWinHttpUrlFetcher(const GURL& url)
    : WinHttpUrlFetcher() {}

FakeWinHttpUrlFetcher::~FakeWinHttpUrlFetcher() {}

// WinHttpUrlFetcher
bool FakeWinHttpUrlFetcher::IsValid() const {
  return true;
}

HRESULT FakeWinHttpUrlFetcher::Fetch(std::string* response) {
  response->assign(response_);
  return S_OK;
}

HRESULT FakeWinHttpUrlFetcher::Close() {
  return S_OK;
}

}  // namespace credential_provider
