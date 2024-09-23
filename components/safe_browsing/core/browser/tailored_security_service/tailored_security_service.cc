// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_notification_result.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_observer.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace safe_browsing {
namespace {

const int kRepeatingCheckTailoredSecurityBitDelayInMinutes = 5;

const char kQueryTailoredSecurityServiceUrl[] =
    "https://history.google.com/history/api/lookup?client=aesb";

// The maximum number of retries for the SimpleURLLoader requests.
const size_t kMaxRetries = 1;

// Returns a primary Google account that can be used for getting a token.
CoreAccountId GetAccountForRequest(
    const signin::IdentityManager* identity_manager) {
  CoreAccountInfo result =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  return result.IsEmpty() ? CoreAccountId() : result.account_id;
}

class RequestImpl : public TailoredSecurityService::Request {
 public:
  RequestImpl(signin::IdentityManager* identity_manager,
              scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
              const GURL& url,
              TailoredSecurityService::CompletionCallback callback,
              const net::NetworkTrafficAnnotationTag& traffic_annotation)
      : identity_manager_(identity_manager),
        url_loader_factory_(std::move(url_loader_factory)),
        url_(url),
        callback_(std::move(callback)),
        traffic_annotation_(traffic_annotation) {
    DCHECK(identity_manager_);
    DCHECK(url_loader_factory_);
  }
  ~RequestImpl() override = default;

  // Returns the response code received from the server, which will only be
  // valid if the request succeeded.
  int GetResponseCode() const override { return response_code_; }

  // Returns the contents of the response body received from the server.
  const std::string& GetResponseBody() const override { return response_body_; }

  void SetPostData(const std::string& post_data) override {
    post_data_ = post_data;
  }

  bool IsPending() const override { return is_pending_; }

 private:
  friend class safe_browsing::TailoredSecurityService;

  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info) {
    access_token_fetcher_.reset();

    if (error.state() != GoogleServiceAuthError::NONE) {
      is_pending_ = false;
      UMA_HISTOGRAM_BOOLEAN(
          "SafeBrowsing.TailoredSecurityService.OAuthTokenCompletion", false);
      UMA_HISTOGRAM_ENUMERATION(
          "SafeBrowsing.TailoredSecurityService.OAuthTokenErrorState",
          error.state(), GoogleServiceAuthError::NUM_STATES);
      std::move(callback_).Run(this, false);

      // It is valid for the callback to delete |this|, so do not access any
      // members below here.
      return;
    }

    DCHECK(!access_token_info.token.empty());
    access_token_ = access_token_info.token;

    UMA_HISTOGRAM_BOOLEAN(
        "SafeBrowsing.TailoredSecurityService.OAuthTokenCompletion", true);

    // Got an access token -- start the actual API request.
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url_;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    resource_request->method = post_data_ ? "POST" : "GET";
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                        "Bearer " + access_token_info.token);
    resource_request->headers.SetHeader(
        "X-Developer-Key", GaiaUrls::GetInstance()->oauth2_chrome_client_id());

