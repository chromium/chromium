// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"

#include <iomanip>
#include <map>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/common/chrome_version.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/associated_user_validator.h"
#include "chrome/credential_provider/gaiacp/auth_utils.h"
#include "chrome/credential_provider/gaiacp/device_policies_manager.h"
#include "chrome/credential_provider/gaiacp/gaia_credential.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_other_user.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reauth_credential.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {

#define W2CW(p) const_cast<wchar_t*>(p)

static const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR g_field_desc[] = {
    {FID_DESCRIPTION, CPFT_LARGE_TEXT, W2CW(L"Description"), GUID_NULL},
    {FID_CURRENT_PASSWORD_FIELD, CPFT_PASSWORD_TEXT, W2CW(L"Windows Password"),
     GUID_NULL},
    {FID_SUBMIT, CPFT_SUBMIT_BUTTON, W2CW(L"Submit button"), GUID_NULL},
    {FID_FORGOT_PASSWORD_LINK, CPFT_COMMAND_LINK, W2CW(L"Forgot Password"),
     GUID_NULL},
    {FID_PROVIDER_LOGO, CPFT_TILE_IMAGE, W2CW(L"Provider logo"),
     CPFG_CREDENTIAL_PROVIDER_LOGO},
    {FID_PROVIDER_LABEL, CPFT_LARGE_TEXT, W2CW(L"Provider label"),
     CPFG_CREDENTIAL_PROVIDER_LABEL},
};

static_assert(std::size(g_field_desc) == FIELD_COUNT,
              "g_field_desc does not match FIELDID enum");

namespace {

// Initializes an object that implements IReauthCredential.
HRESULT InitializeReauthCredential(
    CGaiaCredentialProvider* provider,
    const std::wstring& sid,
    const std::wstring& domain,
    const std::wstring& username,
    const Microsoft::WRL::ComPtr<IGaiaCredential>& gaia_cred) {
  Microsoft::WRL::ComPtr<IReauthCredential> reauth;
  HRESULT hr = gaia_cred.As(&reauth);
  if (FAILED(hr)) {
    LOG(ERROR) << "Could not get reauth credential interface hr=" << putHR(hr);
    return hr;
  }

  hr = gaia_cred->Initialize(provider);
  if (FAILED(hr)) {
    LOG(ERROR) << "Could not initialize credential hr=" << putHR(hr);
    return hr;
  }

  // Get the user's email address.  If not found, proceed anyway.  The net
  // effect is that the user will need to enter their email address manually
  // instead of it being pre-filled.
  wchar_t email[64];
  ULONG email_length = std::size(email);
  hr = GetUserProperty(sid.c_str(), kUserEmail, email, &email_length);
  if (FAILED(hr))
    email[0] = 0;

  hr = reauth->SetOSUserInfo(CComBSTR(W2COLE(sid.c_str())),
                             CComBSTR(W2COLE(domain.c_str())),
                             CComBSTR(W2COLE(username.c_str())));
  if (FAILED(hr)) {
    LOGFN(ERROR) << "cred->SetOSUserInfo hr=" << putHR(hr);
    return hr;
  }

  if (email[0]) {
    hr = reauth->SetEmailForReauth(CComBSTR(email));
    if (FAILED(hr))
      LOGFN(ERROR) << "reauth->SetEmailForReauth hr=" << putHR(hr);
  } else {
    LOGFN(VERBOSE) << "reauth for sid " << sid
                   << " doesn't contain the email association";
  }

  return S_OK;
}

template <class CredentialT>
HRESULT CreateCredentialObject(
    CGaiaCredentialProvider::CredentialCreatorFn creator_fn,
    CGaiaCredentialProvider::GaiaCredentialComPtrStorage* credential_com_ptr) {
  if (creator_fn) {
    return creator_fn(credential_com_ptr);
  }

  return CComCreator<CComObject<CredentialT>>::CreateInstance(
      nullptr, IID_PPV_ARGS(&credential_com_ptr->gaia_cred));
}

}  // namespace

