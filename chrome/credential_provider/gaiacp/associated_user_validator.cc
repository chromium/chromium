// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/associated_user_validator.h"

#include <ntstatus.h>
#include <process.h>

#include <string>
#include <string_view>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/win_util.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/internet_availability_checker.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/user_policies_manager.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

namespace credential_provider {

const base::TimeDelta
    AssociatedUserValidator::kDefaultTokenHandleValidationTimeout =
        base::Milliseconds(3000);

const base::TimeDelta AssociatedUserValidator::kTokenHandleValidityLifetime =
    base::Seconds(60);

const char AssociatedUserValidator::kTokenInfoUrl[] =
    "https://www.googleapis.com/oauth2/v2/tokeninfo";

constexpr long kDayInMillis = 86400000;

namespace {

struct CheckReauthParams {
  std::wstring sid;
  std::wstring token_handle;
  std::unique_ptr<WinHttpUrlFetcher> fetcher;
};

// Queries google to see whether the user's token handle is no longer valid or
// is expired. Returns 1 if it is still valid, 0 if the user needs to reauth.
unsigned __stdcall CheckReauthStatus(void* param) {
  DCHECK(param);
  std::unique_ptr<CheckReauthParams> reauth_info(
      reinterpret_cast<CheckReauthParams*>(param));

  if (reauth_info->fetcher) {
    reauth_info->fetcher->SetRequestHeader("Content-Type",
                                           "application/x-www-form-urlencoded");

    std::string body = base::StringPrintf("token_handle=%ls",
                                          reauth_info->token_handle.c_str());
    HRESULT hr = reauth_info->fetcher->SetRequestBody(body.c_str());
    if (FAILED(hr)) {
      LOGFN(ERROR) << "fetcher.SetRequestBody sid=" << reauth_info->sid
                   << " hr=" << putHR(hr);
      return 1;
    }

    std::vector<char> response;
    hr = reauth_info->fetcher->Fetch(&response);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "fetcher.Fetch sid=" << reauth_info->sid
                   << " hr=" << putHR(hr);
      return 1;
    }

    std::string_view response_string(response.data(), response.size());
    std::optional<base::Value> properties_val = base::JSONReader::Read(
        response_string, base::JSON_ALLOW_TRAILING_COMMAS);
    if (!properties_val || !properties_val->is_dict()) {
      LOGFN(ERROR) << "base::JSONReader::Read failed forcing reauth";
      return 0;
    }

    const auto& properties = properties_val->GetDict();
    std::optional<int> expires_in = properties.FindInt("expires_in");
    if (properties.contains("error") || !expires_in || expires_in.value() < 0) {
      LOGFN(VERBOSE) << "Needs reauth sid=" << reauth_info->sid;
      return 0;
    }
  }

  return 1;
}

bool TokenHandleNeedsUpdate(const base::Time& last_refresh) {
  return (base::Time::Now() - last_refresh) >
         AssociatedUserValidator::kTokenHandleValidityLifetime;
}

bool WaitForQueryResult(const base::win::ScopedHandle& thread_handle,
                        const base::Time& until) {
  if (!thread_handle.IsValid())
    return true;

  DWORD time_left = std::max<DWORD>(
      static_cast<DWORD>((until - base::Time::Now()).InMilliseconds()), 0);

  // See if a response to the token info can be fetched in a reasonable
  // amount of time. If not, assume there is no internet and that the handle
  // is still valid.
  HRESULT hr = ::WaitForSingleObject(thread_handle.Get(), time_left);

  bool token_handle_validity = false;
  if (hr == WAIT_OBJECT_0) {
    DWORD exit_code;
    token_handle_validity =
        !::GetExitCodeThread(thread_handle.Get(), &exit_code) || exit_code == 1;
  } else if (hr == WAIT_TIMEOUT) {
    token_handle_validity = true;
  }

  return token_handle_validity;
}

HRESULT ModifyUserAccess(const std::unique_ptr<ScopedLsaPolicy>& policy,
                         const std::wstring& sid,
                         bool allow) {
  OSUserManager* manager = OSUserManager::Get();
  wchar_t username[kWindowsUsernameBufferLength];
  wchar_t domain[kWindowsDomainBufferLength];

  HRESULT hr = manager->FindUserBySID(
      sid.c_str(), username, std::size(username), domain, std::size(domain));

  if (FAILED(hr)) {
    LOGFN(ERROR) << "FindUserBySID sid=" << sid << " hr=" << putHR(hr);
    return hr;
  }

  PSID psid = nullptr;
  if (!::ConvertStringSidToSidW(sid.c_str(), &psid)) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ConvertStringSidToSidW sid=" << sid << " hr=" << putHR(hr);
    return hr;
  }

