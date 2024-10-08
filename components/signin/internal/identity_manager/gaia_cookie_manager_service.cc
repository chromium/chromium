// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"

#include <stddef.h>

#include <optional>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/prefs/pref_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/oauth_multilogin_helper.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_constants.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/url_util.h"

namespace {

// The maximum number of retries for a fetcher used in this class.
constexpr int kMaxFetcherRetries = 8;

// Timeout for the ExternalCCResult fetch. See https://crbug.com/750316#c37 for
// details on the exact duration.
constexpr int kExternalCCResultTimeoutSeconds = 7;

// In case of an error while fetching using the GaiaAuthFetcher or
// SimpleURLLoader, retry with exponential backoff. Try up to 7 times within 15
// minutes.
const net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential backoff in ms.
    1000,

    // Factor by which the waiting time will be multiplied.
    3,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    1000 * 60 * 15,  // 15 minutes.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

// State of requests to Gaia logout endpoint. Used as entry for histogram
// |Signin.GaiaCookieManager.Logout|.
enum LogoutRequestState {
  kStarted = 0,
  kSuccess = 1,
  kFailed = 2,
  kMaxValue = kFailed
};

BASE_FEATURE(kGaiaCookieManagerServiceMonitorsAllDeletions,
             "GaiaCookieManagerServiceMonitorsAllDeletions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Records metrics for ListAccounts failures.
void RecordListAccountsFailure(GoogleServiceAuthError::State error_state) {
  UMA_HISTOGRAM_ENUMERATION("Signin.ListAccountsFailure", error_state,
                            GoogleServiceAuthError::NUM_STATES);
}

void RecordLogoutRequestState(LogoutRequestState logout_state) {
  UMA_HISTOGRAM_ENUMERATION("Signin.GaiaCookieManager.Logout", logout_state);
}

}  // namespace

GaiaCookieManagerService::GaiaCookieRequest::SetAccountsParams::
    SetAccountsParams() = default;
GaiaCookieManagerService::GaiaCookieRequest::SetAccountsParams::
    SetAccountsParams(const SetAccountsParams& other) = default;
GaiaCookieManagerService::GaiaCookieRequest::SetAccountsParams::
    ~SetAccountsParams() = default;

GaiaCookieManagerService::GaiaCookieRequest::GaiaCookieRequest(
    GaiaCookieRequestType request_type,
    gaia::GaiaSource source)
    : request_type_(request_type), source_(source) {}

GaiaCookieManagerService::GaiaCookieRequest::~GaiaCookieRequest() {}

GaiaCookieManagerService::GaiaCookieRequest::GaiaCookieRequest(
    GaiaCookieRequest&&) = default;

GaiaCookieManagerService::GaiaCookieRequest&
GaiaCookieManagerService::GaiaCookieRequest::operator=(GaiaCookieRequest&&) =
    default;

const std::vector<GaiaCookieManagerService::AccountIdGaiaIdPair>&
GaiaCookieManagerService::GaiaCookieRequest::GetAccounts() const {
  DCHECK_EQ(request_type_, GaiaCookieRequestType::SET_ACCOUNTS);
  return set_accounts_params_.accounts;
}

gaia::MultiloginMode
GaiaCookieManagerService::GaiaCookieRequest::GetMultiloginMode() const {
  DCHECK_EQ(request_type_, GaiaCookieRequestType::SET_ACCOUNTS);
  return set_accounts_params_.mode;
}

void GaiaCookieManagerService::GaiaCookieRequest::SetSourceSuffix(
    std::string suffix) {
  source_.SetGaiaSourceSuffix(suffix);
}

void GaiaCookieManagerService::GaiaCookieRequest::
    RunSetAccountsInCookieCompletedCallback(
        signin::SetAccountsInCookieResult result) {
  if (set_accounts_in_cookie_completed_callback_) {
    std::move(set_accounts_in_cookie_completed_callback_).Run(result);
  }
}

void GaiaCookieManagerService::GaiaCookieRequest::
    RunLogOutFromCookieCompletedCallback(const GoogleServiceAuthError& error) {
  if (log_out_from_cookie_completed_callback_) {
    std::move(log_out_from_cookie_completed_callback_).Run(error);
  }
}

// static
GaiaCookieManagerService::GaiaCookieRequest
GaiaCookieManagerService::GaiaCookieRequest::CreateSetAccountsRequest(
    gaia::MultiloginMode mode,
    const std::vector<AccountIdGaiaIdPair>& accounts,
    gaia::GaiaSource source,
    SetAccountsInCookieCompletedCallback callback) {
  GaiaCookieManagerService::GaiaCookieRequest request(
      GaiaCookieRequestType::SET_ACCOUNTS, source);
  request.set_accounts_params_.mode = mode;
  request.set_accounts_params_.accounts = accounts;
  request.set_accounts_in_cookie_completed_callback_ = std::move(callback);
  return request;
}

// static
GaiaCookieManagerService::GaiaCookieRequest
GaiaCookieManagerService::GaiaCookieRequest::CreateLogOutRequest(
    gaia::GaiaSource source,
    LogOutFromCookieCompletedCallback callback) {
  GaiaCookieManagerService::GaiaCookieRequest request(
      GaiaCookieRequestType::LOG_OUT, source);
  request.log_out_from_cookie_completed_callback_ = std::move(callback);
  return request;
}

// static
GaiaCookieManagerService::GaiaCookieRequest
GaiaCookieManagerService::GaiaCookieRequest::CreateListAccountsRequest() {
  return GaiaCookieManagerService::GaiaCookieRequest(
      GaiaCookieRequestType::LIST_ACCOUNTS, gaia::GaiaSource::kChrome);
}

GaiaCookieManagerService::ExternalCcResultFetcher::ExternalCcResultFetcher(
    GaiaCookieManagerService* helper)
    : helper_(helper) {
  DCHECK(helper_);
}

GaiaCookieManagerService::ExternalCcResultFetcher::~ExternalCcResultFetcher() {
  CleanupTransientState();
}

std::string
GaiaCookieManagerService::ExternalCcResultFetcher::GetExternalCcResult() {
  std::vector<std::string> results;
  for (ResultMap::const_iterator it = results_.begin(); it != results_.end();
       ++it) {
    results.push_back(it->first + ":" + it->second);
  }
  return base::JoinString(results, ",");
}

void GaiaCookieManagerService::ExternalCcResultFetcher::Start(
    base::OnceClosure callback) {
  DCHECK(!helper_->external_cc_result_fetched_);
  m_external_cc_result_start_time_ = base::Time::Now();
  callback_ = std::move(callback);

  CleanupTransientState();
  results_.clear();
  helper_->gaia_auth_fetcher_ = helper_->signin_client_->CreateGaiaAuthFetcher(
      this, gaia::GaiaSource::kChrome);
  helper_->gaia_auth_fetcher_->StartGetCheckConnectionInfo();

  // Some fetches may timeout.  Start a timer to decide when the result fetcher
  // has waited long enough.
  timer_.Start(FROM_HERE, base::Seconds(kExternalCCResultTimeoutSeconds), this,
               &GaiaCookieManagerService::ExternalCcResultFetcher::Timeout);
}

bool GaiaCookieManagerService::ExternalCcResultFetcher::IsRunning() {
  return helper_->gaia_auth_fetcher_ || loaders_.size() > 0u ||
         timer_.IsRunning();
}

void GaiaCookieManagerService::ExternalCcResultFetcher::TimeoutForTests() {
  Timeout();
}

void GaiaCookieManagerService::ExternalCcResultFetcher::
    OnGetCheckConnectionInfoSuccess(const std::string& data) {
  std::optional<base::Value> value = base::JSONReader::Read(data);
  if (!value || !value->is_list()) {
    CleanupTransientState();
    GetCheckConnectionInfoCompleted(false);
    return;
  }

  // If there is nothing to check, terminate immediately.
  if (value->GetList().size() == 0) {
    CleanupTransientState();
    GetCheckConnectionInfoCompleted(true);
    return;
  }

  // Start a fetcher for each connection URL that needs to be checked.
  for (const base::Value& elem : value->GetList()) {
    if (!elem.is_dict()) {
      continue;
    }

    const base::Value::Dict& elem_dict = elem.GetDict();
    const std::string* token = elem_dict.FindString("carryBackToken");
    const std::string* url = elem_dict.FindString("url");
    if (token && url) {
      results_[*token] = "null";
      network::SimpleURLLoader* loader =
          CreateAndStartLoader(GURL(*url)).release();
      loaders_[loader] = *token;
    }
  }
}

void GaiaCookieManagerService::ExternalCcResultFetcher::
    OnGetCheckConnectionInfoError(const GoogleServiceAuthError& error) {
  VLOG(1) << "GaiaCookieManagerService::ExternalCcResultFetcher::"
          << "OnGetCheckConnectionInfoError " << error.ToString();

  // Chrome does not have any retry logic for fetching ExternalCcResult. The
  // ExternalCcResult is only used to inform Gaia that Chrome has already
  // checked the connection to other sites.
  //
  // In case fetching the ExternalCcResult fails:
  // * The result of merging accounts to Gaia cookies will not be affected.
  // * Gaia will need make its own call about whether to check them itself,
  //   of make some other assumptions.
  CleanupTransientState();
  GetCheckConnectionInfoCompleted(false);
}

std::unique_ptr<network::SimpleURLLoader>
GaiaCookieManagerService::ExternalCcResultFetcher::CreateAndStartLoader(
    const GURL& url) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "gaia_cookie_manager_external_cc_result", R"(
          semantics {
            sender: "Gaia Cookie Manager"
            description:
              "This request is used by the GaiaCookieManager when adding an "
              "account to the Google authentication cookies to check the "
              "authentication server's connection state."
            trigger:
              "This is used at most once per lifetime of the application "
              "during the first merge session flow (the flow used to add an "
              "account for which Chrome has a valid OAuth2 refresh token to "
              "the Gaia authentication cookies). The value of the first fetch "
              "is stored in RAM for future uses."
            data: "None."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting: "This feature cannot be disabled in settings."
            policy_exception_justification:
              "Not implemented. Disabling GaiaCookieManager would break "
              "features that depend on it (like account consistency and "
              "support for child accounts). It makes sense to control top "
              "level features that use the GaiaCookieManager."
          })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);

  // Fetchers are sometimes cancelled because a network change was detected,
  // especially at startup and after sign-in on ChromeOS.
  loader->SetRetryOptions(1, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      helper_->GetURLLoaderFactory().get(),
      base::BindOnce(&ExternalCcResultFetcher::OnURLLoadComplete,
                     base::Unretained(this), loader.get()));

  return loader;
}