// Class that when constructed automatically starts a thread that tries
// to update the validity of available token handles. If the background
// thread detects that any token handle has changed, then it will notify
// the provider |event_handler| object of this event.
class BackgroundTokenHandleUpdater {
 public:
  explicit BackgroundTokenHandleUpdater(
      ICredentialUpdateEventsHandler* event_handler,
      const std::vector<std::wstring>* reauth_sids);
  ~BackgroundTokenHandleUpdater();

 private:
  static unsigned __stdcall PeriodicTokenHandleUpdate(void* param);
  bool IsAuthEnforcedOnAssociatedUsers();

  // Raw pointer to the interface on CGaiaCredentialProvider that is used
  // to notify that token handle validity has changed. Any instance of this
  // class should be owned by the CGaiaCredentialProvider to ensure that
  // this pointer outlives the updater.
  raw_ptr<ICredentialUpdateEventsHandler> event_handler_;
  raw_ptr<const std::vector<std::wstring>> reauth_sids_;

  base::win::ScopedHandle token_update_thread_;
  base::WaitableEvent token_update_quit_event_;
};

BackgroundTokenHandleUpdater::BackgroundTokenHandleUpdater(
    ICredentialUpdateEventsHandler* event_handler,
    const std::vector<std::wstring>* reauth_sids)
    : event_handler_(event_handler), reauth_sids_(reauth_sids) {
  unsigned wait_thread_id;
  uintptr_t wait_thread =
      _beginthreadex(nullptr, 0, PeriodicTokenHandleUpdate,
                     reinterpret_cast<void*>(this), 0, &wait_thread_id);
  if (wait_thread != 0) {
    token_update_thread_.Set(
        reinterpret_cast<base::win::ScopedHandle::Handle>(wait_thread));
  }
}

BackgroundTokenHandleUpdater::~BackgroundTokenHandleUpdater() {
  if (token_update_thread_.IsValid()) {
    // Tell the background thread to quit and then make sure it does.  This
    // prevents it from accessing data members that have been freed.
    token_update_quit_event_.Signal();
    ::WaitForSingleObject(token_update_thread_.Get(), INFINITE);
  }
}

bool BackgroundTokenHandleUpdater::IsAuthEnforcedOnAssociatedUsers() {
  std::map<std::wstring, UserTokenHandleInfo> sids_to_handle_info;
  HRESULT hr = GetUserTokenHandles(&sids_to_handle_info);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetUserTokenHandles hr=" << putHR(hr);
    return hr;
  }

  for (const auto& sid_to_association : sids_to_handle_info) {
    const std::wstring& sid = sid_to_association.first;
    // Checks if the login UI was already refreshed due to
    // auth enforcements on this sid.
    if (reauth_sids_ != nullptr && base::Contains(*reauth_sids_, sid))
      continue;

    // Return true if the associated user sid has auth enforced.
    if (AssociatedUserValidator::Get()->IsAuthEnforcedForUser(sid)) {
      return true;
    }
  }
  return false;
}

unsigned __stdcall BackgroundTokenHandleUpdater::PeriodicTokenHandleUpdate(
    void* param) {
  BackgroundTokenHandleUpdater* updater =
      reinterpret_cast<BackgroundTokenHandleUpdater*>(param);
  ICredentialUpdateEventsHandler* event_handler = updater->event_handler_;
  base::WaitableEvent& stop_event = updater->token_update_quit_event_;

  while (true) {
    HRESULT hr = ::WaitForSingleObject(
        stop_event.handle(),
        AssociatedUserValidator::kTokenHandleValidityLifetime.InMilliseconds());

    if (hr != WAIT_TIMEOUT)
      break;

    bool user_access_changed = updater->IsAuthEnforcedOnAssociatedUsers();
    if (user_access_changed) {
      LOGFN(VERBOSE) << "A user token handle has been invalidated. Refreshing "
                        "credentials";
    }

    if (GetGlobalFlagOrDefault(kRegUpdateCredentialsOnChange, 0))
      event_handler->UpdateCredentialsIfNeeded(user_access_changed);
  }

  return 0;
}

CGaiaCredentialProvider::GaiaCredentialComPtrStorage::
    GaiaCredentialComPtrStorage() = default;
