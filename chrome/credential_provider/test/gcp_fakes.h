// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_TEST_GCP_FAKES_H_
#define CHROME_CREDENTIAL_PROVIDER_TEST_GCP_FAKES_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/scoped_handle.h"
#include "chrome/credential_provider/gaiacp/associated_user_validator.h"
#include "chrome/credential_provider/gaiacp/internet_availability_checker.h"
#include "chrome/credential_provider/gaiacp/os_process_manager.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/password_recovery_manager.h"
#include "chrome/credential_provider/gaiacp/scoped_lsa_policy.h"
#include "chrome/credential_provider/gaiacp/scoped_user_profile.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

namespace base {
class WaitableEvent;
}

namespace credential_provider {

void InitializeRegistryOverrideForTesting(
    registry_util::RegistryOverrideManager* registry_override);

///////////////////////////////////////////////////////////////////////////////

class FakeOSProcessManager : public OSProcessManager {
 public:
  FakeOSProcessManager();
  ~FakeOSProcessManager() override;

  // OSProcessManager
  HRESULT GetTokenLogonSID(const base::win::ScopedHandle& token,
                           PSID* sid) override;
  HRESULT SetupPermissionsForLogonSid(PSID sid) override;
  HRESULT CreateProcessWithToken(
      const base::win::ScopedHandle& logon_token,
      const base::CommandLine& command_line,
      _STARTUPINFOW* startupinfo,
      base::win::ScopedProcessInformation* procinfo) override;

 private:
  OSProcessManager* original_manager_;
  DWORD next_rid_ = 0;
};

///////////////////////////////////////////////////////////////////////////////

class FakeOSUserManager : public OSUserManager {
 public:
  FakeOSUserManager();
  ~FakeOSUserManager() override;

  // OSUserManager
  HRESULT GenerateRandomPassword(wchar_t* password, int length) override;
  HRESULT AddUser(const wchar_t* username,
                  const wchar_t* password,
                  const wchar_t* fullname,
                  const wchar_t* comment,
                  bool add_to_users_group,
                  BSTR* sid,
                  DWORD* error) override;
  // Add a user to the OS with domain associated with it.
  HRESULT AddUser(const wchar_t* username,
                  const wchar_t* password,
                  const wchar_t* fullname,
                  const wchar_t* comment,
                  bool add_to_users_group,
                  const wchar_t* domain,
                  BSTR* sid,
                  DWORD* error);
  HRESULT ChangeUserPassword(const wchar_t* domain,
                             const wchar_t* username,
                             const wchar_t* password,
                             const wchar_t* old_password) override;
  HRESULT SetUserPassword(const wchar_t* domain,
                          const wchar_t* username,
                          const wchar_t* password) override;
  HRESULT SetUserFullname(const wchar_t* domain,
                          const wchar_t* username,
                          const wchar_t* full_name) override;
  HRESULT IsWindowsPasswordValid(const wchar_t* domain,
                                 const wchar_t* username,
                                 const wchar_t* password) override;

  HRESULT CreateLogonToken(const wchar_t* domain,
                           const wchar_t* username,
                           const wchar_t* password,
                           bool interactive,
                           base::win::ScopedHandle* token) override;
  HRESULT GetUserSID(const wchar_t* domain,
                     const wchar_t* username,
                     PSID* sid) override;
  HRESULT FindUserBySID(const wchar_t* sid,
                        wchar_t* username,
                        DWORD username_size,
                        wchar_t* domain,
                        DWORD domain_size) override;
  HRESULT RemoveUser(const wchar_t* username, const wchar_t* password) override;

  HRESULT GetUserFullname(const wchar_t* domain,
                          const wchar_t* username,
                          base::string16* fullname) override;

  HRESULT ModifyUserAccessWithLogonHours(const wchar_t* domain,
                                         const wchar_t* username,
                                         bool allow) override;

  bool IsDeviceDomainJoined() override;

  void SetShouldFailUserCreation(bool should_fail) {
    should_fail_user_creation_ = should_fail;
  }

  void SetIsDeviceDomainJoined(bool is_device_domain_joined) {
    is_device_domain_joined_ = is_device_domain_joined;
  }

  struct UserInfo {
    UserInfo(const wchar_t* domain,
             const wchar_t* password,
             const wchar_t* fullname,
             const wchar_t* comment,
             const wchar_t* sid);
    UserInfo();
    UserInfo(const UserInfo& other);
    ~UserInfo();

    bool operator==(const UserInfo& other) const;

