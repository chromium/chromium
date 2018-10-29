// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/desktop_ios_promotion/sms_service.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

const char kDesktopIOSPromotionOAuthScope[] =
    "https://www.googleapis.com/auth/mobile_user_preferences";

const char kDesktopIOSPromotionQueryPhoneNumber[] =
    "https://growth-pa.googleapis.com/v1/get_verified_phone_numbers";

const char kDesktopIOSPromotionSendSMS[] =
    "https://growth-pa.googleapis.com/v1/send_sms";

const char kPostDataMimeType[] = "application/json";

const char kSendSMSPromoFormat[] = "{promo_id:%s}";

// The maximum number of retries for the URLFetcher requests.
const size_t kMaxRetries = 1;

class RequestImpl : public SMSService::Request {
 public:
  ~RequestImpl() override {}

  // Returns the response code received from the server, which will only be
  // valid if the request succeeded.
  int GetResponseCode() override { return response_code_; }

  // Returns the contents of the response body received from the server.
  const std::string& GetResponseBody() override { return response_body_; }

  bool IsPending() override { return is_pending_; }

 private:
  friend class ::SMSService;

  RequestImpl(identity::IdentityManager* identity_manager,
              scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
              const GURL& url,
              const SMSService::CompletionCallback& callback)
      : identity_manager_(identity_manager),
        url_loader_factory_(std::move(url_loader_factory)),
        url_(url),
        post_data_mime_type_(kPostDataMimeType),
        response_code_(0),
        auth_retry_count_(0),
        callback_(callback),
        is_pending_(false) {
    DCHECK(identity_manager_);
    DCHECK(url_loader_factory_);
  }

  void Start() override {
    OAuth2TokenService::ScopeSet oauth_scopes;
    oauth_scopes.insert(kDesktopIOSPromotionOAuthScope);
    token_fetcher_ =
        std::make_unique<identity::PrimaryAccountAccessTokenFetcher>(
            "desktop_ios_promotion", identity_manager_, oauth_scopes,
            base::BindOnce(&RequestImpl::AccessTokenFetchComplete,
                           base::Unretained(this)),
            identity::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
    is_pending_ = true;
  }

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body) {
    response_code_ = -1;
    if (simple_url_loader_->ResponseInfo() &&
        simple_url_loader_->ResponseInfo()->headers) {
      response_code_ =
          simple_url_loader_->ResponseInfo()->headers->response_code();
    }

    UMA_HISTOGRAM_CUSTOM_ENUMERATION(
        "DesktopIOSPromotion.OAuthTokenResponseCode",
        net::HttpUtil::MapStatusCodeForHistogram(response_code_),
        net::HttpUtil::GetStatusCodesForHistogram());

    // If the response code indicates that the token might not be valid,
    // invalidate the token and try again.
    if (response_code_ == net::HTTP_UNAUTHORIZED && ++auth_retry_count_ <= 1) {
      OAuth2TokenService::ScopeSet oauth_scopes;
      oauth_scopes.insert(kDesktopIOSPromotionOAuthScope);
      identity_manager_->RemoveAccessTokenFromCache(
          identity_manager_->GetPrimaryAccountId(), oauth_scopes,
          access_token_);
      access_token_.clear();
      Start();
      return;
    }
    bool success = !!response_body;
    if (success) {
      response_body_ = std::move(*response_body);
    } else {
      response_body_.clear();
    }
    simple_url_loader_.reset();
    is_pending_ = false;
    callback_.Run(this, success);
    // It is valid for the callback to delete |this|, so do not access any
    // members below here.
  }