CGaiaCredentialProvider::GaiaCredentialComPtrStorage::
    ~GaiaCredentialComPtrStorage() = default;

CGaiaCredentialProvider::ProviderConcurrentState::ProviderConcurrentState() =
    default;
CGaiaCredentialProvider::ProviderConcurrentState::~ProviderConcurrentState() =
    default;

bool CGaiaCredentialProvider::ProviderConcurrentState::
    RequestUserRefreshIfNeeded(bool user_access_changed) {
  base::AutoLock locker(state_update_lock_);
  if (auto_logon_credential_) {
    // Auto logon has precedence and has already signalled a credential changed,
    // save the user refresh for a later update and don't signal the change
    // again.
    if (user_access_changed)
      pending_users_refresh_needed_ = user_access_changed;
    return false;
  }

  // No auto logon present and there is a user access change or a pending
  // refresh to be executed. We clear the pending the refresh and just set
  // |users_need_to_be_refreshed_| to notify that a refresh is needed on the
  // next GetCredentialCount.
  if (user_access_changed || pending_users_refresh_needed_) {
    users_need_to_be_refreshed_ = true;
    pending_users_refresh_needed_ = false;
  }

  return users_need_to_be_refreshed_;
}

bool CGaiaCredentialProvider::ProviderConcurrentState::SetAutoLogonCredential(
    const Microsoft::WRL::ComPtr<IGaiaCredential>& auto_logon_credential) {
  base::AutoLock locker(state_update_lock_);
  // Always update the credential.
  auto_logon_credential_ = auto_logon_credential;
  if (auto_logon_credential_) {
    // If a previous user refresh was signalled, then the credential changed
    // was already sent. Don't need to send it again, but we need save the
    // user refresh for a later credential change event since auto logon has
    // precedence.
    if (users_need_to_be_refreshed_) {
      pending_users_refresh_needed_ = users_need_to_be_refreshed_;
      users_need_to_be_refreshed_ = false;
      return false;
    }

    return true;
  }

  // No auto logon credential was set, no need to send credential changed event.
  return false;
}

void CGaiaCredentialProvider::ProviderConcurrentState::GetUpdatedState(
    bool* needs_to_refresh_users,
    GaiaCredentialComPtrStorage* auto_logon_credential) {
  DCHECK(needs_to_refresh_users);
  DCHECK(auto_logon_credential);
  base::AutoLock locker(state_update_lock_);

  // States need to be mutually exclusive.
  DCHECK(!users_need_to_be_refreshed_ || !auto_logon_credential_);

  *needs_to_refresh_users = users_need_to_be_refreshed_;
  auto_logon_credential->gaia_cred = auto_logon_credential_;

  // No specific state set this cycle, maybe there was a user update pending
  // that should now be processed.
  if (!users_need_to_be_refreshed_ && !auto_logon_credential_) {
    *needs_to_refresh_users = pending_users_refresh_needed_;
    // Can now reset the pending refresh since it has been processed.
    pending_users_refresh_needed_ = false;
  }

  // State has been extracted for use, reset back to the default state.
  InternalReset();
}

void CGaiaCredentialProvider::ProviderConcurrentState::Reset() {
  base::AutoLock locker(state_update_lock_);
  InternalReset();

  // This is a full explicit reset, we also need to reset any pending refresh
  // that may be needed.
  pending_users_refresh_needed_ = false;
}

void CGaiaCredentialProvider::ProviderConcurrentState::InternalReset() {
  users_need_to_be_refreshed_ = false;
  auto_logon_credential_.Reset();
}

CGaiaCredentialProvider::CGaiaCredentialProvider() {}

CGaiaCredentialProvider::~CGaiaCredentialProvider() {}

HRESULT CGaiaCredentialProvider::FinalConstruct() {
  LOGFN(VERBOSE);
  CleanupOlderVersions();
  return S_OK;
}

void CGaiaCredentialProvider::FinalRelease() {
  LOGFN(VERBOSE);
  CHECK(!token_handle_updater_);
  ClearTransient();
  // Unlock all the users that had their access locked due to invalid token
  // handles.
  AssociatedUserValidator::Get()->AllowSigninForUsersWithInvalidTokenHandles();
}

