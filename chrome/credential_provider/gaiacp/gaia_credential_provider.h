// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_PROVIDER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_PROVIDER_H_

#include <wrl/client.h>

#include <limits>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/scoped_handle.h"
#include "chrome/credential_provider/gaiacp/gaia_credential.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"

namespace credential_provider {

class BackgroundTokenHandleUpdater;

// Event handler that can be notified when a user's access has been revoked,
// allowing the credential provider to update the list of available credentials.
class DECLSPEC_UUID("fc2c889b-b468-4eb9-a61c-c984be8cc496")
    ICredentialUpdateEventsHandler : public IUnknown {
 public:
  virtual ~ICredentialUpdateEventsHandler() = default;
  virtual void UpdateCredentialsIfNeeded(bool user_access_changed) = 0;
};

// Implementation of ICredentialProvider backed by Gaia.
class ATL_NO_VTABLE CGaiaCredentialProvider
    : public CComObjectRootEx<CComMultiThreadModel>,
      public CComCoClass<CGaiaCredentialProvider,
                         &CLSID_GaiaCredentialProvider>,
      public IGaiaCredentialProvider,
      public ICredentialProviderSetUserArray,
      public ICredentialProvider,
      public ICredentialUpdateEventsHandler {
 public:
  // This COM object is registered with the rgs file.  The rgs file is used by
  // CGaiaCredentialProviderModule class, see latter for details.
  DECLARE_NO_REGISTRY()

  CGaiaCredentialProvider();
  ~CGaiaCredentialProvider() override;

  BEGIN_COM_MAP(CGaiaCredentialProvider)
  COM_INTERFACE_ENTRY(IGaiaCredentialProvider)
  COM_INTERFACE_ENTRY(ICredentialProviderSetUserArray)
  COM_INTERFACE_ENTRY(ICredentialProvider)
  COM_INTERFACE_ENTRY(ICredentialUpdateEventsHandler)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT FinalConstruct();
  void FinalRelease();

  // ICredentialUpdateEventsHandler:
  void UpdateCredentialsIfNeeded(bool user_access_changed) override;

  // Returns true if the given usage scenario is supported by GCPW. Currently
  // only CPUS_LOGON and CPUS_UNLOCK_WORKSTATION are supported.
  static bool IsUsageScenarioSupported(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus);

  // Returns true if a new user can be added in the current usage scenario. This
  // function also checks other settings controlled by registry settings to
  // determine the result of this query.
  static bool CanNewUsersBeCreated(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus);

  // Struct to allow passing ComPtr by pointer without the implicit conversion
  // to ** version of the ComPtr
  struct GaiaCredentialComPtrStorage {
    GaiaCredentialComPtrStorage();
    ~GaiaCredentialComPtrStorage();
    Microsoft::WRL::ComPtr<IGaiaCredential> gaia_cred;
  };

  typedef HRESULT (*CredentialCreatorFn)(GaiaCredentialComPtrStorage*);

 protected:
  void SetCredentialCreatorFunctionsForTesting(
      CredentialCreatorFn anonymous_cred_creator,
      CredentialCreatorFn other_user_cred_creator,
      CredentialCreatorFn reauth_cred_creator);

  virtual HRESULT OnUserAuthenticatedImpl(IUnknown* credential,
                                          BSTR username,
                                          BSTR password,
                                          BSTR sid,
                                          BOOL fire_credentials_changed);

 private:
  HRESULT DestroyCredentials();

  // Class used to store state information for the provider that may be accessed
  // concurrently. This class is thread safe and ensures that the correct state
  // can be set / queried at any moment. When modifying the state, the modifying
  // function will return true to state that the operation was completed
  // successfully to allow the caller to determine if they need to raise a
  // credential changed event.
  class ProviderConcurrentState {
   public:
    ProviderConcurrentState();
    ~ProviderConcurrentState();

    // Checks if a user refresh can be performed on the next call to
    // |GetCredentialCount|. Normally a user refresh is only needed if
    // |user_access_changed|, but if an auto logon is pending the refresh will
    // need to be deferred. In this case a pending refresh is queued and
    // requested when possible on the next call to RequestUserRefreshIfNeeded.
    // Returns true if a new credential changed event needs to be fired.
    bool RequestUserRefreshIfNeeded(bool user_access_changed);

    // Sets the credential used to auto logon credential. This function will
    // always set the credential as auto logon has precedence over a user
    // refresh. If a user refresh was already requested then it will be changed
    // to a pending refresh request. Returns true if a new credential changed
    // event needs to be fired. A credential changed event is not always
    // required if a previous call to RequestUserRefreshIfNeeded was made that
    // requested a credential changed event.
    bool SetAutoLogonCredential(
        const Microsoft::WRL::ComPtr<IGaiaCredential>& auto_logon_credential);

    // Gets the current valid update state of the provider to determnie whether
    // an auto logon needs to be done or a refresh of the credentials. The two
    // update states are mutually exclusive. Only one of
    // |needs_to_refresh_users| and |auto_logon_credential| can be set to a non
    // default value.
    void GetUpdatedState(bool* needs_to_refresh_users,
                         GaiaCredentialComPtrStorage* auto_logon_credential);

    // Resets the state of the provider to be default one. On the next call to
    // GetCredentialCount no auto logon should be performed and no refresh of
    // credentials should be done.
    void Reset();

   private:
    void InternalReset();

    // Reference to the credential that authenticated the user.
    Microsoft::WRL::ComPtr<IGaiaCredential> auto_logon_credential_;

    // Set in NotifyUserAccessDenied to notify the main thread that it will need
    // to update credentials on the next call to GetCredentialCount. This
    // ensures that |users_| is only accessed on the main thread. This member
    // can only be accessed on multiple threads if |token_handle_updater_| is
    // instantiated, which only occurs between a call to Advise and UnAdvise.
    bool users_need_to_be_refreshed_ = false;

    // Used to queue up user refresh requests that had to be deferred because
    // |auto_logon_credential_| has precedence over
    // |users_need_to_be_refreshed_|.
    bool pending_users_refresh_needed_ = false;

    // Locks access to |users_need_to_be_refreshed_| and
    // |auto_logon_credential_| to prevent races between calls to
    // OnUserAuthenticated / GetCredentialCount and calls to
    // NotifyUserAccessDenied.
    base::Lock state_update_lock_;
  };

  // Functions to create credentials during the processing of SetUserArray.

  // Creates necessary anonymous credentials given the state of the sign in
  // screen (currently only whether |showing_other_user| set influences this
  // behavior.
  HRESULT CreateAnonymousCredentialIfNeeded(bool showing_other_user);

  // Creates all the reauth credentials from the users that is returned from
  // |users|. Fills the |gaia_cred| in |auto_logon_credential| with a reference
  // to the credential that needs to perform auto logon (if any).
  HRESULT CreateReauthCredentials(
      ICredentialProviderUserArray* users,
      GaiaCredentialComPtrStorage* auto_logon_credential);

  // This function will always add |cred| to |users_| and will also try to check
  // if the |sid| matches the one set in |set_serialization_sid_| to allow auto
  // logon of remote connections. Fills the |gaia_cred| in
  // |auto_logon_credential| with a reference to the credential that needs to
  // perform auto logon (if any).
  void AddCredentialAndCheckAutoLogon(
      const Microsoft::WRL::ComPtr<IGaiaCredential>& cred,
      const std::wstring& sid,
      GaiaCredentialComPtrStorage* auto_logon_credential);

  // Destroys existing credentials and recreates them based on the contents of
  // |user_array_|. This member must be set via a call to SetUserArray before
  // RecreateCredentials is called. Fills the |gaia_cred| in
  // |auto_logon_credential| with a reference to the credential that needs to
  // perform auto logon (if any).
  void RecreateCredentials(GaiaCredentialComPtrStorage* auto_logon_credential);

  void ClearTransient();
  void CleanupOlderVersions();

  // IGaiaCredentialProvider
  IFACEMETHODIMP GetUsageScenario(DWORD* cpus) override;
  IFACEMETHODIMP OnUserAuthenticated(IUnknown* credential,
                                     BSTR username,
                                     BSTR password,
                                     BSTR sid,
                                     BOOL fire_credentials_changed) override;

  // ICredentialProviderSetUserArray
  IFACEMETHODIMP SetUserArray(ICredentialProviderUserArray* users) override;

  // ICredentialProvider
  IFACEMETHODIMP SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
                                  DWORD dwFlags) override;
  IFACEMETHODIMP SetSerialization(
      const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs) override;
  IFACEMETHODIMP Advise(ICredentialProviderEvents* pcpe,
                        UINT_PTR upAdviseContext) override;
  IFACEMETHODIMP UnAdvise() override;
  IFACEMETHODIMP GetFieldDescriptorCount(DWORD* pdwCount) override;
  IFACEMETHODIMP GetFieldDescriptorAt(
      DWORD dwIndex,
      CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** ppcpfd) override;
  IFACEMETHODIMP GetCredentialCount(DWORD* pdwCount,
                                    DWORD* pdwDefault,
                                    BOOL* pbAutoLogonWithDefault) override;
  IFACEMETHODIMP GetCredentialAt(
      DWORD dwIndex,
      ICredentialProviderCredential** ppcpc) override;

  CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus_ = CPUS_INVALID;
  DWORD cpus_flags_ = 0;
  UINT_PTR advise_context_;
  Microsoft::WRL::ComPtr<ICredentialProviderEvents> events_;
  Microsoft::WRL::ComPtr<ICredentialProviderUserArray> user_array_;

  // List of credentials exposed by this provider.  The first is always the
  // Gaia credential for creating new users.  The rest are reauth credentials.
  std::vector<Microsoft::WRL::ComPtr<IGaiaCredential>> users_;

  // Reference to the credential that authenticated the user.
  Microsoft::WRL::ComPtr<IGaiaCredential> auto_logon_credential_;

  // Background thread updater of token handles that is created on startup to
  // ensure that user must sign in through gaia if their token handle becomes
  // invalid while idle on the sign in screen. The updater will also handle the
  // situation where the machine becomes unenrolled from MDM during sign in.
  std::unique_ptr<BackgroundTokenHandleUpdater> token_handle_updater_;

  // The SID extracted from the serialization information passed into
  // SetSerialization. This sid is used to determine which credential to try
  // to auto logon when GetCredentialCount is called.
  std::wstring set_serialization_sid_;

  ProviderConcurrentState concurrent_state_;

  CredentialCreatorFn anonymous_cred_creator_ = nullptr;
  CredentialCreatorFn other_user_cred_creator_ = nullptr;
  CredentialCreatorFn reauth_cred_creator_ = nullptr;
  std::vector<std::wstring> reauth_cred_sids_;
};

// OBJECT_ENTRY_AUTO() contains an extra semicolon.
// TODO(thakis): Make -Wextra-semi not warn on semicolons that are from a
// macro in a system header, then remove the pragma, https://llvm.org/PR40874
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi"
#endif

OBJECT_ENTRY_AUTO(__uuidof(GaiaCredentialProvider), CGaiaCredentialProvider)

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_PROVIDER_H_