  std::vector<std::wstring> account_rights{
      SE_DENY_INTERACTIVE_LOGON_NAME, SE_DENY_NETWORK_LOGON_NAME,
      SE_DENY_REMOTE_INTERACTIVE_LOGON_NAME};
  HRESULT status;
  if (!allow) {
    status = policy->AddAccountRights(psid, account_rights);
  } else {
    // Note: We are still going to keep this time restrictions flow to avoid
    // any cornercase scenario where user is blocked on login UI because
    // time restrictions were set but were never added back.
    hr = manager->ModifyUserAccessWithLogonHours(domain, username, allow);
    if (FAILED(hr))
      LOGFN(ERROR) << "Failed to remove time restrictions for sid : " << sid;

    status = policy->RemoveAccountRights(psid, account_rights);
  }
  ::LocalFree(psid);
  return status;
}

}  // namespace

AssociatedUserValidator::TokenHandleInfo::TokenHandleInfo() = default;
AssociatedUserValidator::TokenHandleInfo::~TokenHandleInfo() = default;

AssociatedUserValidator::TokenHandleInfo::TokenHandleInfo(
    const std::wstring& token_handle)
    : queried_token_handle(token_handle), last_update(base::Time::Now()) {}

AssociatedUserValidator::TokenHandleInfo::TokenHandleInfo(
    const std::wstring& token_handle,
    base::Time update_time,
    base::win::ScopedHandle::Handle thread_handle)
    : queried_token_handle(token_handle),
      last_update(update_time),
      pending_query_thread(thread_handle) {}

AssociatedUserValidator::ScopedBlockDenyAccessUpdate::
    ScopedBlockDenyAccessUpdate(AssociatedUserValidator* validator)
    : validator_(validator) {
  DCHECK(validator_);
  validator_->BlockDenyAccessUpdate();
}

AssociatedUserValidator::ScopedBlockDenyAccessUpdate::
    ~ScopedBlockDenyAccessUpdate() {
  validator_->UnblockDenyAccessUpdate();
}

// static
AssociatedUserValidator* AssociatedUserValidator::Get() {
  return *GetInstanceStorage();
}

// static
AssociatedUserValidator** AssociatedUserValidator::GetInstanceStorage() {
  static AssociatedUserValidator instance(kDefaultTokenHandleValidationTimeout);
  static AssociatedUserValidator* instance_storage = &instance;

  return &instance_storage;
}

AssociatedUserValidator::AssociatedUserValidator(
    base::TimeDelta validation_timeout)
    : validation_timeout_(validation_timeout) {}

AssociatedUserValidator::~AssociatedUserValidator() = default;

bool AssociatedUserValidator::IsOnlineLoginStale(
    const std::wstring& sid) const {
  wchar_t last_token_valid_millis[512];
  ULONG last_token_valid_size = std::size(last_token_valid_millis);
  HRESULT hr = GetUserProperty(sid, base::UTF8ToWide(kKeyLastTokenValid),
                               last_token_valid_millis, &last_token_valid_size);

  if (FAILED(hr)) {
    LOGFN(VERBOSE) << "GetUserProperty for " << kKeyLastTokenValid
                   << " failed. hr=" << putHR(hr);
    // DEPRECATED FLOW. Keeping it for backward compatibility.
    hr = GetUserProperty(sid,
                         base::UTF8ToWide(kKeyLastSuccessfulOnlineLoginMillis),
                         last_token_valid_millis, &last_token_valid_size);

    if (FAILED(hr)) {
      LOGFN(VERBOSE) << "GetUserProperty for "
                     << kKeyLastSuccessfulOnlineLoginMillis
                     << " failed. hr=" << putHR(hr);
      // Fallback to the less obstructive option to not enforce
      // login via google when fetching the registry entry fails.
      return false;
    }
  }

  int64_t last_token_valid_millis_int64;
  base::StringToInt64(last_token_valid_millis, &last_token_valid_millis_int64);

  DWORD validity_period_days;
  if (UserPoliciesManager::Get()->CloudPoliciesEnabled()) {
    UserPolicies user_policies;
    UserPoliciesManager::Get()->GetUserPolicies(sid, &user_policies);
    validity_period_days = user_policies.validity_period_days;
  } else {
    hr = GetGlobalFlag(base::UTF8ToWide(kKeyValidityPeriodInDays),
                       &validity_period_days);
    if (FAILED(hr)) {
      LOGFN(VERBOSE) << "GetGlobalFlag for " << kKeyValidityPeriodInDays
                     << " failed. hr=" << putHR(hr);
      // Fallback to the less obstructive option to not enforce login via google
      // when fetching the registry entry fails.
      return false;
    }
  }

  int64_t validity_period_in_millis =
      kDayInMillis * static_cast<int64_t>(validity_period_days);
  int64_t time_delta_from_last_login =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds() -
      last_token_valid_millis_int64;
  return time_delta_from_last_login >= validity_period_in_millis;
}

