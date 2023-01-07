// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_ASSOCIATED_USER_VALIDATOR_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_ASSOCIATED_USER_VALIDATOR_H_

#include <credentialprovider.h>

#include <map>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"

#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"

namespace credential_provider {

// Caches the current validity of token handles and updates the validity if
// it is older than a specified validity lifetime.
// NOTE: This class is thread safe.
//
// The following functions are called at a time when it is impossible for
// the validator to be accessed by multiple threads. The validator will only
// be accessed from another thread through the BackgroundTokenHandleUpdater
// that is created in CGaiaCredentialProvider::Advise and destroyed in
// CGaiaCredentialProvider::Unadvise:
// StartRefreshingTokenHandleValidity: Only called on the main thread during
// a call to DllGetClassObject.
// IsUserAccessBlockingEnforced: Only called on the main thread in
//   CGaiaCredentialProvider::Advise and in
//   CGaiaCredentialProviderFilter::UpdateRemoteCredential.
// AllowSigninForUsersWithInvalidTokenHandles: Only called on the main thread
//   in CGaiaCredentialProvider::FinalRelease.
// AllowSigninForAllAssociatedUsers: Only called on the main thread in
//   CGaiaCredentialProviderFilter::Filter.
//
// The following functions can be called while the validator can be accessed
// from another thread:
// IsAuthEnforcedForUser: Called on the main thread indirectly in
// CGaiaCredentialProvider::GetCredentialCount. Also called on the update
// thread while checking DenySigninForUsersWithInvalidTokenHandles.
// GetAssociatedUsersCount: Only called on the main thread indirectly in
// CGaiaCredentialProvider::GetCredentialCount.
// RestoreUserAccess: Only called on the main thread in
// CGaiaCredentialBase::HandleAutologon.
//
// Finally the one function that can be called on the update thread is
// DenySigninForUsersWithInvalidTokenHandles. If this function returns
// true, it will queue a credential update which will only be executed
// on the main thread. The update thread will then be dormant for
// |kTokenHandleValidityLifetime| seconds and in this time the expected
// update of the credentials on the main thread via a call to
// CGaiaCredentialProvider::GetCredentialCount should be able to complete
// before a new update is requested on the update thread. This timing will
// protect the two functions IsAuthEnforcedForUser and
// GetAssociatedUsersCount from being called by multiple threads at the same
// time.
class AssociatedUserValidator {
 public:
  // Prevent update of user access through the call to
  // DenySigninForUsersWithInvalidTokenHandles. This will be used to prevent
  // locking out users that are in the process of signing in.
  class ScopedBlockDenyAccessUpdate {
   public:
    explicit ScopedBlockDenyAccessUpdate(AssociatedUserValidator* validator);
    ~ScopedBlockDenyAccessUpdate();

   private:
    raw_ptr<AssociatedUserValidator> validator_;
  };
  // Default timeout when querying token info for token handles. If a timeout
  // occurs the token handle is assumed to be valid.
  static const base::TimeDelta kDefaultTokenHandleValidationTimeout;

  // Minimum time between token handle info refreshes. When trying to get token
  // info, if the info is older than this value, a new token info query will
  // be made.
  static const base::TimeDelta kTokenHandleValidityLifetime;

  // Default URL used to fetch token info for token handles.
  static const char kTokenInfoUrl[];

  static AssociatedUserValidator* Get();

  // Get all the token handles for all associated users and start queries
  // for their validity. The queries are fired in separate threads but
  // no wait is done for the result. This allows background processing of
  // the queries until they are actually needed. An eventual call to
  // IsAuthEnforcedForUser will cause the wait for the result as needed.
  void StartRefreshingTokenHandleValidity();

  // Checks whether the token handle for the given user is valid or not.
  // This function is blocking and may fire off a query for a token handle that
  // needs to complete before the function returns.
  bool IsAuthEnforcedForUser(const std::wstring& sid);

  enum EnforceAuthReason {
    NOT_ENFORCED = 0,
    NOT_ENROLLED_WITH_MDM,
    MISSING_PASSWORD_RECOVERY_INFO,
    INVALID_TOKEN_HANDLE,
    ONLINE_LOGIN_STALE,
    UPLOAD_DEVICE_DETAILS_FAILED,
    ONLINE_LOGIN_ENFORCED,
    MISSING_OR_STALE_USER_POLICIES
  };

  // Returns the reason for enforcing authentication for the provided |sid|.
  // This function is blocking and may fire off a query for a token handle that
  // needs to complete before the function returns.
  EnforceAuthReason GetAuthEnforceReason(const std::wstring& sid);

  // Checks if user access blocking is enforced given the usage scenario (and
  // other registry based checks).
  bool IsUserAccessBlockingEnforced(
      CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus) const;

