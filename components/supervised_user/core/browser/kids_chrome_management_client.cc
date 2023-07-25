// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/kids_chrome_management_client.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/url_constants.h"

namespace {

enum class RequestMethod {
  kClassifyUrl,
};

// Corresponds to tools/metrics/histograms/enums.xml counterpart. Do not
// renumber entries as this breaks Uma metrics.
enum class KidsChromeManagementClientParsingResult {
  kSuccess = 0,
  kResponseDictionaryFailure = 1,
  kDisplayClassificationFailure = 2,
  kInvalidDisplayClassification = 3,
  kMaxValue = kInvalidDisplayClassification,
};

constexpr char kClassifyUrlDataContentType[] =
    "application/x-www-form-urlencoded";

constexpr char kClassifyUrlAuthErrorMetric[] =
    "FamilyLinkUser.ClassifyUrlRequest.AuthError";
constexpr char kClassifyUrlNetOrHttpStatusMetric[] =
    "FamilyLinkUser.ClassifyUrlRequest.NetOrHttpStatus";
constexpr char kClassifyUrlParsingResultMetric[] =
    "FamilyLinkUser.ClassifyUrlRequest.ParsingResult";
constexpr char kClassifyUrlLatencyMetric[] =
    "FamilyLinkUser.ClassifyUrlRequest.Latency";
constexpr char kClassifyUrlStatusMetric[] =
    "FamilyLinkUser.ClassifyUrlRequest.Status";

// Constants for ClassifyURL.
constexpr char kClassifyUrlOauthConsumerName[] = "kids_url_classifier";
constexpr char kClassifyUrlDataFormat[] = "url=%s&region_code=%s";
constexpr char kClassifyUrlAllowed[] = "allowed";
constexpr char kClassifyUrlRestricted[] = "restricted";

// TODO(crbug.com/980273): remove conversion methods when experiment flag is
// fully flipped. More info on crbug.com/978130.

// Converts the ClassifyUrlRequest proto to a serialized string in the
// format that the Kids Management API receives.
std::string GetClassifyURLRequestString(
    kids_chrome_management::ClassifyUrlRequest* request_proto) {
  std::string query =
      base::EscapeQueryParamValue(request_proto->url(), true /* use_plus */);
  return base::StringPrintf(kClassifyUrlDataFormat, query.c_str(),
                            request_proto->region_code().c_str());
}

// Converts the serialized string returned by the Kids Management API to a
// ClassifyUrlResponse proto object.
std::unique_ptr<kids_chrome_management::ClassifyUrlResponse>
GetClassifyURLResponseProto(const std::string& response) {
  absl::optional<base::Value> maybe_value = base::JSONReader::Read(response);
  const base::Value::Dict* dict = nullptr;
  if (maybe_value.has_value()) {
    dict = maybe_value->GetIfDict();
  }

  auto response_proto =
      std::make_unique<kids_chrome_management::ClassifyUrlResponse>();

  if (!dict) {
    DLOG(WARNING)
        << "GetClassifyURLResponseProto failed to parse response dictionary";
    base::UmaHistogramEnumeration(
        kClassifyUrlParsingResultMetric,
        KidsChromeManagementClientParsingResult::kResponseDictionaryFailure);
    response_proto->set_display_classification(
        kids_chrome_management::ClassifyUrlResponse::
            UNKNOWN_DISPLAY_CLASSIFICATION);
    return response_proto;
  }

  const std::string* maybe_classification_string =
      dict->FindString("displayClassification");
  if (!maybe_classification_string) {
    DLOG(WARNING)
        << "GetClassifyURLResponseProto failed to parse displayClassification";
    base::UmaHistogramEnumeration(
        kClassifyUrlParsingResultMetric,
        KidsChromeManagementClientParsingResult::kDisplayClassificationFailure);
    response_proto->set_display_classification(
        kids_chrome_management::ClassifyUrlResponse::
            UNKNOWN_DISPLAY_CLASSIFICATION);
    return response_proto;
  }

  const std::string classification_string = *maybe_classification_string;
  if (classification_string == kClassifyUrlAllowed) {
    response_proto->set_display_classification(
        kids_chrome_management::ClassifyUrlResponse::ALLOWED);
  } else if (classification_string == kClassifyUrlRestricted) {
    response_proto->set_display_classification(
        kids_chrome_management::ClassifyUrlResponse::RESTRICTED);
  } else {
    DLOG(WARNING)
        << "GetClassifyURLResponseProto expected a valid displayClassification";
    base::UmaHistogramEnumeration(
        kClassifyUrlParsingResultMetric,
        KidsChromeManagementClientParsingResult::kInvalidDisplayClassification);
    response_proto->set_display_classification(
        kids_chrome_management::ClassifyUrlResponse::
            UNKNOWN_DISPLAY_CLASSIFICATION);
  }

  base::UmaHistogramEnumeration(
      kClassifyUrlParsingResultMetric,
      KidsChromeManagementClientParsingResult::kSuccess);
  return response_proto;
}

std::unique_ptr<network::ResourceRequest>
CreateResourceRequestForUrlClassifier() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      supervised_user::KidsManagementClassifyURLRequestURL();
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  return resource_request;
}

}  // namespace

