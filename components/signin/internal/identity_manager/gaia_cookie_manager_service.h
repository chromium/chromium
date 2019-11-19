// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_GAIA_COOKIE_MANAGER_SERVICE_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_GAIA_COOKIE_MANAGER_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/signin_client.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/backoff_entry.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

class GaiaAuthFetcher;
class GaiaCookieRequest;
class GoogleServiceAuthError;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace signin {

class OAuthMultiloginHelper;
class UbertokenFetcherImpl;
enum class SetAccountsInCookieResult;

}  // namespace signin

// Merges a Google account known to Chrome into the cookie jar.  When merging
// multiple accounts, one instance of the helper is better than multiple
// instances if there is the possibility that they run concurrently, since
// changes to the cookie must be serialized.
//
// Also checks the External CC result to ensure no services that consume the
// GAIA cookie are blocked (such as youtube). This is executed once for the
// lifetime of this object, when the first call is made to AddAccountToCookie.
class GaiaCookieManagerService : public GaiaAuthConsumer,
                                 public network::mojom::CookieChangeListener {
 public:
  using AccountIdGaiaIdPair = std::pair<CoreAccountId, std::string>;

  enum GaiaCookieRequestType {
    ADD_ACCOUNT,
    LOG_OUT,
    LIST_ACCOUNTS,
    SET_ACCOUNTS
  };

  typedef base::OnceCallback<void(signin::SetAccountsInCookieResult)>
      SetAccountsInCookieCompletedCallback;
  typedef base::OnceCallback<void(const CoreAccountId&,
                                  const GoogleServiceAuthError&)>
      AddAccountToCookieCompletedCallback;

  typedef base::RepeatingCallback<void(const std::vector<gaia::ListedAccount>&,
                                       const std::vector<gaia::ListedAccount>&,
                                       const GoogleServiceAuthError&)>
      GaiaAccountsInCookieUpdatedCallback;
  typedef base::RepeatingCallback<void()> GaiaCookieDeletedByUserActionCallback;

  // Contains the information and parameters for any request.
  class GaiaCookieRequest {
   public:
    ~GaiaCookieRequest();
    GaiaCookieRequest(GaiaCookieRequest&&);
    GaiaCookieRequest& operator=(GaiaCookieRequest&&);

    GaiaCookieRequestType request_type() const { return request_type_; }

    // For use in the Request of type SET_ACCOUNTS.
    const std::vector<AccountIdGaiaIdPair>& GetAccounts() const;
    gaia::MultiloginMode GetMultiloginMode() const;

    // For use in the Request of type ADD_ACCOUNT which must have exactly one
    // account_id.
    const CoreAccountId GetAccountID();
    gaia::GaiaSource source() const { return source_; }
    // Sets GaiaSource suffix.
    void SetSourceSuffix(std::string suffix);

    void RunSetAccountsInCookieCompletedCallback(
        signin::SetAccountsInCookieResult result);
    void RunAddAccountToCookieCompletedCallback(
        const CoreAccountId& account_id,
        const GoogleServiceAuthError& error);

    static GaiaCookieRequest CreateAddAccountRequest(
        const CoreAccountId& account_id,
        gaia::GaiaSource source,
        AddAccountToCookieCompletedCallback callback);
    static GaiaCookieRequest CreateLogOutRequest(gaia::GaiaSource source);
    static GaiaCookieRequest CreateListAccountsRequest();
    static GaiaCookieRequest CreateSetAccountsRequest(
        gaia::MultiloginMode mode,
        const std::vector<AccountIdGaiaIdPair>& account_ids,
        gaia::GaiaSource source,
        SetAccountsInCookieCompletedCallback callback);

   private:
    // Parameters for the SET_ACCOUNTS requests.
    struct SetAccountsParams {
      SetAccountsParams();
      SetAccountsParams(const SetAccountsParams& other);
      ~SetAccountsParams();

      gaia::MultiloginMode mode;
      std::vector<AccountIdGaiaIdPair> accounts;
    };

    GaiaCookieRequest(GaiaCookieRequestType request_type,
                      gaia::GaiaSource source);

    GaiaCookieRequestType request_type_;
    // For use in the request of type ADD_ACCOUNT.
    CoreAccountId account_id_;
    // For use in the request of type SET_ACCOUNT.
    SetAccountsParams set_accounts_params_;

    gaia::GaiaSource source_;

    SetAccountsInCookieCompletedCallback
        set_accounts_in_cookie_completed_callback_;
    AddAccountToCookieCompletedCallback
        add_account_to_cookie_completed_callback_;

    DISALLOW_COPY_AND_ASSIGN(GaiaCookieRequest);
  };

  // Class to retrieve the external connection check results from gaia.
  // Declared publicly for unit tests.
  class ExternalCcResultFetcher : public GaiaAuthConsumer {
   public:
    // Maps connection check SimpleURLLoader to corresponding token.
    typedef std::map<const network::SimpleURLLoader*, std::string>
        LoaderToToken;

    // Maps tokens to the fetched result for that token.
    typedef std::map<std::string, std::string> ResultMap;

    explicit ExternalCcResultFetcher(GaiaCookieManagerService* helper);
    ~ExternalCcResultFetcher() override;

    // Gets the current value of the external connection check result string.
    std::string GetExternalCcResult();

    // Start fetching the external CC result.  If a fetch is already in progress
    // it is canceled.
    void Start(base::OnceClosure callback);

    // Are external URLs still being checked?
    bool IsRunning();

    // Returns a copy of the internal loader to token map.
    LoaderToToken get_loader_map_for_testing() { return loaders_; }

    // Simulate a timeout for tests.
    void TimeoutForTests();

   private:
    // Overridden from GaiaAuthConsumer.
    void OnGetCheckConnectionInfoSuccess(const std::string& data) override;
    void OnGetCheckConnectionInfoError(
        const GoogleServiceAuthError& error) override;

    // Creates and initializes a loader for doing a connection check.
    std::unique_ptr<network::SimpleURLLoader> CreateAndStartLoader(
        const GURL& url);

    // Called back from SimpleURLLoader.
    void OnURLLoadComplete(const network::SimpleURLLoader* source,
                           std::unique_ptr<std::string> body);

    // Any fetches still ongoing after this call are considered timed out.
    void Timeout();

    void CleanupTransientState();

    void GetCheckConnectionInfoCompleted(bool succeeded);

    GaiaCookieManagerService* helper_;
    base::OneShotTimer timer_;
    LoaderToToken loaders_;
    ResultMap results_;
    base::Time m_external_cc_result_start_time_;
    base::OnceClosure callback_;

    DISALLOW_COPY_AND_ASSIGN(ExternalCcResultFetcher);
  };

  GaiaCookieManagerService(ProfileOAuth2TokenService* token_service,
                           SigninClient* signin_client);

  ~GaiaCookieManagerService() override;

  void InitCookieListener();
  void Shutdown();

  void AddAccountToCookie(
      const CoreAccountId& account_id,
      gaia::GaiaSource source,
      AddAccountToCookieCompletedCallback completion_callback);
  void AddAccountToCookieWithToken(
      const CoreAccountId& account_id,
      const std::string& access_token,
      gaia::GaiaSource source,
      AddAccountToCookieCompletedCallback completion_callback);

  // Takes list of account_ids and sets the cookie for these accounts regardless
  // of the current cookie state. Removes the accounts that are not in
  // account_ids and add the missing ones.
  void SetAccountsInCookie(gaia::MultiloginMode mode,
                           const std::vector<AccountIdGaiaIdPair>& account_ids,
                           gaia::GaiaSource source,
                           SetAccountsInCookieCompletedCallback
                               set_accounts_in_cookies_completed_callback);

  // Returns if the listed accounts are up to date or not. The out parameter
  // will be assigned the current cached accounts (whether they are not up to
  // date or not). If the accounts are not up to date, a ListAccounts fetch is
  // sent GAIA and Observer::OnGaiaAccountsInCookieUpdated will be called.  If
  // either of |accounts| or |signed_out_accounts| is null, the corresponding
  // accounts returned from /ListAccounts are ignored.
  bool ListAccounts(std::vector<gaia::ListedAccount>* accounts,
                    std::vector<gaia::ListedAccount>* signed_out_accounts);

  // Triggers a ListAccounts fetch. This is public so that callers that know
  // that a check which GAIA should be done can force it.
  void TriggerListAccounts();

  // Forces the processing of OnCookieChange. This is public so that callers
  // that know the GAIA SAPISID cookie might have changed can inform the
  // service. Virtual for testing.
  virtual void ForceOnCookieChangeProcessing();

  // Cancel all login requests.
  void CancelAll();

  // Signout all accounts.
  void LogOutAllAccounts(gaia::GaiaSource source);

  // Call observers when setting accounts in cookie completes.
  void SignalSetAccountsComplete(signin::SetAccountsInCookieResult result);

  // Returns true of there are pending log ins or outs.
  bool is_running() const { return requests_.size() > 0; }

  // Access the internal object during tests.
  ExternalCcResultFetcher* external_cc_result_fetcher_for_testing() {
    return &external_cc_result_fetcher_;
  }

  void set_list_accounts_stale_for_testing(bool stale) {
    list_accounts_stale_ = stale;
  }

  // If set, this callback will be invoked whenever the
  // GaiaCookieManagerService's list of GAIA accounts is updated. The GCMS
  // monitors the SAPISID cookie and triggers a /ListAccounts call on change.
  // The GCMS will also call ListAccounts upon the first call to
  // ListAccounts(). The GCMS will delay calling ListAccounts if other
  // requests are in queue that would modify the SAPISID cookie.
  // If the ListAccounts call fails and the GCMS cannot recover, the reason
  // is passed in |error|.
  // This method can only be called once.
  void SetGaiaAccountsInCookieUpdatedCallback(
      GaiaAccountsInCookieUpdatedCallback callback);

  // If set, this callback will be invoked whenever the Gaia cookie has
  // been deleted explicitly by a user action, e.g. from the settings or by an
  // extension.
  // This method can only be called once.
  void SetGaiaCookieDeletedByUserActionCallback(
      GaiaCookieDeletedByUserActionCallback callback);

  // Returns a non-null pointer to its instance of net::BackoffEntry
  const net::BackoffEntry* GetBackoffEntry() { return &fetcher_backoff_; }

  // Ubertoken fetch completion callback. Called by unittests directly.
  void OnUbertokenFetchComplete(GoogleServiceAuthError error,
                                const std::string& uber_token);

  // Final call in the Setting accounts in cookie procedure. Public for testing.
  void OnSetAccountsFinished(signin::SetAccountsInCookieResult result);

  // Registers prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  // Calls the AddAccountToCookie completion callback.
  void SignalAddToCookieComplete(
      const base::circular_deque<GaiaCookieRequest>::iterator& request,
      const GoogleServiceAuthError& error);

  // Marks the list account being staled, and for iOS only, it triggers to fetch
  // the list of accounts (on iOS there is no OnCookieChange() notification).
  void MarkListAccountsStale();

  // Overridden from network::mojom::CookieChangeListner. If the cookie relates
  // to a GAIA APISID cookie, then we call ListAccounts and fire
  // OnGaiaAccountsInCookieUpdated.
  void OnCookieChange(const net::CookieChangeInfo& change) override;
  void OnCookieListenerConnectionError();

  // Overridden from GaiaAuthConsumer.
  void OnMergeSessionSuccess(const std::string& data) override;
  void OnMergeSessionFailure(const GoogleServiceAuthError& error) override;
  void OnListAccountsSuccess(const std::string& data) override;
  void OnListAccountsFailure(const GoogleServiceAuthError& error) override;
  void OnLogOutSuccess() override;
  void OnLogOutFailure(const GoogleServiceAuthError& error) override;

  // Helper method to initialize listed accounts ids.
  void InitializeListedAccountsIds();

  // Helper method for AddAccountToCookie* methods.
  void AddAccountToCookieInternal(
      const CoreAccountId& account_id,
      gaia::GaiaSource source,
      AddAccountToCookieCompletedCallback completion_callback);

  // Starts the proess of fetching the uber token and performing a merge session
  // for the next account.  Virtual so that it can be overriden in tests.
  virtual void StartFetchingUbertoken();

  // Virtual for testing purposes.
  virtual void StartFetchingMergeSession();

  // Virtual for testing purposes.
  virtual void StartFetchingListAccounts();

  // Prepare for logout and then starts fetching logout request.
  void StartGaiaLogOut();

  // Starts fetching log out.
  // Virtual for testing purpose.
  virtual void StartFetchingLogOut();

  // Starts setting account using multilogin endpoint.
  void StartSetAccounts();

  // Start the next request, if needed.
  void HandleNextRequest();

  ProfileOAuth2TokenService* token_service_;
  SigninClient* signin_client_;

  GaiaAccountsInCookieUpdatedCallback gaia_accounts_updated_in_cookie_callback_;
  GaiaCookieDeletedByUserActionCallback
      gaia_cookie_deleted_by_user_action_callback_;

  std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
  std::unique_ptr<signin::UbertokenFetcherImpl> uber_token_fetcher_;
  ExternalCcResultFetcher external_cc_result_fetcher_;
  std::unique_ptr<signin::OAuthMultiloginHelper> oauth_multilogin_helper_;

  // If the GaiaAuthFetcher or SimpleURLLoader fails, retry with exponential
  // backoff and network delay.
  net::BackoffEntry fetcher_backoff_;
  base::OneShotTimer fetcher_timer_;
  int fetcher_retries_;

  // If list accounts retried after a failure because of getting an unexpected
  // service response.
  bool listAccountsUnexpectedServerResponseRetried_;

  // The last fetched ubertoken, for use in MergeSession retries.
  std::string uber_token_;

  // The access token that can be used to prime the UberToken fetch.
  std::string access_token_;

  // Connection to the CookieManager that signals when the GAIA cookies change.
  mojo::Receiver<network::mojom::CookieChangeListener>
      cookie_listener_receiver_{this};

  // A worklist for this class. Stores any pending requests that couldn't be
  // executed right away, since this class only permits one request to be
  // executed at a time.
  base::circular_deque<GaiaCookieRequest> requests_;

  // True once the ExternalCCResultFetcher has completed once.
  bool external_cc_result_fetched_;

  std::vector<gaia::ListedAccount> listed_accounts_;
  std::vector<gaia::ListedAccount> signed_out_accounts_;

  bool list_accounts_stale_;

  base::WeakPtrFactory<GaiaCookieManagerService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GaiaCookieManagerService);
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_GAIA_COOKIE_MANAGER_SERVICE_H_