  // Goes through all reauth creds found and denies their access to sign
  // in to the system based on the auth reason being not enforced. Returns true
  // if a user has just been denied signin access.
  bool DenySigninForUsersWithInvalidTokenHandles(
      CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
      const std::vector<std::wstring>& reauth_sids);

  // Restores the access for a user that was denied access (if applicable).
  // Returns S_OK on success, failure otherwise.
  HRESULT RestoreUserAccess(const std::wstring& sid);

  // Allows access for all users that have had their access denied by this
  // token validator.
  void AllowSigninForUsersWithInvalidTokenHandles();

  // Restores access to all associated users. Regardless of their access
  // state. This ensures that no user can be completely locked out due
  // a bad computer state or crash.
  void AllowSigninForAllAssociatedUsers(
      CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus);

  // Gets the updated count of valid associated users that exist on this system.
  size_t GetAssociatedUsersCount();

  // Returns whether the user should be locked out of sign in (only used in
  // tests).
  bool IsDenyAccessUpdateBlocked() const;

  bool HasInternetConnection() const;

  // Checks for the staleness of the last successful GCPW login for the input
  // user.
  bool IsOnlineLoginStale(const std::wstring& sid) const;

  // Keeps the sid mapping in gcpw registry up to date with the latest token
  // handle information.
  HRESULT UpdateAssociatedSids(
      std::map<std::wstring, std::wstring>* sid_to_handle);

 protected:
  // Returns the storage used for the instance pointer.
  static AssociatedUserValidator** GetInstanceStorage();

  explicit AssociatedUserValidator(base::TimeDelta validation_timeout);
  virtual ~AssociatedUserValidator();

  // Returns whether the user should be locked out of sign in (only used in
  // tests).
  bool IsUserAccessBlockedForTesting(const std::wstring& sid) const;

  // Forces a refresh of all token handles the next time they are queried.
  // This function should only be called in tests.
  void ForceRefreshTokenHandlesForTesting();

 private:
  void CheckTokenHandleValidity(
      const std::map<std::wstring, std::wstring>& handles_to_verify);
  void StartTokenValidityQuery(const std::wstring& sid,
                               const std::wstring& token_handle,
                               base::TimeDelta timeout);
  bool IsTokenHandleValidForUser(const std::wstring& sid);
  bool IsUserAssociated(const std::wstring& sid);
  bool HasInvokedUpdateAssociatedSids();

  // Stores information about the current state of a user's token handle.
  // This information includes:
  //   * The last token handle found for the user.
  //   * The validity of this last token handle (if checked).
  //   * The time of the last update of the validity of this token handle.
  //     This will often be the max end time of the last query that was made on
  //     the token handle.
  //   * The handle to the current thread being executed to verify the
  //     validity of the last token handle.
  struct TokenHandleInfo {
    TokenHandleInfo();
    ~TokenHandleInfo();

    // Used when the handle is empty or invalid.
    explicit TokenHandleInfo(const std::wstring& token_handle);

    // Used to create a new token handle info that needs to query validity.
    // The validity is assumed to be invalid at the time of construction.
    TokenHandleInfo(const std::wstring& token_handle,
                    base::Time update_time,
                    base::win::ScopedHandle::Handle thread_handle);

    std::wstring queried_token_handle;
    bool is_valid = false;
    base::Time last_update;
    base::win::ScopedHandle pending_query_thread;
  };

  // Increments / decrements |block_deny_access_update_| to prevent denying
  // user access when a token handle becomes invalid. Only called via a
  // ScopedBlockDenyAccessUpdate object.
  void BlockDenyAccessUpdate();
  void UnblockDenyAccessUpdate();

  // Maps a user's sid to the token handle info associated with this user (if
  // any).
  std::map<std::wstring, std::unique_ptr<TokenHandleInfo>>
      user_to_token_handle_info_;
  base::TimeDelta validation_timeout_;
  std::set<std::wstring> locked_user_sids_;
  mutable base::Lock validator_lock_;

  // When |block_deny_access_update_| != 0, prevent users from being denied
  // access when DenySigninForUsersWithInvalidTokenHandles is called. This
  // prevents users from being locked out while signing is occurring but a token
  // handle update is also being requested at the same time. The functions
  // LockDenyAccessUpdate / UnlockDenyAccessUpdate are called in the lifetime of
  // a ScopedBlockDenyAccessUpdate to update this member.
  size_t block_deny_access_update_ = 0;

  // Keeps track of whether "UpdateAssociatedSids" method is invoked at least
  // once while creation of reauth credentials in the LoginUI.
  bool has_invoked_update_associated_sids_ = false;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_ASSOCIATED_USER_VALIDATOR_H_