bool AssociatedUserValidator::HasInternetConnection() const {
  return InternetAvailabilityChecker::Get()->HasInternetConnection();
}

bool AssociatedUserValidator::HasInvokedUpdateAssociatedSids() {
  return has_invoked_update_associated_sids_;
}

HRESULT AssociatedUserValidator::UpdateAssociatedSids(
    std::map<std::wstring, std::wstring>* sid_to_handle) {
  has_invoked_update_associated_sids_ = true;

  std::map<std::wstring, UserTokenHandleInfo> sids_to_handle_info;

  HRESULT hr = GetUserTokenHandles(&sids_to_handle_info);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetUserTokenHandles hr=" << putHR(hr);
    return hr;
  }

  std::set<std::wstring> users_to_delete;
  OSUserManager* manager = OSUserManager::Get();
  for (const auto& sid_to_association : sids_to_handle_info) {
    const std::wstring& sid = sid_to_association.first;
    const UserTokenHandleInfo& info = sid_to_association.second;

    // If both gaia id and email address are empty. Then remove the
    // User Properties as it is invalid.
    if (info.gaia_id.empty() && info.email_address.empty()) {
      users_to_delete.insert(sid_to_association.first);
      continue;
    }
    hr = manager->FindUserBySID(sid.c_str(), nullptr, 0, nullptr, 0);
    if (hr == HRESULT_FROM_WIN32(ERROR_NONE_MAPPED)) {
      users_to_delete.insert(sid_to_association.first);
      continue;
    } else if (FAILED(hr)) {
      LOGFN(ERROR) << "manager->FindUserBySID hr=" << putHR(hr);
    }

    if (sid_to_handle)
      sid_to_handle->emplace(sid, info.token_handle);
  }

  for (const auto& to_delete : users_to_delete) {
    user_to_token_handle_info_.erase(to_delete);
    RemoveAllUserProperties(to_delete);
  }

  return S_OK;
}

size_t AssociatedUserValidator::GetAssociatedUsersCount() {
  base::AutoLock locker(validator_lock_);

  UpdateAssociatedSids(nullptr);

  return user_to_token_handle_info_.size();
}

bool AssociatedUserValidator::IsUserAccessBlockingEnforced(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus) const {
  if (!CGaiaCredentialProvider::IsUsageScenarioSupported(cpus))
    return false;

  return true;
}

bool AssociatedUserValidator::DenySigninForUsersWithInvalidTokenHandles(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    const std::vector<std::wstring>& reauth_sids) {
  base::AutoLock locker(validator_lock_);

  if (block_deny_access_update_) {
    LOGFN(VERBOSE) << "Block the deny access update";
    return false;
  }

  if (!IsUserAccessBlockingEnforced(cpus)) {
    LOGFN(VERBOSE) << "User Access Blocking not enforced.";
    return false;
  }

  HRESULT hr = UpdateAssociatedSids(nullptr);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "UpdateAssociatedSids hr=" << putHR(hr);
    return false;
  }

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);

  bool user_denied_signin = false;
  OSUserManager* manager = OSUserManager::Get();
  for (const auto& sid : reauth_sids) {
    if (locked_user_sids_.find(sid) != locked_user_sids_.end())
      continue;

    // Note that logon hours cannot be changed on domain joined AD user account.
    if (GetAuthEnforceReason(sid) != EnforceAuthReason::NOT_ENFORCED &&
        !manager->IsUserDomainJoined(sid)) {
      LOGFN(VERBOSE) << "Revoking access for sid=" << sid;
      hr = ModifyUserAccess(policy, sid, false);
      if (FAILED(hr)) {
        LOGFN(ERROR) << "ModifyUserAccess sid=" << sid << " hr=" << putHR(hr);
      } else {
        locked_user_sids_.insert(sid);
        user_denied_signin = true;
      }
    } else if (manager->IsUserDomainJoined(sid)) {
      // TODO(crbug.com/40631676): Description provided in the bug.
      LOGFN(VERBOSE) << "Not denying signin for AD user accounts.";
    }
  }

  return user_denied_signin;
}