HRESULT CGaiaCredentialProvider::DestroyCredentials() {
  LOGFN(VERBOSE);
  for (auto it = users_.begin(); it != users_.end(); ++it)
    (*it)->Terminate();

  users_.clear();
  return S_OK;
}

void CGaiaCredentialProvider::ClearTransient() {
  LOGFN(VERBOSE);
  CHECK(!token_handle_updater_);
  // Reset event support.
  advise_context_ = 0;
  events_.Reset();
  set_serialization_sid_.clear();
  concurrent_state_.Reset();
  user_array_.Reset();
}

void CGaiaCredentialProvider::CleanupOlderVersions() {
  base::FilePath versions_directory = GetInstallDirectory();
  if (!versions_directory.empty())
    DeleteVersionsExcept(versions_directory, TEXT(CHROME_VERSION_STRING));
}

HRESULT CGaiaCredentialProvider::CreateAnonymousCredentialIfNeeded(
    bool showing_other_user) {
  GaiaCredentialComPtrStorage cred;
  HRESULT hr = E_FAIL;
  if (showing_other_user) {
    hr = CreateCredentialObject<COtherUserGaiaCredential>(
        other_user_cred_creator_, &cred);
  } else if (CanNewUsersBeCreated(cpus_)) {
    hr =
        CreateCredentialObject<CGaiaCredential>(anonymous_cred_creator_, &cred);
  } else {
    return S_OK;
  }

  if (SUCCEEDED(hr)) {
    hr = cred.gaia_cred->Initialize(this);
    if (SUCCEEDED(hr)) {
      AddCredentialAndCheckAutoLogon(cred.gaia_cred, std::wstring(), nullptr);
    } else {
      LOG(ERROR) << "Could not create credential hr=" << putHR(hr);
    }
  }

  return hr;
}

