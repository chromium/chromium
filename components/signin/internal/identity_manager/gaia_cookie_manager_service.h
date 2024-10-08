// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_GAIA_COOKIE_MANAGER_SERVICE_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_GAIA_COOKIE_MANAGER_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
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
enum class SetAccountsInCookieResult;

}  // namespace signin

// Merges a Google account known to Chrome into the cookie jar.  When merging
// multiple accounts, one instance of the helper is better than multiple
// instances if there is the possibility that they run concurrently, since
// changes to the cookie must be serialized.
//
// Also checks the External CC result to ensure no services that consume the
// GAIA cookie are blocked (such as youtube). This is executed once for the
// lifetime of this object, when the first call is made to SetAccountsInCookie.
class GaiaCookieManagerService
    : public GaiaAuthConsumer,
      public signin::AccountsCookieMutator::PartitionDelegate,
      public network::mojom::CookieChangeListener {
 public:
  using AccountIdGaiaIdPair = std::pair<CoreAccountId, std::string>;

  enum GaiaCookieRequestType { LOG_OUT, LIST_ACCOUNTS, SET_ACCOUNTS };

  typedef base::OnceCallback<void(signin::SetAccountsInCookieResult)>
      SetAccountsInCookieCompletedCallback;
  typedef base::OnceCallback<void(const GoogleServiceAuthError&)>
      LogOutFromCookieCompletedCallback;

  typedef base::RepeatingCallback<void(const signin::AccountsInCookieJarInfo&,
                                       const GoogleServiceAuthError&)>
      GaiaAccountsInCookieUpdatedCallback;
  typedef base::RepeatingCallback<void()> GaiaCookieDeletedByUserActionCallback;

  // Contains the information and parameters for any request.
  class GaiaCookieRequest {
   public:
    GaiaCookieRequest(const GaiaCookieRequest&) = delete;
    GaiaCookieRequest& operator=(const GaiaCookieRequest&) = delete;

    ~GaiaCookieRequest();
    GaiaCookieRequest(GaiaCookieRequest&&);
    GaiaCookieRequest& operator=(GaiaCookieRequest&&);

    GaiaCookieRequestType request_type() const { return request_type_; }

    // For use in the Request of type SET_ACCOUNTS.
    const std::vector<AccountIdGaiaIdPair>& GetAccounts() const;
    gaia::MultiloginMode GetMultiloginMode() const;

    gaia::GaiaSource source() const { return source_; }
    // Sets GaiaSource suffix.
    void SetSourceSuffix(std::string suffix);

    void RunSetAccountsInCookieCompletedCallback(
        signin::SetAccountsInCookieResult result);
    void RunLogOutFromCookieCompletedCallback(
        const GoogleServiceAuthError& error);

    static GaiaCookieRequest CreateLogOutRequest(
        gaia::GaiaSource source,
        LogOutFromCookieCompletedCallback callback);
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
    // For use in the request of type SET_ACCOUNT.
    SetAccountsParams set_accounts_params_;

    gaia::GaiaSource source_;

    SetAccountsInCookieCompletedCallback
        set_accounts_in_cookie_completed_callback_;
    LogOutFromCookieCompletedCallback log_out_from_cookie_completed_callback_;
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

    ExternalCcResultFetcher(const ExternalCcResultFetcher&) = delete;
    ExternalCcResultFetcher& operator=(const ExternalCcResultFetcher&) = delete;

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

    raw_ptr<GaiaCookieManagerService> helper_;
    base::OneShotTimer timer_;
    LoaderToToken loaders_;
    ResultMap results_;
    base::Time m_external_cc_result_start_time_;
    base::OnceClosure callback_;
  };

  GaiaCookieManagerService(AccountTrackerService* account_tracker_service_,
                           ProfileOAuth2TokenService* token_service,
                           SigninClient* signin_client);

  GaiaCookieManagerService(const GaiaCookieManagerService&) = delete;
  GaiaCookieManagerService& operator=(const GaiaCookieManagerService&) = delete;

  ~GaiaCookieManagerService() override;

  void InitCookieListener();

  // Takes list of account_ids and sets the cookie for these accounts regardless
  // of the current cookie state. Removes the accounts that are not in
  // account_ids and add the missing ones.
  void SetAccountsInCookie(gaia::MultiloginMode mode,
                           const std::vector<AccountIdGaiaIdPair>& account_ids,
                           gaia::GaiaSource source,
                           SetAccountsInCookieCompletedCallback
                               set_accounts_in_cookies_completed_callback);

  // Returns an object that has methods to get listed accounts and check whether
  // they are up to date or not. If the accounts are not up to date, a
  // ListAccounts fetch is sent GAIA and Observer::OnGaiaAccountsInCookieUpdated
  // will be called.
  signin::AccountsInCookieJarInfo ListAccounts();

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
  // Note: this only clears the Gaia cookies. Other cookies such as the SAML
  // provider cookies are not cleared. To cleanly remove an account from the
  // web, the Gaia logout page should be loaded as a navigation.
  void LogOutAllAccounts(gaia::GaiaSource source,
                         LogOutFromCookieCompletedCallback callback);

  // Indicates that an account previously listed via ListAccounts should now
  // be removed. Does not trigger a ListAccounts request and does not change the
  // staleness of the account information.
  void RemoveLoggedOutAccountByGaiaId(const std::string& gaia_id);

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

  // If set, this callback will be invoked whenever one of the Gaia cookies has
  // been deleted explicitly by a user action, e.g. from the settings or by an
  // extension.
  // This method can only be called once.
  void SetGaiaCookieDeletedByUserActionCallback(
      GaiaCookieDeletedByUserActionCallback callback);

  // Returns a non-null pointer to its instance of net::BackoffEntry
  const net::BackoffEntry* GetBackoffEntry() { return &fetcher_backoff_; }

  // Final call in the Setting accounts in cookie procedure. Public for testing.
  void OnSetAccountsFinished(signin::SetAccountsInCookieResult result);

  // Registers prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  FRIEND_TEST_ALL_PREFIXES(GaiaCookieManagerServiceCookieTest, CookieChange);
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  // Calls the LogOutFromCookie completion callback.
  void SignalLogOutComplete(const GoogleServiceAuthError& error);

  // Marks the list account being staled, and for iOS only, it triggers to fetch
  // the list of accounts (on iOS there is no OnCookieChange() notification).
  void MarkListAccountsStale();

  // Overridden from network::mojom::CookieChangeListner. If the cookie relates
  // to a GAIA APISID cookie, then we call ListAccounts and fire
  // OnGaiaAccountsInCookieUpdated.
  void OnCookieChange(const net::CookieChangeInfo& change) override;
  void OnCookieListenerConnectionError();

  // Overridden from GaiaAuthConsumer.
  void OnListAccountsSuccess(const std::string& data) override;
  void OnListAccountsFailure(const GoogleServiceAuthError& error) override;
  void OnLogOutSuccess() override;
  void OnLogOutFailure(const GoogleServiceAuthError& error) override;

  // Overridden from signin::AccountsCookieMutator::PartitionDelegate.
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcherForPartition(
      GaiaAuthConsumer* consumer,
      const gaia::GaiaSource& source) override;
  network::mojom::CookieManager* GetCookieManagerForPartition() override;

  // Helper method to initialize listed accounts ids.
  void InitializeListedAccountsIds();

  // Virtual for testing purposes.
  virtual void StartFetchingListAccounts();

  // Prepare for logout and then starts fetching logout request.
  // Virtual for testing purpose.
  virtual void StartGaiaLogOut();

  // Starts setting account using multilogin endpoint.
  virtual void StartSetAccounts();

  // Start the next request, if needed.
  void HandleNextRequest();
  // Deduplicate list accounts requests.
  // If logout or set accounts operation is in the requests queue, it moves list
  // accounts request to the end of the queue.
  // This function should be called before the front of the queue is in
  // execution.
  void OptimizeListAccounts();

  const raw_ptr<AccountTrackerService> account_tracker_service_ = nullptr;
  raw_ptr<ProfileOAuth2TokenService> token_service_;
  raw_ptr<SigninClient> signin_client_;

  GaiaAccountsInCookieUpdatedCallback gaia_accounts_updated_in_cookie_callback_;
  GaiaCookieDeletedByUserActionCallback
      gaia_cookie_deleted_by_user_action_callback_;

  std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
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

  // Connection to the CookieManager that signals when the GAIA cookies change.
  mojo::Receiver<network::mojom::CookieChangeListener>
      cookie_listener_receiver_{this};

  // A worklist for this class. Stores any pending requests that couldn't be
  // executed right away, since this class only permits one request to be
  // executed at a time.
  base::circular_deque<GaiaCookieRequest> requests_;

  // True once the ExternalCCResultFetcher has completed once.
  bool external_cc_result_fetched_;

  std::vector<gaia::ListedAccount> accounts_;

  bool list_accounts_stale_;

  base::WeakPtrFactory<GaiaCookieManagerService> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_GAIA_COOKIE_MANAGER_SERVICE_H_