HRESULT AssociatedUserValidator::RestoreUserAccess(const std::wstring& sid) {
  base::AutoLock locker(validator_lock_);

  if (locked_user_sids_.erase(sid)) {
    auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
    return ModifyUserAccess(policy, sid, true);
  }

  return S_OK;
}

void AssociatedUserValidator::AllowSigninForAllAssociatedUsers(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus) {
  base::AutoLock locker(validator_lock_);

  if (!CGaiaCredentialProvider::IsUsageScenarioSupported(cpus))
    return;

  std::map<std::wstring, std::wstring> sids_to_handle;
  HRESULT hr = UpdateAssociatedSids(&sids_to_handle);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "UpdateAssociatedSids hr=" << putHR(hr);
    return;
  }

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  for (const auto& sid_to_handle : sids_to_handle)
    ModifyUserAccess(policy, sid_to_handle.first, true);

  locked_user_sids_.clear();
}

void AssociatedUserValidator::AllowSigninForUsersWithInvalidTokenHandles() {
  base::AutoLock locker(validator_lock_);

  LOGFN(VERBOSE);
  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  for (auto& sid : locked_user_sids_) {
    HRESULT hr = ModifyUserAccess(policy, sid, true);
    if (FAILED(hr))
      LOGFN(ERROR) << "ModifyUserAccess sid=" << sid << " hr=" << putHR(hr);
  }
  locked_user_sids_.clear();
}

void AssociatedUserValidator::StartRefreshingTokenHandleValidity() {
  base::AutoLock locker(validator_lock_);

  std::map<std::wstring, std::wstring> sid_to_handle;
  HRESULT hr = UpdateAssociatedSids(&sid_to_handle);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "UpdateAssociatedSids hr=" << putHR(hr);
    return;
  }

  // Fire off the threads that will query the token handles but do not wait for
  // them to complete. Later queries will do the wait.
  CheckTokenHandleValidity(sid_to_handle);
}

void AssociatedUserValidator::CheckTokenHandleValidity(
    const std::map<std::wstring, std::wstring>& handles_to_verify) {
  for (auto it = handles_to_verify.cbegin(); it != handles_to_verify.cend();
       ++it) {
    // Make sure the user actually exists.
    if (FAILED(OSUserManager::Get()->FindUserBySID(it->first.c_str(), nullptr,
                                                   0, nullptr, 0))) {
      continue;
    }

    // User exists, has a gaia id or email address, but no token handle.
    // Consider this an invalid token handle and the user needs to sign in
    // with Gaia to get a new one.
    if (it->second.empty()) {
      user_to_token_handle_info_[it->first] =
          std::make_unique<TokenHandleInfo>(std::wstring());
      continue;
    }

    // If there is already token handle info for the current user and it
    // 1. Is NOT valid.
    // AND
    // 2. There is no query still pending on it.
    // Then this means that the user has an invalid token handle.
    // The state of this handle will never change during the execution of the
    // sign in process (since a new token handle will only be written on
    // successful sign in) so it does not need to update and the current state
    // information is valid.
    auto existing_validity_it = user_to_token_handle_info_.find(it->first);
    if (existing_validity_it != user_to_token_handle_info_.end() &&
        !existing_validity_it->second->is_valid &&
        !existing_validity_it->second->pending_query_thread.IsValid()) {
      continue;
    }

    // Start a new token handle query if:
    // 1. No token info entry yet exists for this token handle.
    // OR
    // 2. Token info exists but it is stale (either because it is a query that
    // was started a while ago but for which we never tried to query the result
    // because the last query result is from a while ago or because the last
    // query result is older than |kTokenHandleValidityLifetime|).
    if (existing_validity_it == user_to_token_handle_info_.end() ||
        TokenHandleNeedsUpdate(existing_validity_it->second->last_update)) {
      StartTokenValidityQuery(it->first, it->second, validation_timeout_);
    }
  }
}