void GaiaCookieManagerService::ExternalCcResultFetcher::OnURLLoadComplete(
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> body) {
  if (source->NetError() != net::OK || !source->ResponseInfo() ||
      !source->ResponseInfo()->headers ||
      source->ResponseInfo()->headers->response_code() != net::HTTP_OK) {
    return;
  }

  auto it = loaders_.find(source);
  if (it == loaders_.end()) {
    return;
  }

  std::string data;
  if (body) {
    data = std::move(*body);
  }

  // Only up to the first 16 characters of the response are important to GAIA.
  // Truncate if needed to keep amount data sent back to GAIA down.
  constexpr int kTruncatedLength = 16;
  if (data.size() > kTruncatedLength) {
    data.resize(kTruncatedLength);
  }

  // Encode the response to avoid cases where a proxy could pass a
  // comma-separated string which would break the server-side parsing
  // (see crbug.com/1086916#c4).

  // A character may be encoded into a maximum of 4 characters.
  constexpr int kEncodedLength = kTruncatedLength * 4;
  url::RawCanonOutputT<char, kEncodedLength> encoded_data;
  url::EncodeURIComponent(data, &encoded_data);
  results_[it->second] = std::string(encoded_data.view());

  // Clean up tracking of this fetcher.  The rest will be cleaned up after
  // the timer expires in CleanupTransientState().
  DCHECK_EQ(source, it->first);
  loaders_.erase(it);
  delete source;

  // If all expected responses have been received, cancel the timer and
  // report the result.
  if (loaders_.empty()) {
    CleanupTransientState();
    GetCheckConnectionInfoCompleted(true);
  }
}