HRESULT CGaiaCredentialProvider::CreateReauthCredentials(
    ICredentialProviderUserArray* users,
    GaiaCredentialComPtrStorage* auto_logon_credential) {
  std::map<std::wstring, std::pair<std::wstring, std::wstring>> sid_to_username;

  // Get the SIDs of all users being shown in the logon UI.
  if (!users) {
    LOGFN(ERROR) << "hr=" << putHR(E_INVALIDARG);
    return E_INVALIDARG;
  }

  HRESULT hr = users->SetProviderFilter(Identity_LocalUserProvider);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "users->SetProviderFilter hr=" << putHR(hr);
    return hr;
  }

  DWORD count;
  hr = users->GetCount(&count);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "users->GetCount hr=" << putHR(hr);
    return hr;
  }

  LOGFN(VERBOSE) << "count=" << count;
  reauth_cred_sids_.clear();

  for (DWORD i = 0; i < count; ++i) {
    Microsoft::WRL::ComPtr<ICredentialProviderUser> user;
    hr = users->GetAt(i, &user);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "users->GetAt hr=" << putHR(hr);
      return hr;
    }

    wchar_t* sid_buffer = nullptr;

    hr = user->GetSid(&sid_buffer);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "user->GetSid hr=" << putHR(hr);
      continue;
    }

    std::wstring sid = sid_buffer;
    ::CoTaskMemFree(sid_buffer);

    wchar_t username[kWindowsUsernameBufferLength];
    wchar_t domain[kWindowsDomainBufferLength];

    hr = OSUserManager::Get()->FindUserBySidWithFallback(
        sid.c_str(), username, std::size(username), domain, std::size(domain));
    if (FAILED(hr)) {
      LOGFN(ERROR) << "Can't get sid or username hr=" << putHR(hr);
      continue;
    }

    // Get the user's gaia id from registry stored against the sid if it
    // exists.
    wchar_t user_id[64];
    ULONG user_id_length = std::size(user_id);
    hr = GetUserProperty(sid.c_str(), kUserId, user_id, &user_id_length);
    if (FAILED(hr))
      user_id[0] = L'\0';

    bool is_token_handle_valid_for_user =
        (!AssociatedUserValidator::Get()->IsAuthEnforcedForUser(sid));

    // (1) If device doesn't have internet and if the device online login
    // attempt is not stale, then don't add the reauth credential.
    // Note: The stale online login attempt is checked only if IT admin
    // configured "validity_period_in_days" registry entry.
    // (2) For a domain joined user, only check for token validity if the
    // user id is not empty. If user id is empty, we should create the
    // reauth credential by default for all AD user sids.
    // (3) For a non-domain joined user, just check if the token handle is
    // valid. If valid, then no need to create a re-auth credential for
    // this sid.
    if (!AssociatedUserValidator::Get()->HasInternetConnection() &&
        !AssociatedUserValidator::Get()->IsOnlineLoginStale(sid)) {
      continue;
    } else if (CGaiaCredentialBase::IsCloudAssociationEnabled() &&
               OSUserManager::Get()->IsUserDomainJoined(sid)) {
      if (user_id[0] && is_token_handle_valid_for_user) {
        continue;
      }
    } else if (is_token_handle_valid_for_user) {
      // If the token handle is valid, no need to create a reauth credential.
      // The user can just sign in using their password.
      continue;
    }

    GaiaCredentialComPtrStorage cred;
    hr = CreateCredentialObject<CReauthCredential>(reauth_cred_creator_, &cred);
    if (FAILED(hr)) {
      LOG(ERROR) << "Could not create credential hr=" << putHR(hr);
      return hr;
    }

    hr =
        InitializeReauthCredential(this, sid, domain, username, cred.gaia_cred);
    if (FAILED(hr)) {
      LOG(ERROR) << "InitializeReauthCredential hr=" << putHR(hr);
      return hr;
    }

    AddCredentialAndCheckAutoLogon(cred.gaia_cred, sid, auto_logon_credential);

    // Add SID to the vector to keep track of all the users that have a reauth
    // credential created.
    reauth_cred_sids_.push_back(sid);

    LOGFN(VERBOSE) << "Reauth SID : " << sid;
  }

  // Deny sign in access for users that have a reauth credential added to them.
  AssociatedUserValidator::Get()->DenySigninForUsersWithInvalidTokenHandles(
      cpus_, reauth_cred_sids_);

  return S_OK;
}

void CGaiaCredentialProvider::AddCredentialAndCheckAutoLogon(
    const Microsoft::WRL::ComPtr<IGaiaCredential>& cred,
    const std::wstring& sid,
    GaiaCredentialComPtrStorage* auto_logon_credential) {
  USES_CONVERSION;
  users_.emplace_back(cred);

  if (!auto_logon_credential)
    return;

  if (sid.empty())
    return;

  if (set_serialization_sid_.empty())
    return;

  // If serialization sid is set, then try to see if this credential is a reauth
  // credential that needs to be auto signed in.
  Microsoft::WRL::ComPtr<IReauthCredential> associated_user;
  if (FAILED(cred.As(&associated_user)))
    return;

  if (set_serialization_sid_ != sid)
    return;

  auto_logon_credential->gaia_cred = cred;
  set_serialization_sid_.clear();
}

void CGaiaCredentialProvider::RecreateCredentials(
    GaiaCredentialComPtrStorage* auto_logon_credential) {
  LOGFN(VERBOSE);
  DCHECK(user_array_);

  DestroyCredentials();

  CREDENTIAL_PROVIDER_ACCOUNT_OPTIONS options;
  HRESULT hr = user_array_->GetAccountOptions(&options);
  bool showing_other_user =
      SUCCEEDED(hr) && options != CPAO_EMPTY_CONNECTED && options != CPAO_NONE;
  hr = CreateAnonymousCredentialIfNeeded(showing_other_user);
  if (FAILED(hr))
    LOG(ERROR) << "Could not create anonymous credential hr=" << putHR(hr);

  hr = CreateReauthCredentials(user_array_.Get(), auto_logon_credential);
  if (FAILED(hr))
    LOG(ERROR) << "CreateReauthCredentials hr=" << putHR(hr);
}