void AssociatedUserValidator::StartTokenValidityQuery(
    const std::wstring& sid,
    const std::wstring& token_handle,
    base::TimeDelta timeout) {
  base::Time max_end_time = base::Time::Now() + timeout;

  // Fire off a thread to check with Gaia if a re-auth is required. The thread
  // does not reference this object nor does this object have any reference
  // directly on the thread. This object only checks for the return code of the
  // thread within a given timeout. If no return code is given in that timeout
  // then assume that the token handle is valid. The running thread can continue
  // running and finish its execution without worrying about notifying anything
  // about the result.
  unsigned wait_thread_id;
  CheckReauthParams* params = new CheckReauthParams{
      sid, token_handle,
      WinHttpUrlFetcher::Create(GURL(AssociatedUserValidator::kTokenInfoUrl))};
  uintptr_t wait_thread =
      _beginthreadex(nullptr, 0, CheckReauthStatus,
                     reinterpret_cast<void*>(params), 0, &wait_thread_id);
  if (wait_thread == 0) {
    user_to_token_handle_info_[sid] =
        std::make_unique<TokenHandleInfo>(token_handle);
    delete params;
    return;
  }

  user_to_token_handle_info_[sid] = std::make_unique<TokenHandleInfo>(
      token_handle, max_end_time, reinterpret_cast<HANDLE>(wait_thread));
}

bool AssociatedUserValidator::IsAuthEnforcedForUser(const std::wstring& sid) {
  base::AutoLock locker(validator_lock_);
  return GetAuthEnforceReason(sid) !=
         AssociatedUserValidator::EnforceAuthReason::NOT_ENFORCED;
}

AssociatedUserValidator::EnforceAuthReason
AssociatedUserValidator::GetAuthEnforceReason(const std::wstring& sid) {
  LOGFN(VERBOSE);

  // Is user not associated, then we shouldn't have any auth enforcement.
  if (!IsUserAssociated(sid)) {
    LOGFN(VERBOSE) << "IsUserAssociated is false, not forcing auth";
    return AssociatedUserValidator::EnforceAuthReason::NOT_ENFORCED;
  }

  // Check if online sign in is enforced.
  if (IsOnlineLoginEnforced(sid)) {
    LOGFN(VERBOSE) << "IsOnlineLoginEnforced is true, forcing auth";
    return AssociatedUserValidator::EnforceAuthReason::ONLINE_LOGIN_ENFORCED;
  }

  // All token handles are valid when no internet connection is available.
  if (!HasInternetConnection()) {
    if (!IsOnlineLoginStale(sid)) {
      LOGFN(VERBOSE) << "HasInternetConnectionis false and IsOnlineLoginStale "
                        "is false - not forcing auth";
      return AssociatedUserValidator::EnforceAuthReason::NOT_ENFORCED;
    }
    LOGFN(VERBOSE) << "HasInternetConnectionis false and IsOnlineLoginStale is "
                      "true - forcing auth";
    return AssociatedUserValidator::EnforceAuthReason::ONLINE_LOGIN_STALE;
  }

  // Force user to login when policies are missing or stale. This check should
  // be done before MDM enrollment to have the correct MDM enrollment policy for
  // user.
  if (UserPoliciesManager::Get()->CloudPoliciesEnabled() &&
      UserPoliciesManager::Get()->IsUserPolicyStaleOrMissing(sid)) {
    LOGFN(VERBOSE) << "CloudPolicies enabled and  >IsUserPolicyStaleOrMissing "
                      "is true - forcing auth";
    return AssociatedUserValidator::EnforceAuthReason::
        MISSING_OR_STALE_USER_POLICIES;
  }

  // Force a reauth only for this user if mdm enrollment is needed, so that they
  // enroll.
  if (NeedsToEnrollWithMdm(sid)) {
    LOGFN(VERBOSE) << "NeedsToEnrollWithMdm is true, forcing auth";
    return AssociatedUserValidator::EnforceAuthReason::NOT_ENROLLED_WITH_MDM;
  }

  if (PasswordRecoveryEnabled()) {
    std::wstring store_key = GetUserPasswordLsaStoreKey(sid);
    auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
    if (!policy->PrivateDataExists(store_key.c_str())) {
      LOGFN(VERBOSE) << "Enforcing re-auth due to missing password lsa store "
                        "data for user "
                     << sid;
      return AssociatedUserValidator::EnforceAuthReason::
          MISSING_PASSWORD_RECOVERY_INFO;
    }
  }

  if (!IsTokenHandleValidForUser(sid)) {
    LOGFN(VERBOSE) << "IsTokenHandleValidForUser is false, forcing auth";
    return AssociatedUserValidator::EnforceAuthReason::INVALID_TOKEN_HANDLE;
  }

  if (UploadDeviceDetailsNeeded(sid)) {
    LOGFN(VERBOSE) << "UploadDeviceDetailsNeeded is true, forcing auth";
    return AssociatedUserValidator::EnforceAuthReason::
        UPLOAD_DEVICE_DETAILS_FAILED;
  }

  return AssociatedUserValidator::EnforceAuthReason::NOT_ENFORCED;
}