struct KidsChromeManagementClient::KidsChromeManagementRequest {
  KidsChromeManagementRequest(
      std::unique_ptr<google::protobuf::MessageLite> request_proto,
      KidsChromeManagementClient::KidsChromeManagementCallback callback,
      std::unique_ptr<network::ResourceRequest> resource_request,
      const net::NetworkTrafficAnnotationTag traffic_annotation,
      const char* oauth_consumer_name,
      const char* scope,
      const RequestMethod method)
      : request_proto(std::move(request_proto)),
        callback(std::move(callback)),
        resource_request(std::move(resource_request)),
        traffic_annotation(traffic_annotation),
        access_token_expired(false),
        oauth_consumer_name(oauth_consumer_name),
        scope(scope),
        method(method) {}

  KidsChromeManagementRequest(KidsChromeManagementRequest&&) = default;

  ~KidsChromeManagementRequest() { DCHECK(!callback); }

  std::unique_ptr<google::protobuf::MessageLite> request_proto;
  KidsChromeManagementCallback callback;
  std::unique_ptr<network::ResourceRequest> resource_request;
  const net::NetworkTrafficAnnotationTag traffic_annotation;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher;
  bool access_token_expired;
  const char* oauth_consumer_name;
  const char* scope;
  const RequestMethod method;
  const base::TimeTicks start_time_{base::TimeTicks::Now()};
};

KidsChromeManagementClient::KidsChromeManagementClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager) {}

KidsChromeManagementClient::~KidsChromeManagementClient() = default;

void KidsChromeManagementClient::ClassifyURL(
    std::unique_ptr<kids_chrome_management::ClassifyUrlRequest> request_proto,
    KidsChromeManagementCallback callback) {
  DVLOG(1) << "Checking URL:  " << request_proto->url();

  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "kids_chrome_management_client_classify_url", R"(
        semantics {
          sender: "Supervised Users"
          description:
            "Checks whether a given URL (or set of URLs) is considered safe by "
            "a Google Family Link web restrictions API."
          trigger:
            "If the parent enabled this feature for the child account, this is "
            "sent for every navigation."
          data:
            "An OAuth2 access token identifying and authenticating the "
            "Google account, and the URL to be checked."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "chrome-kids-eng@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2023-02-13"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is only used in child accounts and cannot be "
            "disabled by settings. Parent accounts can disable it in the "
            "family dashboard."
          policy_exception_justification:
            "Enterprise admins don't have control over this feature "
            "because it can't be enabled on enterprise environments."
        })");

  auto kids_chrome_request = std::make_unique<KidsChromeManagementRequest>(
      std::move(request_proto), std::move(callback),
      CreateResourceRequestForUrlClassifier(), traffic_annotation,
      kClassifyUrlOauthConsumerName,
      GaiaConstants::kClassifyUrlKidPermissionOAuth2Scope,
      RequestMethod::kClassifyUrl);

  MakeHTTPRequest(std::move(kids_chrome_request));
}