void GaiaCookieManagerService::ExternalCcResultFetcher::Timeout() {
  VLOG(1) << " GaiaCookieManagerService::ExternalCcResultFetcher::Timeout";
  CleanupTransientState();
  GetCheckConnectionInfoCompleted(false);
}

void GaiaCookieManagerService::ExternalCcResultFetcher::
    CleanupTransientState() {
  timer_.Stop();
  helper_->gaia_auth_fetcher_.reset();

  for (const auto& loader_token_pair : loaders_) {
    delete loader_token_pair.first;
  }
  loaders_.clear();
}

void GaiaCookieManagerService::ExternalCcResultFetcher::
    GetCheckConnectionInfoCompleted(bool succeeded) {
  base::TimeDelta time_to_check_connections =
      base::Time::Now() - m_external_cc_result_start_time_;
  signin_metrics::LogExternalCcResultFetches(succeeded,
                                             time_to_check_connections);

  helper_->external_cc_result_fetched_ = true;
  std::move(callback_).Run();
}

GaiaCookieManagerService::GaiaCookieManagerService(
    AccountTrackerService* account_tracker_service,
    ProfileOAuth2TokenService* token_service,
    SigninClient* signin_client)
    : account_tracker_service_(account_tracker_service),
      token_service_(token_service),
      signin_client_(signin_client),
      external_cc_result_fetcher_(this),
      fetcher_backoff_(&kBackoffPolicy),
      fetcher_retries_(0),
      listAccountsUnexpectedServerResponseRetried_(false),
      external_cc_result_fetched_(false),
      list_accounts_stale_(true) {
  std::string gaia_cookie_last_list_accounts_data =
      signin_client_->GetPrefs()->GetString(
          prefs::kGaiaCookieLastListAccountsData);

  if (!gaia_cookie_last_list_accounts_data.empty()) {
    if (!gaia::ParseListAccountsData(gaia_cookie_last_list_accounts_data,
                                     &accounts_)) {
      DLOG(WARNING) << "GaiaCookieManagerService::ListAccounts: Failed to "
                       "parse list accounts data from pref.";
      accounts_.clear();
      return;
    }
    InitializeListedAccountsIds();
  }
}