    base::string16 domain;
    base::string16 password;
    base::string16 fullname;
    base::string16 comment;
    base::string16 sid;
  };
  const UserInfo GetUserInfo(const wchar_t* username);

  // Creates a new unique sid.  Free returned sid with FreeSid().
  HRESULT CreateNewSID(PSID* sid);

  // Creates a fake user with the given |username|, |password|, |fullname|,
  // |comment|. If |gaia_id| is non-empty, also associates the user with
  // the given gaia id. If |email| is non-empty, sets the email to use for
  // reauth to be this one.
  // |sid| is allocated and filled with the SID of the new user.
  HRESULT CreateTestOSUser(const base::string16& username,
                           const base::string16& password,
                           const base::string16& fullname,
                           const base::string16& comment,
                           const base::string16& gaia_id,
                           const base::string16& email,
                           BSTR* sid);

  // Creates a fake user with the given |username|, |password|, |fullname|,
  // |comment| and |domain|. If |gaia_id| is non-empty, also associates the
  // user with the given gaia id. If |email| is non-empty, sets the email to
  // use for reauth to be this one.
  // |sid| is allocated and filled with the SID of the new user.
  HRESULT CreateTestOSUser(const base::string16& username,
                           const base::string16& password,
                           const base::string16& fullname,
                           const base::string16& comment,
                           const base::string16& gaia_id,
                           const base::string16& email,
                           const base::string16& domain,
                           BSTR* sid);

  size_t GetUserCount() const { return username_to_info_.size(); }
  std::vector<std::pair<base::string16, base::string16>> GetUsers() const;

 private:
  OSUserManager* original_manager_;
  DWORD next_rid_ = 0;
  std::map<base::string16, UserInfo> username_to_info_;
  bool should_fail_user_creation_ = false;
  bool is_device_domain_joined_ = false;
};

///////////////////////////////////////////////////////////////////////////////

class FakeScopedLsaPolicyFactory {
 public:
  FakeScopedLsaPolicyFactory();
  virtual ~FakeScopedLsaPolicyFactory();

  ScopedLsaPolicy::CreatorCallback GetCreatorCallback();

  // PrivateDataMap is a string-to-string key/value store that maps private
  // names to their corresponding data strings.  The term "private" here is
  // used to reflect the name of the underlying OS calls.  This data is meant
  // to be shared by all ScopedLsaPolicy instances created by this factory.
  using PrivateDataMap = std::map<base::string16, base::string16>;
  PrivateDataMap& private_data() { return private_data_; }

 private:
  std::unique_ptr<ScopedLsaPolicy> Create(ACCESS_MASK mask);

  ScopedLsaPolicy::CreatorCallback original_creator_;
  PrivateDataMap private_data_;
};

class FakeScopedLsaPolicy : public ScopedLsaPolicy {
 public:
  ~FakeScopedLsaPolicy() override;

  // ScopedLsaPolicy
  HRESULT StorePrivateData(const wchar_t* key, const wchar_t* value) override;
  HRESULT RemovePrivateData(const wchar_t* key) override;
  HRESULT RetrievePrivateData(const wchar_t* key,
                              wchar_t* value,
                              size_t length) override;
  bool PrivateDataExists(const wchar_t* key) override;
  HRESULT AddAccountRights(PSID sid, const wchar_t* right) override;
  HRESULT RemoveAccount(PSID sid) override;

 private:
  friend class FakeScopedLsaPolicyFactory;

  explicit FakeScopedLsaPolicy(FakeScopedLsaPolicyFactory* factory);

  FakeScopedLsaPolicyFactory::PrivateDataMap& private_data() {
    return factory_->private_data();
  }

  FakeScopedLsaPolicyFactory* factory_;
};

///////////////////////////////////////////////////////////////////////////////

// A scoped FakeScopedUserProfile factory.  Installs itself when constructed
// and removes itself when deleted.
class FakeScopedUserProfileFactory {
 public:
  FakeScopedUserProfileFactory();
  virtual ~FakeScopedUserProfileFactory();

 private:
  std::unique_ptr<ScopedUserProfile> Create(const base::string16& sid,
                                            const base::string16& domain,
                                            const base::string16& username,
                                            const base::string16& password);

  ScopedUserProfile::CreatorCallback original_creator_;
};

class FakeScopedUserProfile : public ScopedUserProfile {
 public:
  HRESULT SaveAccountInfo(const base::Value& properties) override;

 private:
  friend class FakeScopedUserProfileFactory;