    simple_url_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation_);
    simple_url_loader_->SetRetryOptions(kMaxRetries,
                                        network::SimpleURLLoader::RETRY_ON_5XX);
    simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&RequestImpl::OnSimpleLoaderComplete,
                       base::Unretained(this)));
  }

  // Tells the request to get an access token and make the call to OnePlatform.
  void Start() override {
    access_token_fetcher_ =
        identity_manager_->CreateAccessTokenFetcherForAccount(
            GetAccountForRequest(identity_manager_),
            /*oauth_consumer_name=*/"tailored_security_service",
            {GaiaConstants::kChromeSafeBrowsingOAuth2Scope},
            base::BindOnce(&RequestImpl::OnAccessTokenFetchComplete,
                           base::Unretained(this)),
            signin::AccessTokenFetcher::Mode::kImmediate);
    is_pending_ = true;
  }

  void Shutdown() override {}

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body) {
    response_code_ = -1;
    if (simple_url_loader_->ResponseInfo() &&
        simple_url_loader_->ResponseInfo()->headers) {
      response_code_ =
          simple_url_loader_->ResponseInfo()->headers->response_code();
    }
    simple_url_loader_.reset();

    UMA_HISTOGRAM_CUSTOM_ENUMERATION(
        "SafeBrowsing.TailoredSecurityService.OAuthTokenResponseCode",
        net::HttpUtil::MapStatusCodeForHistogram(response_code_),
        net::HttpUtil::GetStatusCodesForHistogram());

    // If the response code indicates that the token might not be valid,
    // invalidate the token and try again.
    if (response_code_ == net::HTTP_UNAUTHORIZED && ++auth_retry_count_ <= 1) {
      signin::ScopeSet oauth_scopes;
      oauth_scopes.insert(GaiaConstants::kChromeSafeBrowsingOAuth2Scope);
      identity_manager_->RemoveAccessTokenFromCache(
          GetAccountForRequest(identity_manager_), oauth_scopes, access_token_);
      access_token_.clear();
      Start();
      return;
    }

    if (response_body) {
      response_body_ = std::move(*response_body);
    } else {
      response_body_.clear();
    }
    is_pending_ = false;
    std::move(callback_).Run(this, true);
    // It is valid for the callback to delete |this|, so do not access any
    // members below here.
  }

  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The URL of the API endpoint.
  GURL url_;
  // POST data to be sent with the request (may be empty).
  std::optional<std::string> post_data_;

  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;

  // The current OAuth2 access token.
  std::string access_token_;

  // Handles the actual API requests after the OAuth token is acquired.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Holds the response code received from the server.
  int response_code_ = 0;

  // Holds the response body received from the server.
  std::string response_body_ = "";

  // The number of times this request has already been retried due to
  // authorization problems.
  int auth_retry_count_ = 0;

  // The callback to execute when the query is complete.
  TailoredSecurityService::CompletionCallback callback_;

  // True if the request was started and has not yet completed, otherwise false.
  bool is_pending_ = false;

  // Network traffic annotation used to create SimpleURLLoader for this
  // request.
  const net::NetworkTrafficAnnotationTag traffic_annotation_;
};

}  // namespace

TailoredSecurityService::Request::Request() = default;

TailoredSecurityService::Request::~Request() = default;