void KidsChromeManagementClient::MakeHTTPRequest(
    std::unique_ptr<KidsChromeManagementRequest> kids_chrome_request) {
  requests_in_progress_.push_front(std::move(kids_chrome_request));

  StartFetching(requests_in_progress_.begin());
}

void KidsChromeManagementClient::StartFetching(
    KidsChromeRequestList::iterator it) {
  KidsChromeManagementRequest* req = it->get();

  // This is a quick fix for https://crbug.com/1192222. `resource_request` is
  // moved during creation of SimpleURLLoader. Retrying the request causes
  // dereferencing nullptr. To avoid that recreate the `resource_request` here.
  if (!req->resource_request) {
    req->resource_request = CreateResourceRequestForUrlClassifier();
  }

  signin::ScopeSet scopes{req->scope};

  req->access_token_fetcher =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          req->oauth_consumer_name, identity_manager_, scopes,
          base::BindOnce(
              &KidsChromeManagementClient::OnAccessTokenFetchComplete,
              base::Unretained(this), it),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
          // This class doesn't care about browser sync consent.
          signin::ConsentLevel::kSignin);
}

void KidsChromeManagementClient::OnAccessTokenFetchComplete(
    KidsChromeRequestList::iterator it,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    DLOG(WARNING) << "Token error: " << error.ToString();
    base::UmaHistogramEnumeration(kClassifyUrlAuthErrorMetric, error.state(),
                                  GoogleServiceAuthError::NUM_STATES);
    std::unique_ptr<google::protobuf::MessageLite> response_proto;
    DispatchResult(
        it, std::move(response_proto),
        KidsChromeManagementClient::ErrorCode::kTokenError,
        supervised_user::ProtoFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR);
    return;
  }

  KidsChromeManagementRequest* req = it->get();

  req->resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::JoinString(
          {supervised_user::kAuthorizationHeader, token_info.token}, " "));

  std::string request_data;
  // TODO(crbug.com/980273): remove this when experiment flag is fully flipped.
  if (req->method == RequestMethod::kClassifyUrl) {
    request_data = GetClassifyURLRequestString(
        static_cast<kids_chrome_management::ClassifyUrlRequest*>(
            req->request_proto.get()));
  } else {
    DVLOG(1) << "Could not detect the request proto's class.";
    std::unique_ptr<google::protobuf::MessageLite> response_proto;
    DispatchResult(
        it, std::move(response_proto),
        KidsChromeManagementClient::ErrorCode::kServiceError,
        supervised_user::ProtoFetcherStatus::State::INVALID_RESPONSE);
    return;
  }

  requests_loaders_[req] = network::SimpleURLLoader::Create(
      std::move(req->resource_request), req->traffic_annotation);
  network::SimpleURLLoader* simple_url_loader = requests_loaders_[req].get();

  simple_url_loader->AttachStringForUpload(request_data,
                                           kClassifyUrlDataContentType);

  // `this` is guaranteed to exist when the callback is called, because the
  // KidsChromeManagementClient class owns `simple_url_loader`, and deleting the
  // the simple_url_loader during the request will cause the callback not to be
  // called.
  // TODO(https://crbug.com/1444748): Write a test making sure that this cannot
  // cause a UAF. The test needs to:
  // - Start a ClassifyURL request.
  // - Start a loader and pause it before completion.
  // - Kill the KidsChromeManagementClient. Note that this will not work unless
  //   DCHECKs are turned off, because we otherwise check that callback_ has
  //   been run.
  simple_url_loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&KidsChromeManagementClient::OnSimpleLoaderComplete,
                     base::Unretained(this), it, token_info),
      /*max_body_size*/ 128);
}