void CGaiaCredentialProvider::SetCredentialCreatorFunctionsForTesting(
    CredentialCreatorFn anonymous_cred_creator,
    CredentialCreatorFn other_user_cred_creator,
    CredentialCreatorFn reauth_cred_creator) {
  DCHECK(!anonymous_cred_creator_);
  DCHECK(!other_user_cred_creator_);
  DCHECK(!reauth_cred_creator_);

  anonymous_cred_creator_ = anonymous_cred_creator;
  other_user_cred_creator_ = other_user_cred_creator;
  reauth_cred_creator_ = reauth_cred_creator;
}

HRESULT CGaiaCredentialProvider::OnUserAuthenticatedImpl(
    IUnknown* credential,
    BSTR /*username*/,
    BSTR /*password*/,
    BSTR sid,
    BOOL fire_credentials_changed) {
  DCHECK(!credential || sid);

  if (!fire_credentials_changed)
    return S_OK;

  // Ensure that user access cannot be denied at this time so that the user
  // that is about to sign in won't be locked. If a ScopedLockDenyAccessUpdate
  // is created before calling this function this should guarantee that
  // situation because the call to BlockDenyAccessUpdate is locked with the
  // same lock that is used in DenySigninForUsersWithInvalidTokenHandles.
  // So either the call to Deny has finished and no new deny will occur
  // afterwards or the Deny will be disabled because the block has been
  // incremented first.
  CHECK(!credential ||
        AssociatedUserValidator::Get()->IsDenyAccessUpdateBlocked());

  Microsoft::WRL::ComPtr<IGaiaCredential> gaia_credential;
  if (credential->QueryInterface(IID_PPV_ARGS(&gaia_credential)) == S_OK) {
    // Try to set the auto logon credential. If it succeeds we can raise a
    // credential changed event.
    if (concurrent_state_.SetAutoLogonCredential(gaia_credential) && events_)
      events_->CredentialsChanged(advise_context_);
  }

  LOGFN(VERBOSE) << "Signing in authenticated sid=" << OLE2CW(sid);
  return S_OK;
}

// Static.
bool CGaiaCredentialProvider::IsUsageScenarioSupported(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus) {
  return cpus == CPUS_LOGON || cpus == CPUS_UNLOCK_WORKSTATION;
}

// Static.
bool CGaiaCredentialProvider::CanNewUsersBeCreated(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus) {
  // When in an unlock usage, only the user that locked the computer will be
  // allowed to sign in, so no new users can be added.
  if (cpus == CPUS_UNLOCK_WORKSTATION)
    return false;

  bool enable_multi_user_login =
      GetGlobalFlagOrDefault(kRegMdmSupportsMultiUser, 1) != 0;
  if (DevicePoliciesManager::Get()->CloudPoliciesEnabled()) {
    DevicePolicies policies;
    DevicePoliciesManager::Get()->GetDevicePolicies(&policies);
    enable_multi_user_login = policies.enable_multi_user_login;
  }

  return enable_multi_user_login ||
         !AssociatedUserValidator::Get()->GetAssociatedUsersCount();
}

// ICredentialUpdateEventsHandler //////////////////////////////////////////////

void CGaiaCredentialProvider::UpdateCredentialsIfNeeded(
    bool user_access_changed) {
  // Defer refresh of the users to the next GetCredentialCount that will be
  // called after the credentials changed event. This prevents potential
  // contention of the |users_| list in multiple places. If the call to
  // RequestUserRefreshIfNeeded returns true, this means we are allowed to
  // proceed with a credentials changed event. Otherwise either no credentials
  // need to be updated (|user_access_changed| == false) or a higher priority
  // update is pending (e.g. auto logon) and thus we cannot send a
  // credentials changed event.
  if (concurrent_state_.RequestUserRefreshIfNeeded(user_access_changed) &&
      events_) {
    events_->CredentialsChanged(advise_context_);
  }
}

// IGaiaCredentialProvider ////////////////////////////////////////////////////

HRESULT CGaiaCredentialProvider::GetUsageScenario(DWORD* cpus) {
  DCHECK(cpus);
  *cpus = static_cast<DWORD>(cpus_);
  return S_OK;
}

