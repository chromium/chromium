// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_GAIA_COOKIE_MANAGER_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_GAIA_COOKIE_MANAGER_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/signin/core/browser/signin_client.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/oauth_multilogin_result.h"
#include "google_apis/gaia/ubertoken_fetcher.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

class GaiaAuthFetcher;
class GaiaCookieRequest;
class GoogleServiceAuthError;
class OAuth2TokenService;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}

namespace signin {
// The maximum number of retries for a fetcher used in this class.
constexpr int kMaxFetcherRetries = 8;

struct MultiloginParameters {
  MultiloginParameters(const MultiloginMode mode,
                       const std::vector<std::string>& accounts_to_send);
  MultiloginParameters(const MultiloginParameters& other);
  MultiloginParameters& operator=(const MultiloginParameters& other);
  ~MultiloginParameters();

  // Needed for testing.
  bool operator==(const MultiloginParameters& other) const {
    return mode == other.mode && accounts_to_send == other.accounts_to_send;
  }

  MultiloginMode mode;
  std::vector<std::string> accounts_to_send;
};
}  // namespace signin

// Merges a Google account known to Chrome into the cookie jar.  When merging
// multiple accounts, one instance of the helper is better than multiple
// instances if there is the possibility that they run concurrently, since
// changes to the cookie must be serialized.
//
// Also checks the External CC result to ensure no services that consume the
// GAIA cookie are blocked (such as youtube). This is executed once for the
// lifetime of this object, when the first call is made to AddAccountToCookie.
class GaiaCookieManagerService : public KeyedService,
                                 public GaiaAuthConsumer,
                                 public UbertokenConsumer,
                                 public network::mojom::CookieChangeListener,
                                 public OAuth2TokenService::Consumer {
 public:
  enum GaiaCookieRequestType {
    ADD_ACCOUNT,
    LOG_OUT,
    LIST_ACCOUNTS,
    SET_ACCOUNTS
  };

  // Contains the information and parameters for any request.
  class GaiaCookieRequest {
   public:
    GaiaCookieRequest(const GaiaCookieRequest& other);
    ~GaiaCookieRequest();

    GaiaCookieRequestType request_type() const { return request_type_; }
    const std::vector<std::string>& account_ids() const { return account_ids_; }
    // For use in the Request of type ADD_ACCOUNT which must have exactly one
    // account_id in the array. It checks this condition and extracts this one
    // account.
    const std::string GetAccountID();
    const std::string& source() const {return source_; }

    static GaiaCookieRequest CreateAddAccountRequest(
        const std::string& account_id,
        const std::string& source);
    static GaiaCookieRequest CreateLogOutRequest(const std::string& source);
    static GaiaCookieRequest CreateListAccountsRequest(
        const std::string& source);
    static GaiaCookieRequest CreateSetAccountsRequest(
        const std::vector<std::string>& account_ids,
        const std::string& source);

   private:
    GaiaCookieRequest(GaiaCookieRequestType request_type,
                      const std::vector<std::string>& account_ids,
                      const std::string& source);

    GaiaCookieRequestType request_type_;
    std::vector<std::string> account_ids_;
    std::string source_;
  };

  class Observer {
   public:
    // Called whenever a merge session is completed.  The account that was
    // merged is given by |account_id|.  If |error| is equal to
    // GoogleServiceAuthError::AuthErrorNone() then the merge succeeded.
    virtual void OnAddAccountToCookieCompleted(
        const std::string& account_id,
        const GoogleServiceAuthError& error) {}

    // Called whenever setting cookies is completed. If |error| is equal to
    // GoogleServiceAuthError::AuthErrorNone() then the call succeeded although
    // there still might be some cookies that failed to be set.
    virtual void OnSetAccountsInCookieCompleted(
        const GoogleServiceAuthError& error) {}

    // Called whenever a logout is completed. If |error| is equal to
    // GoogleServiceAuthError::AuthErrorNone() then the logout succeeded.
    virtual void OnLogOutAccountsFromCookieCompleted(
        const GoogleServiceAuthError& error) {}

    // Called whenever the GaiaCookieManagerService's list of GAIA accounts is
    // updated. The GCMS monitors the APISID cookie and triggers a /ListAccounts
    // call on change. The GCMS will also call ListAccounts upon the first call
    // to ListAccounts(). The GCMS will delay calling ListAccounts if other
    // requests are in queue that would modify the APISID cookie.
    // If the ListAccounts call fails and the GCMS cannot recover, the reason
    // is passed in |error|.
    virtual void OnGaiaAccountsInCookieUpdated(
        const std::vector<gaia::ListedAccount>& accounts,
        const std::vector<gaia::ListedAccount>& signed_out_accounts,
        const GoogleServiceAuthError& error) {}

    // Called when the Gaia cookie has been deleted explicitly by a user action,
    // e.g. from the settings or by an extension.
    virtual void OnGaiaCookieDeletedByUserAction() {}

   protected:
    virtual ~Observer() {}
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
    void Start();

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

    DISALLOW_COPY_AND_ASSIGN(ExternalCcResultFetcher);
  };

  GaiaCookieManagerService(OAuth2TokenService* token_service,
                           const std::string& source,
                           SigninClient* signin_client);
  ~GaiaCookieManagerService() override;

  void InitCookieListener();
  void Shutdown() override;

  void AddAccountToCookie(const std::string& account_id,
                          const std::string& source);
  void AddAccountToCookieWithToken(const std::string& account_id,
                                   const std::string& access_token,
                                   const std::string& source);

  // Takes list of account_ids and sets the cookie for these accounts regardless
  // of the current cookie state. Removes the accounts that are not in
  // account_ids and add the missing ones.
  void SetAccountsInCookie(const std::vector<std::string>& account_ids,
                           const std::string& source);

  // Takes list of account_ids from the front request, matches them with a
  // corresponding stored access_token and calls StartMultilogin.
  // Virtual for testing purposes.
  virtual void SetAccountsInCookieWithTokens();

  // Returns if the listed accounts are up to date or not. The out parameter
  // will be assigned the current cached accounts (whether they are not up to
  // date or not). If the accounts are not up to date, a ListAccounts fetch is
  // sent GAIA and Observer::OnGaiaAccountsInCookieUpdated will be called.  If
  // either of |accounts| or |signed_out_accounts| is null, the corresponding
  // accounts returned from /ListAccounts are ignored.
  bool ListAccounts(std::vector<gaia::ListedAccount>* accounts,
                    std::vector<gaia::ListedAccount>* signed_out_accounts,
                    const std::string& source);

  // Triggers a ListAccounts fetch. This is public so that callers that know
  // that a check which GAIA should be done can force it.
  void TriggerListAccounts(const std::string& source);

  // Forces the processing of OnCookieChange. This is public so that callers
  // that know the GAIA APISID cookie might have changed can inform the
  // service. Virtual for testing.
  virtual void ForceOnCookieChangeProcessing();

  // Add or remove observers of this helper.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Cancel all login requests.
  void CancelAll();

  // Signout all accounts.
  void LogOutAllAccounts(const std::string& source);

  // Call observers when merge session completes.  This public so that callers
  // that know that a given account is already in the cookie jar can simply
  // inform the observers.
  void SignalComplete(const std::string& account_id,
                      const GoogleServiceAuthError& error);

  // Call observers when setting accounts in cookie completes.
  void SignalSetAccountsComplete(const GoogleServiceAuthError& error);

  // Returns true of there are pending log ins or outs.
  bool is_running() const { return requests_.size() > 0; }

  // Access the internal object during tests.
  ExternalCcResultFetcher* external_cc_result_fetcher_for_testing() {
    return &external_cc_result_fetcher_;
  }

  void set_list_accounts_stale_for_testing(bool stale) {
    list_accounts_stale_ = stale;
  }

  // Returns a non-NULL pointer to its instance of net::BackoffEntry
  const net::BackoffEntry* GetBackoffEntry() {
    return &fetcher_backoff_;
  }

  // Can be overridden by tests.
  virtual scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

 private:
  FRIEND_TEST_ALL_PREFIXES(GaiaCookieManagerServiceTest,
                           MultiloginSuccessAndCookiesSet);
  FRIEND_TEST_ALL_PREFIXES(GaiaCookieManagerServiceTest,
                           MultiloginFailurePersistentError);
  FRIEND_TEST_ALL_PREFIXES(GaiaCookieManagerServiceTest,
                           MultiloginFailureMaxRetriesReached);
  FRIEND_TEST_ALL_PREFIXES(GaiaCookieManagerServiceTest,
                           FetcherRetriesZeroedBetweenCalls);
  FRIEND_TEST_ALL_PREFIXES(GaiaCookieManagerServiceTest,
                           MultiloginFailureInvalidGaiaCredentialsMobile);
  FRIEND_TEST_ALL_PREFIXES(GaiaCookieManagerServiceTest,
                           MultiloginFailureInvalidGaiaCredentialsDesktop);
  // Returns the source value to use for GaiaFetcher requests.  This is
  // virtual to allow tests and fake classes to override.
  virtual std::string GetSourceForRequest(
      const GaiaCookieManagerService::GaiaCookieRequest& request);

  // Returns the default source value to use for GaiaFetcher requests.  This is
  // virtual to allow tests and fake classes to override.
  virtual std::string GetDefaultSourceForRequest();

  // Overridden from network::mojom::CookieChangeListner. If the cookie relates
  // to a GAIA APISID cookie, then we call ListAccounts and fire
  // OnGaiaAccountsInCookieUpdated.
  void OnCookieChange(const net::CanonicalCookie& cookie,
                      network::mojom::CookieChangeCause cause) override;
  void OnCookieListenerConnectionError();

  // Overridden from UbertokenConsumer.
  void OnUbertokenSuccess(const std::string& token) override;
  void OnUbertokenFailure(const GoogleServiceAuthError& error) override;

  // Overridden from OAuth2TokenService::Consumer.
  void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;
  // Called when either refresh or access token becomes available.
  void OnTokenFetched(const std::string& account_id, const std::string& token);

  // Overridden from GaiaAuthConsumer.
  void OnMergeSessionSuccess(const std::string& data) override;
  void OnMergeSessionFailure(const GoogleServiceAuthError& error) override;
  void OnOAuthMultiloginFinished(const OAuthMultiloginResult& result) override;
  void OnListAccountsSuccess(const std::string& data) override;
  void OnListAccountsFailure(const GoogleServiceAuthError& error) override;
  void OnLogOutSuccess() override;
  void OnLogOutFailure(const GoogleServiceAuthError& error) override;

  // Callback for CookieManager::SetCanonicalCookie.
  void OnCookieSet(const std::string& cookie_name,
                   const std::string& cookie_domain,
                   bool success);

  // Final call in the Setting accounts in cookie procedure. Virtual for testing
  // purposes.
  virtual void OnSetAccountsFinished(const GoogleServiceAuthError& error);

  // Helper method for AddAccountToCookie* methods.
  void AddAccountToCookieInternal(const std::string& account_id,
                                  const std::string& source);

  // Helper function to trigger fetching retry in case of failure for only
  // failed account id. Virtual for testing purposes.
  virtual void StartFetchingAccessTokenForMultilogin(
      const std::string& account_id);

  // Starts the process of fetching the access token with OauthLogin scope and
  // performing SetAccountsInCookie on success.  Virtual so that it can be
  // overridden in tests.
  virtual void StartFetchingAccessTokensForMultilogin();

  // Starts the proess of fetching the uber token and performing a merge session
  // for the next account.  Virtual so that it can be overriden in tests.
  virtual void StartFetchingUbertoken();

  // Starts the process of setting accounts in cookie.
  void StartFetchingMultiLogin(
      const std::vector<GaiaAuthFetcher::MultiloginTokenIDPair>& accounts);

  // Virtual for testing purposes.
  virtual void StartFetchingMergeSession();

  // Virtual for testing purposes.
  virtual void StartFetchingListAccounts();

  // Prepare for logout and then starts fetching logout request.
  void StartGaiaLogOut();

  // Starts fetching log out.
  // Virtual for testing purpose.
  virtual void StartFetchingLogOut();

  // Starts setting parsed cookies in browser.
  void StartSettingCookies(const OAuthMultiloginResult& result);

  // Start the next request, if needed.
  void HandleNextRequest();

  OAuth2TokenService* token_service_;
  SigninClient* signin_client_;
  std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
  std::unique_ptr<UbertokenFetcher> uber_token_fetcher_;
  ExternalCcResultFetcher external_cc_result_fetcher_;

  // If the GaiaAuthFetcher or SimpleURLLoader fails, retry with exponential
  // backoff and network delay.
  net::BackoffEntry fetcher_backoff_;
  base::OneShotTimer fetcher_timer_;
  int fetcher_retries_;

  // The last fetched ubertoken, for use in MergeSession retries.
  std::string uber_token_;

  // Access tokens for use inside SetAccountsToCookie.
  // TODO (valeriyas): make FetchUberToken use those instead of a separate
  // access_token.
  std::unordered_map<std::string, std::string> access_tokens_;

  // Current list of processed token requests;
  std::vector<std::unique_ptr<OAuth2TokenService::Request>> token_requests_;

  // The access token that can be used to prime the UberToken fetch.
  std::string access_token_;

  // List of pairs (cookie name and cookie domain) that have to be set in
  // cookie jar.
  std::set<std::pair<std::string, std::string>> cookies_to_set_;

  // Connection to the CookieManager that signals when the GAIA cookies change.
  mojo::Binding<network::mojom::CookieChangeListener> cookie_listener_binding_;

  // A worklist for this class. Stores any pending requests that couldn't be
  // executed right away, since this class only permits one request to be
  // executed at a time.
  base::circular_deque<GaiaCookieRequest> requests_;

  // List of observers to notify when merge session completes.
  // Makes sure list is empty on destruction.
  base::ObserverList<Observer, true>::Unchecked observer_list_;

  // Source to use with GAIA endpoints for accounting.
  std::string source_;

  // True once the ExternalCCResultFetcher has completed once.
  bool external_cc_result_fetched_;

  std::vector<gaia::ListedAccount> listed_accounts_;
  std::vector<gaia::ListedAccount> signed_out_accounts_;

  bool list_accounts_stale_;

  base::WeakPtrFactory<GaiaCookieManagerService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GaiaCookieManagerService);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_GAIA_COOKIE_MANAGER_SERVICE_H_