TailoredSecurityService::TailoredSecurityService(
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    PrefService* prefs)
    : identity_manager_(identity_manager),
      sync_service_(sync_service),
      prefs_(prefs) {
  // `prefs` can be nullptr in unit tests.
  if (prefs_) {
    pref_registrar_.Init(prefs_);
    pref_registrar_.Add(
        prefs::kAccountTailoredSecurityUpdateTimestamp,
        base::BindRepeating(
            &TailoredSecurityService::TailoredSecurityTimestampUpdateCallback,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

TailoredSecurityService::~TailoredSecurityService() {
  for (auto& observer : observer_list_) {
    observer.OnTailoredSecurityServiceDestroyed();
  }
}

void TailoredSecurityService::AddObserver(
    TailoredSecurityServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TailoredSecurityService::RemoveObserver(
    TailoredSecurityServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

std::unique_ptr<TailoredSecurityService::Request>
TailoredSecurityService::CreateRequest(
    const GURL& url,
    CompletionCallback callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  return std::make_unique<RequestImpl>(identity_manager_, GetURLLoaderFactory(),
                                       url, std::move(callback),
                                       traffic_annotation);
}

size_t
TailoredSecurityService::GetNumberOfPendingTailoredSecurityServiceRequests() {
  return pending_tailored_security_requests_.size();
}

bool TailoredSecurityService::AddQueryRequest() {
  DCHECK(!is_shut_down_);
  if (!can_query_) {
    return false;
  }

  active_query_request_++;
  if (active_query_request_ == 1) {
    if (base::Time::Now() - last_updated_ <=
        base::Minutes(kRepeatingCheckTailoredSecurityBitDelayInMinutes)) {
      // Since we queried recently, start the timer with a shorter delay.
      base::TimeDelta delay =
          base::Minutes(kRepeatingCheckTailoredSecurityBitDelayInMinutes) -
          (base::Time::Now() - last_updated_);
      timer_.Start(FROM_HERE, delay, this,
                   &TailoredSecurityService::QueryTailoredSecurityBit);
    } else {
      // Query now and register a timer to get the tailored security bit
      // every `kRepeatingCheckTailoredSecurityBitDelayInMinutes` minutes.
      QueryTailoredSecurityBit();
      timer_.Start(
          FROM_HERE,
          base::Minutes(kRepeatingCheckTailoredSecurityBitDelayInMinutes), this,
          &TailoredSecurityService::QueryTailoredSecurityBit);
    }
  }
  return true;
}

void TailoredSecurityService::RemoveQueryRequest() {
  DCHECK(!is_shut_down_);
  DCHECK_GE(active_query_request_, 0UL);
  active_query_request_--;
  if (active_query_request_ == 0) {
    timer_.Stop();
  }
}

void TailoredSecurityService::QueryTailoredSecurityBit() {
  StartRequest(
      base::BindOnce(&TailoredSecurityService::OnTailoredSecurityBitRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TailoredSecurityService::StartRequest(
    QueryTailoredSecurityBitCallback callback) {
  DCHECK(!is_shut_down_);
  if (!can_query_) {
    saved_callback_ = std::move(callback);
    return;
  }

  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback =
      base::BindOnce(&TailoredSecurityService::
                         ExtractTailoredSecurityBitFromResponseAndRunCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  GURL url(kQueryTailoredSecurityServiceUrl);

  static constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("tailored_security_service",
                                          R"(
        semantics {
          description:
            "Queries history.google.com to find out if user has Account level"
            "Enhanced Safe Browsing enabled."
          trigger:
            "This request is sent every 5 minutes as long as the user does not"
            "have Enhanced Safe Browsing set for their Chrome profile."
          data:
            "An OAuth2 token authenticating the user."
          sender: "Safe Browsing"
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "To disable this feature, users uncheck the setting in Account "
            "Settings."
          chrome_policy {
            SafeBrowsingEnabled {
              policy_options {mode: MANDATORY}
              SafeBrowsingEnabled: false
            }
          }
        })");

  std::unique_ptr<Request> request =
      CreateRequest(url, std::move(completion_callback), traffic_annotation);
  Request* request_ptr = request.get();
  pending_tailored_security_requests_[request.get()] = std::move(request);
  if (pending_tailored_security_requests_[request_ptr]) {
    request_ptr->Start();
  }
}

void TailoredSecurityService::OnTailoredSecurityBitRetrieved(
    bool is_enabled,
    base::Time previous_update) {
  if (is_tailored_security_enabled_ != is_enabled) {
    for (auto& observer : observer_list_) {
      observer.OnTailoredSecurityBitChanged(is_enabled, previous_update);
    }
  }
  is_tailored_security_enabled_ = is_enabled;
  last_updated_ = base::Time::Now();
  if (active_query_request_ > 0) {
    timer_.Start(
        FROM_HERE,
        base::Minutes(kRepeatingCheckTailoredSecurityBitDelayInMinutes), this,
        &TailoredSecurityService::QueryTailoredSecurityBit);
  }
}

void TailoredSecurityService::MaybeNotifySyncUser(bool is_enabled,
                                                  base::Time previous_update) {
  if (!base::FeatureList::IsEnabled(kTailoredSecurityIntegration))
    return;

  if (!HistorySyncEnabledForUser()) {
    if (is_enabled) {
      RecordEnabledNotificationResult(
          TailoredSecurityNotificationResult::kHistoryNotSynced);
    }
    SaveRetryState(TailoredSecurityRetryState::NO_RETRY_NEEDED);
    return;
  }

  if (SafeBrowsingPolicyHandler::IsSafeBrowsingProtectionLevelSetByPolicy(
          prefs())) {
    if (is_enabled) {
      RecordEnabledNotificationResult(
          TailoredSecurityNotificationResult::kSafeBrowsingControlledByPolicy);
    }
    SaveRetryState(TailoredSecurityRetryState::NO_RETRY_NEEDED);
    return;
  }

  if (is_enabled && IsEnhancedProtectionEnabled(*prefs())) {
    RecordEnabledNotificationResult(
        TailoredSecurityNotificationResult::kEnhancedProtectionAlreadyEnabled);
    SaveRetryState(TailoredSecurityRetryState::NO_RETRY_NEEDED);
    return;
  }

  if (is_enabled && !IsEnhancedProtectionEnabled(*prefs())) {
    for (auto& observer : observer_list_) {
      observer.OnSyncNotificationMessageRequest(true);
    }
  }

  if (!is_enabled && IsEnhancedProtectionEnabled(*prefs()) &&
      prefs()->GetBoolean(
          prefs::kEnhancedProtectionEnabledViaTailoredSecurity)) {
    for (auto& observer : observer_list_) {
      observer.OnSyncNotificationMessageRequest(false);
    }
  }
}

bool TailoredSecurityService::HistorySyncEnabledForUser() {
  return sync_service_ &&
         sync_service_->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kHistory);
}

void TailoredSecurityService::
    ExtractTailoredSecurityBitFromResponseAndRunCallback(
        QueryTailoredSecurityBitCallback callback,
        Request* request,
        bool success) {
  DCHECK(!is_shut_down_);

  std::unique_ptr<Request> request_ptr =
      std::move(pending_tailored_security_requests_[request]);
  pending_tailored_security_requests_.erase(request);

  bool is_enabled = is_tailored_security_enabled_;
  base::Time previous_update = last_updated_;
  if (success) {
    base::Value::Dict response_value = ReadResponse(request);
    is_enabled =
        response_value.FindBool("history_recording_enabled").value_or(false);
  }

  std::move(callback).Run(is_enabled, previous_update);
}

void TailoredSecurityService::SetTailoredSecurityBitForTesting(
    bool is_enabled,
    QueryTailoredSecurityBitCallback callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback =
      base::BindOnce(&TailoredSecurityService::
                         ExtractTailoredSecurityBitFromResponseAndRunCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  GURL url(kQueryTailoredSecurityServiceUrl);
  std::unique_ptr<Request> request =
      CreateRequest(url, std::move(completion_callback), traffic_annotation);

  auto enable_tailored_security_service =
      base::Value::Dict().Set("history_recording_enabled", is_enabled);
  std::string post_data;
  base::JSONWriter::Write(enable_tailored_security_service, &post_data);
  request->SetPostData(post_data);

  request->Start();
  Request* request_ptr = request.get();
  pending_tailored_security_requests_[request_ptr] = std::move(request);
}

// static
base::Value::Dict TailoredSecurityService::ReadResponse(Request* request) {
  base::Value::Dict result;
  if (request->GetResponseCode() == net::HTTP_OK) {
    std::optional<base::Value> json_value =
        base::JSONReader::Read(request->GetResponseBody());
    if (json_value && json_value.value().is_dict())
      result = std::move(json_value->GetDict());
    else
      DLOG(WARNING) << "Non-JSON response received from server.";
  }
  return result;
}

void TailoredSecurityService::Shutdown() {
  // |pending_tailored_security_requests_| owns the pending Request,
  // clearing it will destroy all of them.
  pending_tailored_security_requests_.clear();
  timer_.Stop();
  is_shut_down_ = true;
  identity_manager_ = nullptr;
  sync_service_ = nullptr;
}

void TailoredSecurityService::TailoredSecurityTimestampUpdateCallback() {
  if (base::FeatureList::IsEnabled(
          safe_browsing::kTailoredSecurityRetryForSyncUsers)) {
    // TODO(crbug.com/40925236): remove sync flow last user interaction pref.
    prefs_->SetInteger(prefs::kTailoredSecuritySyncFlowLastUserInteractionState,
                       TailoredSecurityRetryState::UNKNOWN);
    prefs_->SetTime(prefs::kTailoredSecuritySyncFlowLastRunTime,
                    base::Time::Now());
    // If this method fails, then a retry is needed. If it succeeds, the
    // ChromeTailoredSecurityService will set this value to NO_RETRY_NEEDED for
    // us.
    prefs_->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                       TailoredSecurityRetryState::RETRY_NEEDED);
  }

  StartRequest(base::BindOnce(&TailoredSecurityService::MaybeNotifySyncUser,
                              weak_ptr_factory_.GetWeakPtr()));
}

void TailoredSecurityService::SaveRetryState(TailoredSecurityRetryState state) {
  if (base::FeatureList::IsEnabled(
          safe_browsing::kTailoredSecurityRetryForSyncUsers)) {
    prefs_->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState, state);
  }
}

void TailoredSecurityService::SetCanQuery(bool can_query) {
  can_query_ = can_query;
  if (can_query) {
    if (!saved_callback_.is_null()) {
      StartRequest(std::move(saved_callback_));
    }
  } else {
    timer_.Stop();
  }
}

}  // namespace safe_browsing