GaiaCookieManagerService::~GaiaCookieManagerService() {
  cookie_listener_receiver_.reset();
  CancelAll();
  DCHECK(requests_.empty());
}

// static
void GaiaCookieManagerService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGaiaCookieLastListAccountsData,
                               std::string());
}

void GaiaCookieManagerService::InitCookieListener() {
  DCHECK(!cookie_listener_receiver_.is_bound());

  network::mojom::CookieManager* cookie_manager =
      signin_client_->GetCookieManager();

  // NOTE: |cookie_manager| can be nullptr when TestSigninClient is used in
  // testing contexts.
  if (cookie_manager) {
    std::optional<std::string> cookie_name;
    if (!base::FeatureList::IsEnabled(
            kGaiaCookieManagerServiceMonitorsAllDeletions)) {
      cookie_name = GaiaConstants::kGaiaSigninCookieName;
    }
    cookie_manager->AddCookieChangeListener(
        GaiaUrls::GetInstance()->secure_google_url(),
        /*name=*/cookie_name,
        cookie_listener_receiver_.BindNewPipeAndPassRemote());
    cookie_listener_receiver_.set_disconnect_handler(base::BindOnce(
        &GaiaCookieManagerService::OnCookieListenerConnectionError,
        base::Unretained(this)));
  }
}

void GaiaCookieManagerService::SetAccountsInCookie(
    gaia::MultiloginMode mode,
    const std::vector<AccountIdGaiaIdPair>& accounts,
    gaia::GaiaSource source,
    SetAccountsInCookieCompletedCallback
        set_accounts_in_cookies_completed_callback) {
  std::vector<std::string> account_ids;
  for (const auto& id : accounts) {
    account_ids.push_back(id.first.ToString());
  }
  VLOG(1) << "GaiaCookieManagerService::SetAccountsInCookie: "
          << base::JoinString(account_ids, " ");
  requests_.push_back(GaiaCookieRequest::CreateSetAccountsRequest(
      mode, accounts, source,
      std::move(set_accounts_in_cookies_completed_callback)));
  if (!signin_client_->AreSigninCookiesAllowed()) {
    OnSetAccountsFinished(signin::SetAccountsInCookieResult::kPersistentError);
    return;
  }
  if (requests_.size() == 1) {
    fetcher_retries_ = 0;
    StartSetAccounts();
  }
}