bool AssociatedUserValidator::IsUserAssociated(const std::wstring& sid) {
  // If at this point there is no token info entry for this user, assume the
  // user is not associated and does not need a token handle and is thus always
  // valid. Between the first creation of all the token infos
  // and the eventual successful sign in, there should be no new token handles
  // created so we can immediately assign that an absence of token handle info
  // for this user means that they are not associated and do not need to
  // validate any token handles.
  auto validity_it = user_to_token_handle_info_.find(sid);
  return validity_it != user_to_token_handle_info_.end();
}

bool AssociatedUserValidator::IsTokenHandleValidForUser(
    const std::wstring& sid) {
  // Make sure sid mapping in registry is always up to date before checking
  // for token validity.
  if (!HasInvokedUpdateAssociatedSids())
    UpdateAssociatedSids(nullptr);

  auto validity_it = user_to_token_handle_info_.find(sid);
  if (validity_it == user_to_token_handle_info_.end())
    return true;

  // This function will start a new query if the current info for the token
  // handle is stale or has not yet been queried. At the end of this function,
  // either we will already have the validity of the token handle or we have a
  // handle to a pending query that we wait to complete before finally having
  // the validity.
  CheckTokenHandleValidity({{sid, validity_it->second->queried_token_handle}});

  // If a query is still pending, wait for it and update the validity.
  if (validity_it->second->pending_query_thread.IsValid()) {
    validity_it->second->is_valid =
        WaitForQueryResult(validity_it->second->pending_query_thread,
                           validity_it->second->last_update);
    if (!validity_it->second->is_valid) {
      // Clear the token handle to delete it. At this point we know it is
      // definetely invalid so if we remove the handle completely we will
      // no longer need to query it and can just assume immediately that
      // the user needs to be reauthorized.
      HRESULT hr = SetUserProperty(sid, kUserTokenHandle, L"");
      if (FAILED(hr))
        LOGFN(ERROR) << "SetUserProperty hr=" << putHR(hr);
    }
    validity_it->second->pending_query_thread.Close();
    base::Time now = base::Time::Now();
    // NOTE: Don't always update |last_update| because the result of this query
    // may still be old. E.g.:
    // 1. At time X thread is started to query. The maximum end time of this
    //    query is X + timeout
    // 2. At time Y (X + timeout + lifetime > Y >> X + timeout) we ask for
    //    the validity of the token handle. The time Y might still be valid
    //    when considering the lifetime but may still be relatively old
    //    depending on how long after time X the request is made. So keep the
    //    original end time of the query as the last update (if the end time
    //    occurs before time Y) so that the token handle is updated earlier on
    //    the next query.
    validity_it->second->last_update = now > validity_it->second->last_update
                                           ? validity_it->second->last_update
                                           : now;
  }

  if (validity_it->second->is_valid) {
    // Update the last token valid timestamp.
    int64_t current_time = static_cast<int64_t>(
        base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds());
    SetUserProperty(sid, base::UTF8ToWide(kKeyLastTokenValid),
                    base::NumberToWString(current_time));
  }

  return validity_it->second->is_valid;
}

void AssociatedUserValidator::BlockDenyAccessUpdate() {
  base::AutoLock locker(validator_lock_);
  ++block_deny_access_update_;
}

void AssociatedUserValidator::UnblockDenyAccessUpdate() {
  base::AutoLock locker(validator_lock_);
  DCHECK(block_deny_access_update_ > 0);
  --block_deny_access_update_;
}

bool AssociatedUserValidator::IsDenyAccessUpdateBlocked() const {
  base::AutoLock locker(validator_lock_);
  return block_deny_access_update_ > 0;
}

bool AssociatedUserValidator::IsUserAccessBlockedForTesting(
    const std::wstring& sid) const {
  base::AutoLock locker(validator_lock_);
  return locked_user_sids_.find(sid) != locked_user_sids_.end();
}

void AssociatedUserValidator::ForceRefreshTokenHandlesForTesting() {
  base::AutoLock locker(validator_lock_);
  for (const auto& user_info : user_to_token_handle_info_) {
    // Make the last update time outside the validity lifetime of the token
    // handle.
    user_info.second->last_update =
        base::Time::Now() - kTokenHandleValidityLifetime;
  }
}

}  // namespace credential_provider