  void AccessTokenFetchComplete(GoogleServiceAuthError error,
                                identity::AccessTokenInfo access_token_info) {
    token_fetcher_.reset();

    if (error.state() != GoogleServiceAuthError::NONE) {
      is_pending_ = false;
      UMA_HISTOGRAM_BOOLEAN("DesktopIOSPromotion.OAuthTokenCompletion", false);

      callback_.Run(this, false);
      // It is valid for the callback to delete |this|, so do not access any
      // members below here.
      return;
    }

    std::string access_token = access_token_info.token;
    DCHECK(!access_token.empty());
    access_token_ = access_token;

    UMA_HISTOGRAM_BOOLEAN("DesktopIOSPromotion.OAuthTokenCompletion", true);

    // Got an access token -- start the actual API request.
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("desktop_ios_promotion", R"(
        semantics {
          sender: "Desktop iOS Promotion"
          description:
            "Performes either of the following two tasks: Queries a logged in "
            "user's recovery phone number, or sends a predetermined "
            "promotional SMS to that number. SMS text may change but it always "
            "contains a link to download Chrome from iTunes."
          trigger:
            "The query without SMS is only triggered when the desktop to iOS "
            "promotion is shown to the user. The query and send is triggered "
            "when the user clicks on 'Send SMS' button in the desktop to iOS "
            "promotion."
          data:
            "It sends an oauth token (X-Developer-Key) which lets the Google "
            "API identify the user."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "The feature cannot be disabled by settings, but it can be "
            "disabled by 'Desktop to iOS Promotions' feature flag in "
            "about:flags."
          policy_exception_justification:
            "Not implemented, considered not useful as it does not upload any "
            "data and just downloads a recovery number."
        })");
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url_;
    resource_request->load_flags =
        net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;
    resource_request->method = post_data_ ? "POST" : "GET";
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                        "Bearer " + access_token);
    resource_request->headers.SetHeader(
        "X-Developer-Key", GaiaUrls::GetInstance()->oauth2_chrome_client_id());
    simple_url_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    simple_url_loader_->SetRetryOptions(kMaxRetries,
                                        network::SimpleURLLoader::RETRY_ON_5XX);
    if (post_data_)
      simple_url_loader_->AttachStringForUpload(post_data_.value(),
                                                post_data_mime_type_);
    simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&RequestImpl::OnSimpleLoaderComplete,
                       base::Unretained(this)));
  }

  void SetPostData(const std::string& post_data) override {
    SetPostDataAndType(post_data, kPostDataMimeType);
  }

  void SetPostDataAndType(const std::string& post_data,
                          const std::string& mime_type) override {
    post_data_ = post_data;
    post_data_mime_type_ = mime_type;
  }

  identity::IdentityManager* identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The URL of the API endpoint.
  GURL url_;

  // POST data to be sent with the request (may be empty).
  base::Optional<std::string> post_data_;

  // MIME type of the post requests. Defaults to text/plain.
  std::string post_data_mime_type_;

  // The OAuth2 access token request.
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher> token_fetcher_;

  // The current OAuth2 access token.
  std::string access_token_;

  // Handles the actual API requests after the OAuth token is acquired.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Holds the response code received from the server.
  int response_code_;

  // Holds the response body received from the server.
  std::string response_body_;

  // The number of times this request has already been retried due to
  // authorization problems.
  int auth_retry_count_;

  // The callback to execute when the query is complete.
  SMSService::CompletionCallback callback_;

  // True if the request was started and has not yet completed, otherwise false.
  bool is_pending_;
};

}  // namespace

SMSService::Request::Request() {}

SMSService::Request::~Request() {}

SMSService::SMSService(
    identity::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      weak_ptr_factory_(this) {}

SMSService::~SMSService() {}

SMSService::Request* SMSService::CreateRequest(
    const GURL& url,
    const CompletionCallback& callback) {
  return new RequestImpl(identity_manager_, url_loader_factory_, url, callback);
}

void SMSService::QueryPhoneNumber(const PhoneNumberCallback& callback) {
  CompletionCallback completion_callback =
      base::Bind(&SMSService::QueryPhoneNumberCompletionCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback);

  GURL url(kDesktopIOSPromotionQueryPhoneNumber);
  Request* request = CreateRequest(url, completion_callback);
  // This placeholder is required by the API.
  request->SetPostData("{}");
  pending_requests_[request] = base::WrapUnique(request);
  request->Start();
}

void SMSService::SendSMS(const std::string& promo_id,
                         const SMSService::PhoneNumberCallback& callback) {
  CompletionCallback completion_callback = base::Bind(
      &SMSService::SendSMSCallback, weak_ptr_factory_.GetWeakPtr(), callback);
  GURL url(kDesktopIOSPromotionSendSMS);
  Request* request = CreateRequest(url, completion_callback);
  request->SetPostData(
      base::StringPrintf(kSendSMSPromoFormat, promo_id.c_str()));
  pending_requests_[request] = base::WrapUnique(request);
  request->Start();
}

void SMSService::QueryPhoneNumberCompletionCallback(
    const SMSService::PhoneNumberCallback& callback,
    SMSService::Request* request,
    bool success) {
  std::unique_ptr<Request> request_ptr = std::move(pending_requests_[request]);
  pending_requests_.erase(request);

  std::string phone_number;
  bool has_number = false;
  std::unique_ptr<base::Value> value =
      base::JSONReader::Read(request->GetResponseBody());
  if (value.get() && value.get()->is_dict()) {
    const base::DictionaryValue* dictionary;
    if (value->GetAsDictionary(&dictionary)) {
      const base::ListValue* number_list;
      if (dictionary->GetList("phoneNumber", &number_list)) {
        const base::DictionaryValue* sub_dictionary;
        // For now only handle the first number.
        if (number_list->GetSize() > 0 &&
            number_list->GetDictionary(0, &sub_dictionary)) {
          if (sub_dictionary->GetString("phoneNumber", &phone_number))
            has_number = true;
        }
      }
    }
  }
  callback.Run(request, success && has_number, phone_number);
}

void SMSService::SendSMSCallback(
    const SMSService::PhoneNumberCallback& callback,
    SMSService::Request* request,
    bool success) {
  std::unique_ptr<Request> request_ptr = std::move(pending_requests_[request]);
  pending_requests_.erase(request);

  std::string phone_number;
  bool has_number = false;
  std::unique_ptr<base::Value> value =
      base::JSONReader::Read(request->GetResponseBody());
  if (value.get() && value.get()->is_dict()) {
    const base::DictionaryValue* dictionary;
    if (value->GetAsDictionary(&dictionary)) {
      if (dictionary->GetString("phoneNumber", &phone_number))
        has_number = true;
    }
  }
  callback.Run(request, success && has_number, phone_number);
}