void KidsChromeManagementClient::OnSimpleLoaderComplete(
    KidsChromeRequestList::iterator it,
    signin::AccessTokenInfo token_info,
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;

  KidsChromeManagementRequest* req = it->get();

  // Get back the SimpleURLLoader from the stored map.
  CHECK(requests_loaders_[req]);
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      std::move(requests_loaders_[req]);
  requests_loaders_.erase(req);

  if (simple_url_loader->ResponseInfo() &&
      simple_url_loader->ResponseInfo()->headers) {
    response_code = simple_url_loader->ResponseInfo()->headers->response_code();

    // Handle first HTTP_UNAUTHORIZED response by removing access token and
    // restarting the request from the beginning (fetching access token).
    if (response_code == net::HTTP_UNAUTHORIZED && !req->access_token_expired) {
      DLOG(WARNING) << "Access token expired:\n" << token_info.token;
      // Do not record metrics in here to avoid double-counting.
      req->access_token_expired = true;
      signin::ScopeSet scopes{req->scope};
      identity_manager_->RemoveAccessTokenFromCache(
          identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          scopes, token_info.token);
      StartFetching(it);
      return;
    }
  }

  std::unique_ptr<google::protobuf::MessageLite> response_proto;

  int net_error = simple_url_loader->NetError();

  if (net_error != net::OK) {
    DLOG(WARNING) << "Network error " << net_error;
    base::UmaHistogramSparse(kClassifyUrlNetOrHttpStatusMetric, net_error);
    DispatchResult(
        it, std::move(response_proto),
        KidsChromeManagementClient::ErrorCode::kNetworkError,
        supervised_user::ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR);
    return;
  }

  if (response_code != net::HTTP_OK) {
    DLOG(WARNING) << "Response: " << response_body.get();
    base::UmaHistogramSparse(kClassifyUrlNetOrHttpStatusMetric, response_code);
    DispatchResult(
        it, std::move(response_proto),
        KidsChromeManagementClient::ErrorCode::kHttpError,
        supervised_user::ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR);
    return;
  }

  // |response_body| is nullptr only in case of failure.
  if (!response_body) {
    DLOG(WARNING) << "URL request failed! Letting through...";
    DispatchResult(
        it, std::move(response_proto),
        KidsChromeManagementClient::ErrorCode::kNetworkError,
        supervised_user::ProtoFetcherStatus::State::INVALID_RESPONSE);
    return;
  }

  if (it->get()->method == RequestMethod::kClassifyUrl) {
    response_proto = GetClassifyURLResponseProto(*response_body);
  } else {
    DVLOG(1) << "Could not detect the request proto class.";
    DispatchResult(
        it, std::move(response_proto),
        KidsChromeManagementClient::ErrorCode::kServiceError,
        supervised_user::ProtoFetcherStatus::State::INVALID_RESPONSE);
    return;
  }

  DispatchResult(it, std::move(response_proto),
                 KidsChromeManagementClient::ErrorCode::kSuccess,
                 supervised_user::ProtoFetcherStatus::State::OK);
}

void KidsChromeManagementClient::DispatchResult(
    KidsChromeRequestList::iterator it,
    std::unique_ptr<google::protobuf::MessageLite> response_proto,
    ErrorCode error,
    supervised_user::ProtoFetcherStatus::State status) {
  KidsChromeManagementRequest* request = it->get();

  // Record metrics for a/b testing of EnableProtoApiForClassifyUrl experiment.
  base::TimeDelta latency = base::TimeTicks::Now() - request->start_time_;
  base::UmaHistogramTimes(kClassifyUrlLatencyMetric, latency);
  base::UmaHistogramEnumeration(kClassifyUrlStatusMetric, status);

  std::move(request->callback).Run(std::move(response_proto), error);

  requests_in_progress_.erase(it);
}