HRESULT CGaiaCredentialProvider::OnUserAuthenticated(
    IUnknown* credential,
    BSTR username,
    BSTR password,
    BSTR sid,
    BOOL fire_credentials_changed) {
  return OnUserAuthenticatedImpl(credential, username, password, sid,
                                 fire_credentials_changed);
}

// ICredentialProviderSetUserArray ////////////////////////////////////////////

HRESULT CGaiaCredentialProvider::SetUserArray(
    ICredentialProviderUserArray* users) {
  LOGFN(VERBOSE);
  CHECK(!token_handle_updater_);

  if (!IsUsageScenarioSupported(cpus_))
    return S_OK;

  if (users_.size() > 0) {
    LOG(ERROR) << "Users should be empty";
    return E_UNEXPECTED;
  }

  user_array_ = users;
  // Force refresh of all users on the next GetCredentialCount.
  concurrent_state_.RequestUserRefreshIfNeeded(true);

  return S_OK;
}

// ICredentialProvider ////////////////////////////////////////////////////////

HRESULT CGaiaCredentialProvider::SetUsageScenario(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    DWORD flags) {
  ClearTransient();
  CHECK(!token_handle_updater_);

  cpus_ = cpus;
  cpus_flags_ = flags;

  LOGFN(VERBOSE) << " cpu=" << cpus << " flags=" << std::setbase(16) << flags;
  return IsUsageScenarioSupported(cpus_) ? S_OK : E_NOTIMPL;
}

HRESULT CGaiaCredentialProvider::SetSerialization(
    const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs) {
  DCHECK(pcpcs);
  CHECK(!token_handle_updater_);

  if (pcpcs->clsidCredentialProvider != CLSID_GaiaCredentialProvider)
    return E_NOTIMPL;

  ULONG auth_package_id;
  HRESULT hr = GetAuthenticationPackageId(&auth_package_id);
  if (FAILED(hr))
    return E_NOTIMPL;

  if (pcpcs->ulAuthenticationPackage != auth_package_id)
    return E_NOTIMPL;

  if (pcpcs->cbSerialization == 0)
    return E_NOTIMPL;

  // If serialziation data is set, try to extract the sid for the user
  // referenced in the serialization data.
  hr = DetermineUserSidFromAuthenticationBuffer(pcpcs, &set_serialization_sid_);
  if (FAILED(hr))
    LOGFN(ERROR) << "DetermineUserSidFromAuthenticationBuffer hr=" << putHR(hr);

  return S_OK;
}

HRESULT CGaiaCredentialProvider::Advise(ICredentialProviderEvents* pcpe,
                                        UINT_PTR context) {
  DCHECK(pcpe);
  CHECK(!token_handle_updater_);

  events_ = pcpe;
  advise_context_ = context;

  if (AssociatedUserValidator::Get()->IsUserAccessBlockingEnforced(cpus_)) {
    token_handle_updater_ = std::make_unique<BackgroundTokenHandleUpdater>(
        this, &reauth_cred_sids_);
  }

  return S_OK;
}

HRESULT CGaiaCredentialProvider::UnAdvise() {
  LOGFN(VERBOSE);

  // Kill the updater thread (if any).
  token_handle_updater_.reset();

  ClearTransient();
  DestroyCredentials();

  // Delete the startup sentinel file if any still exists. It can still exist in
  // 2 cases:

  // 1. The UnAdvise should only occur after the user has logged in, so if they
  // never selected any gaia credential and just used normal credentials this
  // function will be called in that situation and it is guaranteed that the
  // user has at least been able provide some input to winlogon.
  // 2. When no usage scenario is supported, none of the credentials will be
  // selected and thus the gcpw startup sentinel file will not be deleted. So in
  // the case where the user is asked for CPUS_CRED_UI enough times, the
  // sentinel file size will keep growing without being deleted and eventually
  // GCPW will be disabled completely. In the unsupported usage scenario,
  // FinalRelease will be called shortly after SetUsageScenario if the function
  // returns E_NOTIMPL so try to catch potential crashes of the destruction of
  // the provider when it is not used because crashes in this case will prevent
  // the cred ui from coming up and not allow the user to access their desired
  // resource.
  DeleteStartupSentinel();

  return S_OK;
}