  FakeScopedUserProfile(const base::string16& sid,
                        const base::string16& domain,
                        const base::string16& username,
                        const base::string16& password);
  ~FakeScopedUserProfile() override;

  bool is_valid_ = false;
};

///////////////////////////////////////////////////////////////////////////////

// A scoped FakeWinHttpUrlFetcher factory.  Installs itself when constructed
// and removes itself when deleted.
class FakeWinHttpUrlFetcherFactory {
 public:
  FakeWinHttpUrlFetcherFactory();
  ~FakeWinHttpUrlFetcherFactory();

  void SetFakeResponse(
      const GURL& url,
      const WinHttpUrlFetcher::Headers& headers,
      const std::string& response,
      HANDLE send_response_event_handle = INVALID_HANDLE_VALUE);

  // Sets the response as a failed http attempt. The return result
  // from http_url_fetcher.Fetch() would be set as the input HRESULT
  // to this method.
  void SetFakeFailedResponse(const GURL& url, HRESULT failed_hr);

  size_t requests_created() const { return requests_created_; }

 private:
  std::unique_ptr<WinHttpUrlFetcher> Create(const GURL& url);

  WinHttpUrlFetcher::CreatorCallback original_creator_;

  struct Response {
    Response();
    Response(const Response& rhs);
    Response(const WinHttpUrlFetcher::Headers& new_headers,
             const std::string& new_response,
             HANDLE new_send_response_event_handle);
    ~Response();
    WinHttpUrlFetcher::Headers headers;
    std::string response;
    HANDLE send_response_event_handle;
  };

  std::map<GURL, Response> fake_responses_;
  std::map<GURL, HRESULT> failed_http_fetch_hr_;
  size_t requests_created_ = 0;
};

class FakeWinHttpUrlFetcher : public WinHttpUrlFetcher {
 public:
  explicit FakeWinHttpUrlFetcher(const GURL& url);
  ~FakeWinHttpUrlFetcher() override;

  using WinHttpUrlFetcher::Headers;

  const Headers& response_headers() const { return response_headers_; }

  // WinHttpUrlFetcher
  bool IsValid() const override;
  HRESULT Fetch(std::vector<char>* response) override;
  HRESULT Close() override;

 private:
  friend FakeWinHttpUrlFetcherFactory;

  Headers response_headers_;
  std::string response_;
  HANDLE send_response_event_handle_;
  HRESULT response_hr_ = S_OK;
};

///////////////////////////////////////////////////////////////////////////////

class FakeAssociatedUserValidator : public AssociatedUserValidator {
 public:
  FakeAssociatedUserValidator();
  explicit FakeAssociatedUserValidator(base::TimeDelta validation_timeout);
  ~FakeAssociatedUserValidator() override;

  using AssociatedUserValidator::ForceRefreshTokenHandlesForTesting;
  using AssociatedUserValidator::IsUserAccessBlockedForTesting;

 private:
  AssociatedUserValidator* original_validator_ = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

class FakeInternetAvailabilityChecker : public InternetAvailabilityChecker {
 public:
  enum HasInternetConnectionCheckType { kHicForceYes, kHicForceNo };

  FakeInternetAvailabilityChecker(
      HasInternetConnectionCheckType has_internet_connection = kHicForceYes);
  ~FakeInternetAvailabilityChecker() override;

  bool HasInternetConnection() override;
  void SetHasInternetConnection(
      HasInternetConnectionCheckType has_internet_connection);

 private:
  InternetAvailabilityChecker* original_checker_ = nullptr;

  // Used during tests to force the credential provider to believe if an
  // internet connection is possible or not.  In production the value is
  // always set to HIC_CHECK_ALWAYS to perform a real check at runtime.
  HasInternetConnectionCheckType has_internet_connection_ = kHicForceYes;
};

///////////////////////////////////////////////////////////////////////////////

class FakePasswordRecoveryManager : public PasswordRecoveryManager {
 public:
  FakePasswordRecoveryManager();
  explicit FakePasswordRecoveryManager(
      base::TimeDelta encryption_key_request_timeout,
      base::TimeDelta decryption_key_request_timeout);
  ~FakePasswordRecoveryManager() override;

  using PasswordRecoveryManager::MakeGenerateKeyPairResponseForTesting;
  using PasswordRecoveryManager::MakeGetPrivateKeyResponseForTesting;
  using PasswordRecoveryManager::SetRequestTimeoutForTesting;

 private:
  PasswordRecoveryManager* original_validator_ = nullptr;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_TEST_GCP_FAKES_H_