signin::AccountsInCookieJarInfo GaiaCookieManagerService::ListAccounts() {
  if (list_accounts_stale_) {
    // `ListAccounts()` doesn't mean a change has happened that requires adding
    // a new /ListAccounts request even if there is one in-flight.
    // Only trigger a request, if none is ongoing.
    if (!base::Contains(requests_, LIST_ACCOUNTS,
                        &GaiaCookieRequest::request_type)) {
      TriggerListAccounts();
    }
  }

  return signin::AccountsInCookieJarInfo(
      /*accounts_are_fresh=*/!list_accounts_stale_,
      /*accounts=*/accounts_);
}

void GaiaCookieManagerService::TriggerListAccounts() {
  // Callers suspect that a check to Gaia needs to be done, don't rely on the
  // in progress request, conditions might have changed while the request is
  // in-flight.
  // Note: /ListAccounts requests is optimized in `OptimizeListAccounts()`
  // called in `HandleNextRequest()`. Only if there are no other requests in the
  // queue that `HandleNextRequest()` won't be called before executing
  // /ListAccounts.
  requests_.push_back(GaiaCookieRequest::CreateListAccountsRequest());
  if (requests_.size() == 1) {
    fetcher_retries_ = 0;
    listAccountsUnexpectedServerResponseRetried_ = false;
    signin_client_->DelayNetworkCall(
        base::BindOnce(&GaiaCookieManagerService::StartFetchingListAccounts,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void GaiaCookieManagerService::ForceOnCookieChangeProcessing() {
  GURL google_url = GaiaUrls::GetInstance()->secure_google_url();
  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::CreateSanitizedCookie(
          google_url, GaiaConstants::kGaiaSigninCookieName, std::string(),
          "." + google_url.host(), "/", base::Time(), base::Time(),
          base::Time(), true /* secure */, false /* httponly */,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT,
          std::nullopt /* cookie_partition_key */, /*status=*/nullptr);
  OnCookieChange(
      net::CookieChangeInfo(*cookie, net::CookieAccessResult(),
                            net::CookieChangeCause::UNKNOWN_DELETION));
}

void GaiaCookieManagerService::LogOutAllAccounts(
    gaia::GaiaSource source,
    LogOutFromCookieCompletedCallback completion_callback) {
  VLOG(1) << "GaiaCookieManagerService::LogOutAllAccounts";
  DCHECK(completion_callback);

  // Verify a LOG_OUT isn't already queued.
  if (base::Contains(requests_, GaiaCookieRequestType::LOG_OUT,
                     &GaiaCookieRequest::request_type)) {
    std::move(completion_callback)
        .Run(GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
    return;
  }

  requests_.push_back(GaiaCookieRequest::CreateLogOutRequest(
      source, std::move(completion_callback)));
  if (requests_.size() == 1) {
    fetcher_retries_ = 0;
    signin_client_->DelayNetworkCall(
        base::BindOnce(&GaiaCookieManagerService::StartGaiaLogOut,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void GaiaCookieManagerService::RemoveLoggedOutAccountByGaiaId(
    const std::string& gaia_id) {
  VLOG(1) << "GaiaCookieManagerService::RemoveLoggedOutAccountByGaiaId";

  if (list_accounts_stale_) {
    return;
  }

  const bool accounts_updated =
      std::erase_if(accounts_, [&gaia_id](const gaia::ListedAccount& account) {
        return account.gaia_id == gaia_id && account.signed_out == true;
      }) != 0;

  if (!accounts_updated) {
    return;
  }

  if (gaia_accounts_updated_in_cookie_callback_) {
    gaia_accounts_updated_in_cookie_callback_.Run(
        signin::AccountsInCookieJarInfo(
            /*accounts_are_fresh=*/!list_accounts_stale_,
            /*accounts=*/accounts_),
        GoogleServiceAuthError::AuthErrorNone());
  }
}

void GaiaCookieManagerService::CancelAll() {
  VLOG(1) << "GaiaCookieManagerService::CancelAll";
  gaia_auth_fetcher_.reset();
  oauth_multilogin_helper_.reset();
  requests_.clear();
  fetcher_timer_.Stop();
}

scoped_refptr<network::SharedURLLoaderFactory>
GaiaCookieManagerService::GetURLLoaderFactory() {
  return signin_client_->GetURLLoaderFactory();
}

void GaiaCookieManagerService::MarkListAccountsStale() {
  list_accounts_stale_ = true;
#if BUILDFLAG(IS_IOS)
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&GaiaCookieManagerService::ForceOnCookieChangeProcessing,
                     weak_ptr_factory_.GetWeakPtr()));
#endif  // BUILDFLAG(IS_IOS)
}

void GaiaCookieManagerService::OnCookieChange(
    const net::CookieChangeInfo& change) {
  DCHECK(change.cookie.IsDomainMatch(
      GaiaUrls::GetInstance()->google_url().host()));

  // This function is called for all changes in google.com cookies. It monitors
  // deletions for all cookies, and  non-deletion changes for
  // `kGaiaSigninCookieName`.
  if (GaiaConstants::kGaiaSigninCookieName != change.cookie.Name() &&
      change.cause != net::CookieChangeCause::EXPLICIT) {
    return;
  }

  list_accounts_stale_ = true;

  // Call `gaia_cookie_deleted_by_user_action_callback_` only for
  // `kGaiaSigninCookieName`. Other deletions still trigger a /ListAccounts.
  if (change.cause == net::CookieChangeCause::EXPLICIT &&
      GaiaConstants::kGaiaSigninCookieName == change.cookie.Name()) {
    if (gaia_cookie_deleted_by_user_action_callback_) {
      gaia_cookie_deleted_by_user_action_callback_.Run();
    }
  }
  // Cookie changed, force a check to Gaia.
  TriggerListAccounts();
}

void GaiaCookieManagerService::OnCookieListenerConnectionError() {
  // A connection error from the CookieManager likely means that the network
  // service process has crashed. Try again to set up a listener.
  cookie_listener_receiver_.reset();
  InitCookieListener();
}

void GaiaCookieManagerService::SignalSetAccountsComplete(
    signin::SetAccountsInCookieResult result) {
  DCHECK(requests_.front().request_type() ==
         GaiaCookieRequestType::SET_ACCOUNTS);
  requests_.front().RunSetAccountsInCookieCompletedCallback(result);
}

void GaiaCookieManagerService::SignalLogOutComplete(
    const GoogleServiceAuthError& error) {
  DCHECK(requests_.front().request_type() == GaiaCookieRequestType::LOG_OUT);
  requests_.front().RunLogOutFromCookieCompletedCallback(error);
}

void GaiaCookieManagerService::SetGaiaAccountsInCookieUpdatedCallback(
    GaiaAccountsInCookieUpdatedCallback callback) {
  DCHECK(!gaia_accounts_updated_in_cookie_callback_);
  gaia_accounts_updated_in_cookie_callback_ = std::move(callback);
}

void GaiaCookieManagerService::SetGaiaCookieDeletedByUserActionCallback(
    GaiaCookieDeletedByUserActionCallback callback) {
  DCHECK(!gaia_cookie_deleted_by_user_action_callback_);
  gaia_cookie_deleted_by_user_action_callback_ = std::move(callback);
}

void GaiaCookieManagerService::OnListAccountsSuccess(const std::string& data) {
  VLOG(1) << "ListAccounts successful";
  DCHECK(requests_.front().request_type() ==
         GaiaCookieRequestType::LIST_ACCOUNTS);
  fetcher_backoff_.InformOfRequest(true);

  if (!gaia::ParseListAccountsData(data, &accounts_)) {
    accounts_.clear();
    signin_client_->GetPrefs()->ClearPref(
        prefs::kGaiaCookieLastListAccountsData);
    GoogleServiceAuthError error =
        GoogleServiceAuthError::FromUnexpectedServiceResponse(
            "Error parsing ListAccounts response");
    OnListAccountsFailure(error);
    return;
  }

  signin_client_->GetPrefs()->SetString(prefs::kGaiaCookieLastListAccountsData,
                                        data);
  RecordListAccountsFailure(GoogleServiceAuthError::NONE);

  InitializeListedAccountsIds();

  list_accounts_stale_ = false;
  HandleNextRequest();
  // HandleNextRequest before sending out the notification because some
  // services, in response to OnGaiaAccountsInCookieUpdated, may try in return
  // to call ListAccounts, which would immediately return false if the
  // ListAccounts request is still sitting in queue.
  if (gaia_accounts_updated_in_cookie_callback_) {
    gaia_accounts_updated_in_cookie_callback_.Run(
        ListAccounts(), GoogleServiceAuthError::AuthErrorNone());
  }
}

void GaiaCookieManagerService::OnListAccountsFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "ListAccounts failed";
  DCHECK(requests_.front().request_type() ==
         GaiaCookieRequestType::LIST_ACCOUNTS);

  bool should_retry =
      (++fetcher_retries_ < kMaxFetcherRetries && error.IsTransientError()) ||
      (!listAccountsUnexpectedServerResponseRetried_ &&
       error.state() == GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE);
  if (should_retry) {
    if (error.state() == GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE) {
      listAccountsUnexpectedServerResponseRetried_ = true;
      requests_.front().SetSourceSuffix(
          GaiaConstants::kUnexpectedServiceResponse);
    }
    fetcher_backoff_.InformOfRequest(false);
    UMA_HISTOGRAM_ENUMERATION("Signin.ListAccountsRetry", error.state(),
                              GoogleServiceAuthError::NUM_STATES);
    fetcher_timer_.Start(
        FROM_HERE, fetcher_backoff_.GetTimeUntilRelease(),
        base::BindOnce(
            &SigninClient::DelayNetworkCall, base::Unretained(signin_client_),
            base::BindOnce(&GaiaCookieManagerService::StartFetchingListAccounts,
                           weak_ptr_factory_.GetWeakPtr())));
    return;
  }

  RecordListAccountsFailure(error.state());

  if (gaia_accounts_updated_in_cookie_callback_) {
    gaia_accounts_updated_in_cookie_callback_.Run(ListAccounts(), error);
  }

  HandleNextRequest();
}

void GaiaCookieManagerService::OnLogOutSuccess() {
  DCHECK(requests_.front().request_type() == GaiaCookieRequestType::LOG_OUT);
  VLOG(1) << "GaiaCookieManagerService::OnLogOutSuccess";
  RecordLogoutRequestState(LogoutRequestState::kSuccess);

  MarkListAccountsStale();
  SignalLogOutComplete(GoogleServiceAuthError::AuthErrorNone());
  fetcher_backoff_.InformOfRequest(true);
  HandleNextRequest();
}

void GaiaCookieManagerService::OnLogOutFailure(
    const GoogleServiceAuthError& error) {
  DCHECK(requests_.front().request_type() == GaiaCookieRequestType::LOG_OUT);
  VLOG(1) << "GaiaCookieManagerService::OnLogOutFailure";
  RecordLogoutRequestState(LogoutRequestState::kFailed);

  if (++fetcher_retries_ < kMaxFetcherRetries) {
    fetcher_backoff_.InformOfRequest(false);
    fetcher_timer_.Start(
        FROM_HERE, fetcher_backoff_.GetTimeUntilRelease(),
        base::BindOnce(
            &SigninClient::DelayNetworkCall, base::Unretained(signin_client_),
            base::BindOnce(&GaiaCookieManagerService::StartGaiaLogOut,
                           weak_ptr_factory_.GetWeakPtr())));
    return;
  }

  SignalLogOutComplete(error);
  HandleNextRequest();
}

std::unique_ptr<GaiaAuthFetcher>
GaiaCookieManagerService::CreateGaiaAuthFetcherForPartition(
    GaiaAuthConsumer* consumer,
    const gaia::GaiaSource& source) {
  return signin_client_->CreateGaiaAuthFetcher(consumer, source);
}

network::mojom::CookieManager*
GaiaCookieManagerService::GetCookieManagerForPartition() {
  return signin_client_->GetCookieManager();
}

void GaiaCookieManagerService::InitializeListedAccountsIds() {
  for (gaia::ListedAccount& account : accounts_) {
    DCHECK(account.id.empty());
    account.id = account_tracker_service_->PickAccountIdForAccount(
        account.gaia_id, account.email);
  }
}

void GaiaCookieManagerService::StartGaiaLogOut() {
  DCHECK(requests_.front().request_type() == GaiaCookieRequestType::LOG_OUT);
  VLOG(1) << "GaiaCookieManagerService::StartGaiaLogOut";
  RecordLogoutRequestState(LogoutRequestState::kStarted);
  gaia_auth_fetcher_ =
      signin_client_->CreateGaiaAuthFetcher(this, requests_.front().source());
  gaia_auth_fetcher_->StartLogOut();
}

void GaiaCookieManagerService::StartFetchingListAccounts() {
  VLOG(1) << "GaiaCookieManagerService::ListAccounts";
  gaia_auth_fetcher_ =
      signin_client_->CreateGaiaAuthFetcher(this, requests_.front().source());
  gaia_auth_fetcher_->StartListAccounts();
}

void GaiaCookieManagerService::StartSetAccounts() {
  DCHECK(!requests_.empty());
  const GaiaCookieRequest& request = requests_.front();
  DCHECK_EQ(GaiaCookieRequestType::SET_ACCOUNTS, request.request_type());
  DCHECK(!request.GetAccounts().empty());

  if (!external_cc_result_fetched_ &&
      !external_cc_result_fetcher_.IsRunning()) {
    external_cc_result_fetcher_.Start(
        base::BindOnce(&GaiaCookieManagerService::StartSetAccounts,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  oauth_multilogin_helper_ = std::make_unique<signin::OAuthMultiloginHelper>(
      signin_client_, this, token_service_, request.GetMultiloginMode(),
      request.GetAccounts(), external_cc_result_fetcher_.GetExternalCcResult(),
      request.source(),
      base::BindOnce(&GaiaCookieManagerService::OnSetAccountsFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GaiaCookieManagerService::OnSetAccountsFinished(
    signin::SetAccountsInCookieResult result) {
  MarkListAccountsStale();
  SignalSetAccountsComplete(result);
  HandleNextRequest();
}

void GaiaCookieManagerService::HandleNextRequest() {
  VLOG(1) << "GaiaCookieManagerService::HandleNextRequest";
  // Pop the completed request.
  requests_.pop_front();
  OptimizeListAccounts();

  gaia_auth_fetcher_.reset();
  fetcher_retries_ = 0;
  if (!requests_.empty()) {
    switch (requests_.front().request_type()) {
      case GaiaCookieRequestType::SET_ACCOUNTS: {
        StartSetAccounts();
        break;
      }
      case GaiaCookieRequestType::LOG_OUT:
        signin_client_->DelayNetworkCall(
            base::BindOnce(&GaiaCookieManagerService::StartGaiaLogOut,
                           weak_ptr_factory_.GetWeakPtr()));
        break;
      case GaiaCookieRequestType::LIST_ACCOUNTS:
        listAccountsUnexpectedServerResponseRetried_ = false;
        signin_client_->DelayNetworkCall(
            base::BindOnce(&GaiaCookieManagerService::StartFetchingListAccounts,
                           weak_ptr_factory_.GetWeakPtr()));
        break;
    }
  }
}

void GaiaCookieManagerService::OptimizeListAccounts() {
  if (requests_.empty() || requests_.front().request_type() !=
                               GaiaCookieRequestType::LIST_ACCOUNTS) {
    return;
  }
  // Next request is /ListAccounts.
  // Remove duplicate list accounts requests.
  requests_.erase(std::remove_if(requests_.begin() + 1, requests_.end(),
                                 [](const GaiaCookieRequest& request) {
                                   return request.request_type() ==
                                          LIST_ACCOUNTS;
                                 }),
                  requests_.end());

  // Logout or set accounts will impact the result of list accounts.
  // Handle those requests first.
  bool should_delay_list_accounts =
      base::ranges::any_of(requests_, [](const auto& request) {
        return request.request_type() == GaiaCookieRequestType::LOG_OUT ||
               request.request_type() == GaiaCookieRequestType::SET_ACCOUNTS;
      });

  if (should_delay_list_accounts) {
    // Move list accounts request to the end of the queue.
    requests_.pop_front();
    requests_.push_back(GaiaCookieRequest::CreateListAccountsRequest());
  }
}