HRESULT CGaiaCredentialProvider::GetFieldDescriptorCount(DWORD* count) {
  *count = FIELD_COUNT;
  return S_OK;
}

HRESULT CGaiaCredentialProvider::GetFieldDescriptorAt(
    DWORD index,
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** ppcpfd) {
  *ppcpfd = nullptr;
  HRESULT hr = E_INVALIDARG;
  if (index < FIELD_COUNT) {
    // Always return a CoTask copy of the structure as well as any strings
    // pointed to by that structure.
    *ppcpfd = reinterpret_cast<CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR*>(
        ::CoTaskMemAlloc(sizeof(**ppcpfd)));
    if (*ppcpfd) {
      **ppcpfd = g_field_desc[index];
      // The password field has special greyed out text that is not set through
      // calls to ICredentialProviderCredential::GetStringValue so we need to
      // localize it manually here.
      if (index == FID_CURRENT_PASSWORD_FIELD) {
        std::wstring password_label(
            GetStringResource(IDS_WINDOWS_PASSWORD_FIELD_LABEL_BASE));
        hr = ::SHStrDupW(password_label.c_str(), &(*ppcpfd)->pszLabel);
      } else if ((*ppcpfd)->pszLabel) {
        hr = ::SHStrDupW((*ppcpfd)->pszLabel, &(*ppcpfd)->pszLabel);
      } else {
        (*ppcpfd)->pszLabel = nullptr;
        hr = S_OK;
      }
    }

    if (FAILED(hr)) {
      ::CoTaskMemFree(*ppcpfd);
      *ppcpfd = nullptr;
    }
  }

  return hr;
}

HRESULT CGaiaCredentialProvider::GetCredentialCount(
    DWORD* count,
    DWORD* default_index,
    BOOL* autologin_with_default) {
  bool needs_to_refresh_users = false;
  GaiaCredentialComPtrStorage local_auto_logon_credential;

  // Get the mutually exclusive state of the provider so that we can
  // determine the correct next step (recreate credentials or auto logon).
  concurrent_state_.GetUpdatedState(&needs_to_refresh_users,
                                    &local_auto_logon_credential);

  // NOTE: assumes SetUserArray() is called before this.
  if (needs_to_refresh_users)
    RecreateCredentials(&local_auto_logon_credential);

  *count = users_.size();
  *default_index = CREDENTIAL_PROVIDER_NO_DEFAULT;
  *autologin_with_default = false;

  // If a user was authenticated, winlogon was notified of credentials changes
  // and is re-enumerating the credentials.  Make sure autologin is enabled.
  if (local_auto_logon_credential.gaia_cred) {
    // Find the index of the credential that should contain the authentication
    // information.
    for (size_t i = 0;
         i < users_.size() && *default_index == CREDENTIAL_PROVIDER_NO_DEFAULT;
         ++i) {
      if (local_auto_logon_credential.gaia_cred == users_[i])
        *default_index = i;
    }

    *autologin_with_default = *default_index != CREDENTIAL_PROVIDER_NO_DEFAULT;
  }

  LOGFN(VERBOSE) << " count=" << *count << " default=" << *default_index
                 << " auto=" << *autologin_with_default;
  return S_OK;
}

HRESULT CGaiaCredentialProvider::GetCredentialAt(
    DWORD index,
    ICredentialProviderCredential** ppcpc) {
  HRESULT hr = E_INVALIDARG;
  if (!ppcpc || index >= users_.size()) {
    LOG(ERROR) << "hr=" << putHR(hr) << " index=" << index;
    return hr;
  }

  *ppcpc = nullptr;
  hr = users_[index]->QueryInterface(IID_ICredentialProviderCredential,
                                     (void**)ppcpc);

  LOGFN(VERBOSE) << "hr=" << putHR(hr) << " index=" << index;
  return hr;
}

}  // namespace credential_provider
