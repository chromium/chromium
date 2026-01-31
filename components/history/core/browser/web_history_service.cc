// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/web_history_service.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/web_history_service_observer.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/sync/base/features.h"
#include "components/sync/base/sync_util.h"
#include "components/sync/protocol/history_status.pb.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace history {

namespace {

const char kOldQueryHistoryUrl[] =
    "https://history.google.com/history/api/lookup?client=chrome";
const char kNewQueryHistoryUrl[] =
    "https://footprints-pa.googleapis.com/v1/read_chrome_history";

const char kOldHistoryDeleteHistoryUrl[] =
    "https://history.google.com/history/api/delete?client=chrome";
const char kNewHistoryDeleteHistoryUrl[] =
    "https://footprints-pa.googleapis.com/v1/delete_chrome_history";

const char kOldQueryWebAndAppActivityUrl[] =
    "https://history.google.com/history/api/lookup?client=web_app";
const char kNewQueryWebAndAppActivityUrl[] =
    "https://footprints-pa.googleapis.com/v1/get_facs";

const char kQueryOtherFormsOfBrowsingHistoryUrlSuffix[] = "/historystatus";

const char kPostDataMimeType[] = "text/plain";

const char kSyncProtoMimeType[] = "application/octet-stream";

// The maximum number of retries for the SimpleURLLoader requests.
const size_t kMaxRetries = 1;

class RequestImpl : public WebHistoryService::Request {
 public:
  ~RequestImpl() override = default;

  // Returns the response code received from the server, which will only be
  // valid if the request succeeded.
  int GetResponseCode() const override { return response_code_; }

  // Returns the contents of the response body received from the server.
  const std::string& GetResponseBody() const override { return response_body_; }

  bool IsPending() const override { return is_pending_; }

 private:
  friend class history::WebHistoryService;

  RequestImpl(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& url,
      WebHistoryService::CompletionCallback callback,
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      : identity_manager_(identity_manager),
        url_loader_factory_(std::move(url_loader_factory)),
        url_(url),
        post_data_mime_type_(kPostDataMimeType),
        callback_(std::move(callback)),
        partial_traffic_annotation_(partial_traffic_annotation) {
    DCHECK(identity_manager_);
    DCHECK(url_loader_factory_);
  }

  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info) {
    access_token_fetcher_.reset();

    if (error.state() != GoogleServiceAuthError::NONE) {
      is_pending_ = false;
      std::move(callback_).Run(this, false);

      // It is valid for the callback to delete `this`, so do not access any
      // members below here.
      return;
    }

    DCHECK(!access_token_info.token.empty());
    access_token_ = access_token_info.token;

    // Got an access token -- start the actual API request.
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::CompleteNetworkTrafficAnnotation("web_history_service",
                                              partial_traffic_annotation_,
                                              R"(
          semantics {
            sender: "Web History"
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "To disable this feature, users can either sign out or disable "
              "history sync via unchecking 'History' setting under 'Advanced "
              "sync settings."
          })");
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url_;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    resource_request->method = post_data_ ? "POST" : "GET";
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                        "Bearer " + access_token_info.token);
    resource_request->headers.SetHeader(
        "X-Developer-Key", GaiaUrls::GetInstance()->oauth2_chrome_client_id());
    if (!user_agent_.empty()) {
      resource_request->headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                          user_agent_);
    }
    simple_url_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    if (post_data_) {
      simple_url_loader_->AttachStringForUpload(post_data_.value(),
                                                post_data_mime_type_);
    }
    simple_url_loader_->SetRetryOptions(kMaxRetries,
                                        network::SimpleURLLoader::RETRY_ON_5XX);
    simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&RequestImpl::OnSimpleLoaderComplete,
                       base::Unretained(this)));
  }

  // Tells the request to do its thang.
  void Start() override {
    // TODO(crbug.com/40066882): Simplify this once
    // kReplaceSyncPromosWithSignInPromos has rolled out on all platforms.
    signin::ConsentLevel consent_level = signin::ConsentLevel::kSync;
    if (base::FeatureList::IsEnabled(
            syncer::kReplaceSyncPromosWithSignInPromos)) {
      consent_level = signin::ConsentLevel::kSignin;
    }

    access_token_fetcher_ =
        std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
            signin::OAuthConsumerId::kWebHistoryService, identity_manager_,
            base::BindOnce(&RequestImpl::OnAccessTokenFetchComplete,
                           base::Unretained(this)),
            signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
            consent_level);
    is_pending_ = true;
  }

  void OnSimpleLoaderComplete(std::optional<std::string> response_body) {
    response_code_ = -1;
    if (simple_url_loader_->ResponseInfo() &&
        simple_url_loader_->ResponseInfo()->headers) {
      response_code_ =
          simple_url_loader_->ResponseInfo()->headers->response_code();
    }
    simple_url_loader_.reset();

    // If the response code indicates that the token might not be valid,
    // invalidate the token and try again.
    if (response_code_ == net::HTTP_UNAUTHORIZED && ++auth_retry_count_ <= 1) {
      identity_manager_->RemoveAccessTokenFromCache(
          identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          signin::OAuthConsumerId::kWebHistoryService, access_token_);

      access_token_.clear();
      Start();
      return;
    }
    if (response_body) {
      response_body_ = std::move(response_body).value();
    } else {
      response_body_.clear();
    }
    is_pending_ = false;
    std::move(callback_).Run(this, true);
    // It is valid for the callback to delete `this`, so do not access any
    // members below here.
  }

  void SetPostData(const std::string& post_data) override {
    SetPostDataAndType(post_data, kPostDataMimeType);
  }

  void SetPostDataAndType(const std::string& post_data,
                          const std::string& mime_type) override {
    post_data_ = post_data;
    post_data_mime_type_ = mime_type;
  }

  void SetUserAgent(const std::string& user_agent) override {
    user_agent_ = user_agent;
  }

  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The URL of the API endpoint.
  GURL url_;

  // POST data to be sent with the request (may be empty).
  std::optional<std::string> post_data_;

  // MIME type of the post requests. Defaults to text/plain.
  std::string post_data_mime_type_;

  // The user agent header used with this request.
  std::string user_agent_;

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // The current OAuth2 access token.
  std::string access_token_;

  // Handles the actual API requests after the OAuth token is acquired.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Holds the response code received from the server.
  int response_code_ = 0;

  // Holds the response body received from the server.
  std::string response_body_;

  // The number of times this request has already been retried due to
  // authorization problems.
  int auth_retry_count_ = 0;

  // The callback to execute when the query is complete.
  WebHistoryService::CompletionCallback callback_;

  // True if the request was started and has not yet completed, otherwise false.
  bool is_pending_ = false;

  // Partial Network traffic annotation used to create SimpleURLLoader for this
  // request.
  const net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation_;
};

// Converts a time into a string for use as a parameter in a request to the
// history server, in microseconds since the Unix epoch.
std::string ServerTimeString(base::Time time) {
  if (time < base::Time::UnixEpoch()) {
    return base::NumberToString(0);
  }
  return base::NumberToString(
      (time - base::Time::UnixEpoch()).InMicroseconds());
}

// Returns a URL for querying the history server for a query specified by
// `options`. `version_info`, if not empty, should be a token that was received
// from the server in response to a write operation. It is used to help ensure
// read consistency after a write.
GURL GetQueryUrl(const std::u16string& text_query,
                 const QueryOptions& options,
                 std::string_view version_info) {
  if (base::FeatureList::IsEnabled(kWebHistoryUseNewApi)) {
    // The new API passes the query params as POST data.
    return GURL(kNewQueryHistoryUrl);
  }

  GURL url = GURL(kOldQueryHistoryUrl);
  url = net::AppendQueryParameter(url, "titles", "1");

  // Take `begin_time`, `end_time`, and `max_count` from the original query
  // options, and convert them to the equivalent URL parameters. Note that
  // QueryOptions uses exclusive `end_time` while the history.google.com API
  // uses it inclusively, so we subtract 1us during conversion.

  base::Time end_time = options.end_time.is_null()
                            ? base::Time::Now()
                            : std::min(options.end_time - base::Microseconds(1),
                                       base::Time::Now());
  url = net::AppendQueryParameter(url, "max", ServerTimeString(end_time));

  if (!options.begin_time.is_null()) {
    url = net::AppendQueryParameter(url, "min",
                                    ServerTimeString(options.begin_time));
  }

  if (options.max_count) {
    url = net::AppendQueryParameter(url, "num",
                                    base::NumberToString(options.max_count));
  }

  if (!text_query.empty()) {
    url = net::AppendQueryParameter(url, "q", base::UTF16ToUTF8(text_query));
  }

  if (!version_info.empty()) {
    url = net::AppendQueryParameter(url, "kvi", version_info);
  }

  return url;
}

GURL GetWebAndAppActivityUrl() {
  if (base::FeatureList::IsEnabled(kWebHistoryUseNewApi)) {
    return GURL(kNewQueryWebAndAppActivityUrl);
  }
  return GURL(kOldQueryWebAndAppActivityUrl);
}

GURL GetDeleteUrl(std::string_view version_info) {
  if (base::FeatureList::IsEnabled(kWebHistoryUseNewApi)) {
    // The new API passes the version_info as part of the post data.
    return GURL(kNewHistoryDeleteHistoryUrl);
  }
  GURL url(kOldHistoryDeleteHistoryUrl);
  // Append the version info token, if it is available, to help ensure
  // consistency with any previous deletions.
  if (!version_info.empty()) {
    url = net::AppendQueryParameter(url, "kvi", version_info);
  }
  return url;
}

base::DictValue BuildPostDataHeader(std::string_view version_info) {
  CHECK(base::FeatureList::IsEnabled(kWebHistoryUseNewApi));

  base::DictValue header;
  header.Set("application_id",
             GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  if (!version_info.empty()) {
    header.Set("version_info", version_info);
  }
  return header;
}

std::string BuildQueryPostData(const std::u16string& text_query,
                               const QueryOptions& options,
                               std::string_view version_info) {
  CHECK(base::FeatureList::IsEnabled(kWebHistoryUseNewApi));

  base::DictValue request;

  request.Set("header", BuildPostDataHeader(version_info));

  base::ListValue lookup_list;

  // Take `begin_time`, `end_time`, and `max_count` from the original query
  // options. Note that QueryOptions uses exclusive `end_time` while the API
  // uses it inclusively, so subtract 1us during conversion.
  base::DictValue lookup;
  lookup.Set("max_num_results", options.max_count);
  base::Time end_time = options.end_time.is_null()
                            ? base::Time::Now()
                            : std::min(options.end_time - base::Microseconds(1),
                                       base::Time::Now());
  lookup.Set("max_timestamp_usec", ServerTimeString(end_time));
  if (!options.begin_time.is_null()) {
    lookup.Set("min_timestamp_usec", ServerTimeString(options.begin_time));
  }
  if (!text_query.empty()) {
    lookup.Set("query", text_query);
  }
  lookup_list.Append(std::move(lookup));

  request.Set("lookup", std::move(lookup_list));

  return base::WriteJson(request).value_or("");
}

std::string BuildGetFacsPostData(std::string_view version_info) {
  CHECK(base::FeatureList::IsEnabled(kWebHistoryUseNewApi));

  base::DictValue request;

  request.Set("header", BuildPostDataHeader(version_info));

  request.Set("setting", /*WEB_AND_APP_ACTIVITY*/ 1);

  return base::WriteJson(request).value_or("");
}

// Creates a dictionary to hold the parameters for a deletion.
// `url` may be empty, indicating a time-range deletion.
base::DictValue CreateDeletion(std::string_view min_time,
                               std::string_view max_time,
                               const GURL& url) {
  base::DictValue deletion;

  if (!base::FeatureList::IsEnabled(kWebHistoryUseNewApi)) {
    deletion.Set("type", "CHROME_HISTORY");
  }

  if (url.is_valid()) {
    deletion.Set("url", url.spec());
  }
  deletion.Set("min_timestamp_usec", min_time);
  deletion.Set("max_timestamp_usec", max_time);
  return deletion;
}

std::string BuildDeletePostData(
    const std::vector<ExpireHistoryArgs>& expire_list,
    std::string_view version_info) {
  base::DictValue request;
  if (base::FeatureList::IsEnabled(kWebHistoryUseNewApi)) {
    request.Set("header", BuildPostDataHeader(version_info));
  }

  const base::Time now = base::Time::Now();
  base::ListValue deletions;
  for (const auto& expire : expire_list) {
    // Convert the times to server timestamps.
    std::string min_timestamp = ServerTimeString(expire.begin_time);
    base::Time end_time = expire.end_time;
    if (end_time.is_null() || end_time > now) {
      end_time = now;
    }
    std::string max_timestamp = ServerTimeString(end_time);

    for (const auto& url : expire.urls) {
      deletions.Append(CreateDeletion(min_timestamp, max_timestamp, url));
    }
    // If no URLs were specified, delete everything in the time range.
    if (expire.urls.empty()) {
      deletions.Append(CreateDeletion(min_timestamp, max_timestamp, GURL()));
    }
  }

  if (base::FeatureList::IsEnabled(kWebHistoryUseNewApi)) {
    request.Set("delete_chrome_history", std::move(deletions));
  } else {
    request.Set("del", std::move(deletions));
  }

  return base::WriteJson(request).value_or("");
}

WebHistoryService::QueryHistoryResult ParseQueryResponseOldApi(
    const base::DictValue& response) {
  CHECK(!base::FeatureList::IsEnabled(kWebHistoryUseNewApi));

  WebHistoryService::QueryHistoryResult query_history_result;

  if (const base::ListValue* events = response.FindList("event")) {
    query_history_result.visits.reserve(events->size());

    for (const base::Value& event : *events) {
      const base::DictValue* event_dict = event.GetIfDict();
      if (!event_dict) {
        continue;
      }
      const base::ListValue* results = event_dict->FindList("result");
      if (!results || results->empty()) {
        continue;
      }
      const base::DictValue* result = results->front().GetIfDict();
      if (!result) {
        continue;
      }
      const std::string* url_str = result->FindString("url");
      if (!url_str) {
        continue;
      }
      const base::ListValue* ids = result->FindList("id");
      if (!ids || ids->empty()) {
        continue;
      }

      GURL url(*url_str);

      // Title is optional.
      const std::string* title = result->FindString("title");

      // Favicon URL is optional.
      const std::string* favicon_url = result->FindString("favicon_url");

      // Extract the timestamps of all the visits to this URL.
      // They are referred to as "IDs" by the server.
      for (const base::Value& id : *ids) {
        const base::DictValue* id_dict = id.GetIfDict();
        const std::string* timestamp_string;
        int64_t timestamp_usec = 0;
        if (!id_dict ||
            !(timestamp_string = id_dict->FindString("timestamp_usec")) ||
            !base::StringToInt64(*timestamp_string, &timestamp_usec)) {
          continue;
        }

        WebHistoryService::QueryHistoryResult::Visit result_visit;

        result_visit.url = url;
        if (title) {
          result_visit.title = *title;
        }
        if (favicon_url) {
          result_visit.favicon_url = GURL(*favicon_url);
        }

        // The timestamp on the server is a Unix time in microseconds.
        result_visit.timestamp =
            base::Time::UnixEpoch() + base::Microseconds(timestamp_usec);

        // Get the ID of the client that this visit came from.
        if (const std::string* client_id = id_dict->FindString("client_id")) {
          result_visit.client_id = *client_id;
        }

        query_history_result.visits.push_back(std::move(result_visit));
      }
    }
  }
  const std::string* continuation_token =
      response.FindString("continuation_token");
  query_history_result.has_more_results =
      continuation_token && !continuation_token->empty();

  return query_history_result;
}

WebHistoryService::QueryHistoryResult ParseQueryResponseNewApi(
    const base::DictValue& response) {
  CHECK(base::FeatureList::IsEnabled(kWebHistoryUseNewApi));

  WebHistoryService::QueryHistoryResult query_history_result;

  const base::ListValue* lookups = response.FindList("lookup");
  if (!lookups || lookups->empty()) {
    return query_history_result;
  }

  const base::DictValue* lookup_dict = lookups->front().GetIfDict();
  if (!lookup_dict) {
    return query_history_result;
  }

  // There should be exactly one lookup in the response.
  const base::ListValue* history_entries =
      lookup_dict->FindList("chromeHistory");
  if (!history_entries || history_entries->empty()) {
    return query_history_result;
  }

  query_history_result.visits.reserve(history_entries->size());

  for (const base::Value& entry : *history_entries) {
    const base::DictValue* entry_dict = entry.GetIfDict();
    if (!entry_dict) {
      continue;
    }

    // URL and timestamp are required.
    const std::string* url = entry_dict->FindString("url");
    if (!url) {
      continue;
    }
    const std::string* timestamp_string;
    int64_t timestamp_usec = 0;
    if (!(timestamp_string = entry_dict->FindString("timestamp")) ||
        !base::StringToInt64(*timestamp_string, &timestamp_usec)) {
      continue;
    }
    // Remaining fields are optional.
    const std::string* title = entry_dict->FindString("title");
    const std::string* favicon_url = entry_dict->FindString("faviconUrl");
    const std::string* client_id = entry_dict->FindString("clientId");

    WebHistoryService::QueryHistoryResult::Visit result_visit;
    result_visit.url = GURL(*url);
    if (title) {
      result_visit.title = *title;
    }
    if (favicon_url) {
      result_visit.favicon_url = GURL(*favicon_url);
    }
    if (client_id) {
      result_visit.client_id = *client_id;
    }
    result_visit.timestamp =
        base::Time::UnixEpoch() + base::Microseconds(timestamp_usec);

    query_history_result.visits.push_back(std::move(result_visit));
  }

  query_history_result.has_more_results =
      lookup_dict->FindBool("hasMoreResults").value_or(false);

  return query_history_result;
}

WebHistoryService::QueryHistoryResult ParseQueryResponse(
    const base::DictValue& response) {
  if (base::FeatureList::IsEnabled(kWebHistoryUseNewApi)) {
    return ParseQueryResponseNewApi(response);
  } else {
    return ParseQueryResponseOldApi(response);
  }
}

}  // namespace

WebHistoryService::Request::Request() = default;
WebHistoryService::Request::~Request() = default;

WebHistoryService::QueryHistoryResult::QueryHistoryResult() = default;
WebHistoryService::QueryHistoryResult::QueryHistoryResult(
    const QueryHistoryResult&) = default;
WebHistoryService::QueryHistoryResult::QueryHistoryResult(
    QueryHistoryResult&&) = default;
WebHistoryService::QueryHistoryResult::~QueryHistoryResult() = default;

WebHistoryService::QueryHistoryResult::Visit::Visit() = default;
WebHistoryService::QueryHistoryResult::Visit::Visit(const Visit&) = default;
WebHistoryService::QueryHistoryResult::Visit::Visit(Visit&&) = default;
WebHistoryService::QueryHistoryResult::Visit::~Visit() = default;

WebHistoryService::WebHistoryService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {}

WebHistoryService::~WebHistoryService() = default;

void WebHistoryService::AddObserver(WebHistoryServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void WebHistoryService::RemoveObserver(WebHistoryServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

std::unique_ptr<WebHistoryService::Request> WebHistoryService::CreateRequest(
    const GURL& url,
    CompletionCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  // Can't use std::make_unique due to private constructor.
  return base::WrapUnique(
      new RequestImpl(identity_manager_, url_loader_factory_, url,
                      std::move(callback), partial_traffic_annotation));
}

// static
std::optional<base::DictValue> WebHistoryService::ReadResponse(
    const WebHistoryService::Request& request) {
  if (request.GetResponseCode() != net::HTTP_OK) {
    return std::nullopt;
  }
  std::optional<base::Value> value = base::JSONReader::Read(
      request.GetResponseBody(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (value && value->is_dict()) {
    return std::move(*value).TakeDict();
  }
  DLOG(WARNING) << "Non-JSON response received from history server.";
  return std::nullopt;
}

std::unique_ptr<WebHistoryService::Request> WebHistoryService::QueryHistory(
    const std::u16string& text_query,
    const QueryOptions& options,
    WebHistoryService::QueryWebHistoryCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback = base::BindOnce(
      &WebHistoryService::QueryHistoryCompletionCallback, std::move(callback));

  GURL url = GetQueryUrl(text_query, options, server_version_info_);
  std::unique_ptr<Request> request(CreateRequest(
      url, std::move(completion_callback), partial_traffic_annotation));
  if (base::FeatureList::IsEnabled(kWebHistoryUseNewApi)) {
    request->SetPostData(
        BuildQueryPostData(text_query, options, server_version_info_));
  }
  request->Start();
  return request;
}

void WebHistoryService::ExpireHistory(
    const std::vector<ExpireHistoryArgs>& expire_list,
    ExpireWebHistoryCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  GURL url = GetDeleteUrl(server_version_info_);

  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback =
      base::BindOnce(&WebHistoryService::ExpireHistoryCompletionCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  std::unique_ptr<Request> request(CreateRequest(
      url, std::move(completion_callback), partial_traffic_annotation));
  request->SetPostData(BuildDeletePostData(expire_list, server_version_info_));
  Request* request_ptr = request.get();
  pending_expire_requests_[request_ptr] = std::move(request);
  request_ptr->Start();
}

void WebHistoryService::ExpireHistoryBetween(
    const std::set<GURL>& restrict_urls,
    base::Time begin_time,
    base::Time end_time,
    ExpireWebHistoryCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  std::vector<ExpireHistoryArgs> expire_list(1);
  expire_list.back().urls = restrict_urls;
  expire_list.back().begin_time = begin_time;
  expire_list.back().end_time = end_time;
  ExpireHistory(expire_list, std::move(callback), partial_traffic_annotation);
}

void WebHistoryService::QueryWebAndAppActivity(
    QueryWebAndAppActivityCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback = base::BindOnce(
      &WebHistoryService::QueryWebAndAppActivityCompletionCallback,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  GURL url = GetWebAndAppActivityUrl();
  std::unique_ptr<Request> request = CreateRequest(
      url, std::move(completion_callback), partial_traffic_annotation);
  if (base::FeatureList::IsEnabled(kWebHistoryUseNewApi)) {
    request->SetPostData(BuildGetFacsPostData(server_version_info_));
  }
  Request* request_raw = request.get();
  pending_web_and_app_activity_requests_[request_raw] = std::move(request);
  request_raw->Start();
}

void WebHistoryService::QueryOtherFormsOfBrowsingHistory(
    version_info::Channel channel,
    QueryOtherFormsOfBrowsingHistoryCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback = base::BindOnce(
      &WebHistoryService::QueryOtherFormsOfBrowsingHistoryCompletionCallback,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  // Find the Sync request URL.
  GURL url = syncer::GetSyncServiceURL(*base::CommandLine::ForCurrentProcess(),
                                       channel);
  GURL::Replacements replace_path;
  std::string new_path =
      url.GetPath() + kQueryOtherFormsOfBrowsingHistoryUrlSuffix;
  replace_path.SetPathStr(new_path);
  url = url.ReplaceComponents(replace_path);
  DCHECK(url.is_valid());

  std::unique_ptr<Request> request = CreateRequest(
      url, std::move(completion_callback), partial_traffic_annotation);
  Request* request_raw = request.get();

  // Set the Sync-specific user agent.
  request->SetUserAgent(syncer::MakeUserAgentForSync(channel));

  pending_other_forms_of_browsing_history_requests_[request_raw] =
      std::move(request);

  // Set the request protobuf.
  sync_pb::HistoryStatusRequest request_proto;
  std::string post_data;
  request_proto.SerializeToString(&post_data);
  request_raw->SetPostDataAndType(post_data, kSyncProtoMimeType);

  request_raw->Start();
}

// static
void WebHistoryService::QueryHistoryCompletionCallback(
    WebHistoryService::QueryWebHistoryCallback callback,
    WebHistoryService::Request* request,
    bool success) {
  RequestOutcome outcome =
      QueryHistoryCompletionCallbackImpl(std::move(callback), request, success);
  base::UmaHistogramEnumeration("History.WebHistoryRequestOutcome.QueryHistory",
                                outcome);
}

// static
WebHistoryService::RequestOutcome
WebHistoryService::QueryHistoryCompletionCallbackImpl(
    WebHistoryService::QueryWebHistoryCallback callback,
    WebHistoryService::Request* request,
    bool success) {
  if (!success) {
    std::move(callback).Run(request, std::nullopt);
    return RequestOutcome::kFailure;
  }

  std::optional<base::DictValue> response = ReadResponse(*request);
  if (!response) {
    std::move(callback).Run(request, std::nullopt);
    return RequestOutcome::kInvalidResponse;
  }

  QueryHistoryResult result = ParseQueryResponse(*response);
  // Note: The histogram max of 150 is chosen to match `RESULTS_PER_PAGE` from
  // chrome/browser/resources/history/constants.ts and `kMaxQueryCount` from
  // chrome/browser/android/history/browsing_history_bridge.cc.
  base::UmaHistogramCustomCounts("History.WebHistory.QueryHistoryResultsCount",
                                 result.visits.size(), 0, 150, 50);
  std::move(callback).Run(request, std::move(result));
  return RequestOutcome::kSuccess;
}

void WebHistoryService::ExpireHistoryCompletionCallback(
    WebHistoryService::ExpireWebHistoryCallback callback,
    WebHistoryService::Request* request,
    bool success) {
  RequestOutcome outcome = ExpireHistoryCompletionCallbackImpl(
      std::move(callback), request, success);
  base::UmaHistogramEnumeration(
      "History.WebHistoryRequestOutcome.ExpireHistory", outcome);
}

WebHistoryService::RequestOutcome
WebHistoryService::ExpireHistoryCompletionCallbackImpl(
    WebHistoryService::ExpireWebHistoryCallback callback,
    WebHistoryService::Request* request,
    bool success) {
  std::unique_ptr<Request> request_ptr =
      std::move(pending_expire_requests_[request]);
  pending_expire_requests_.erase(request);

  if (!success) {
    std::move(callback).Run(/*success=*/false);
    return RequestOutcome::kFailure;
  }

  std::optional<base::DictValue> response = ReadResponse(*request);
  if (!response) {
    std::move(callback).Run(/*success=*/false);
    return RequestOutcome::kInvalidResponse;
  }

  if (base::FeatureList::IsEnabled(kWebHistoryUseNewApi)) {
    if (const auto* version = response->FindString("versionInfo")) {
      server_version_info_ = *version;
    }
  } else {
    if (const auto* version = response->FindString("version_info")) {
      server_version_info_ = *version;
    }
  }
  // Inform the observers about the history deletion.
  for (WebHistoryServiceObserver& observer : observer_list_) {
    observer.OnWebHistoryDeleted();
  }
  std::move(callback).Run(/*success=*/true);
  return RequestOutcome::kSuccess;
}

void WebHistoryService::QueryWebAndAppActivityCompletionCallback(
    WebHistoryService::QueryWebAndAppActivityCallback callback,
    WebHistoryService::Request* request,
    bool success) {
  RequestOutcome outcome = QueryWebAndAppActivityCompletionCallbackImpl(
      std::move(callback), request, success);
  base::UmaHistogramEnumeration(
      "History.WebHistoryRequestOutcome.QueryWebAndAppActivity", outcome);
}

WebHistoryService::RequestOutcome
WebHistoryService::QueryWebAndAppActivityCompletionCallbackImpl(
    WebHistoryService::QueryWebAndAppActivityCallback callback,
    WebHistoryService::Request* request,
    bool success) {
  std::unique_ptr<Request> request_ptr =
      std::move(pending_web_and_app_activity_requests_[request]);
  pending_web_and_app_activity_requests_.erase(request);

  if (!success) {
    std::move(callback).Run(/*web_and_app_activity_enabled=*/false);
    return RequestOutcome::kFailure;
  }

  std::optional<bool> enabled;
  if (std::optional<base::DictValue> response = ReadResponse(*request)) {
    if (base::FeatureList::IsEnabled(kWebHistoryUseNewApi)) {
      if (const base::ListValue* facs_setting =
              response->FindList("facsSetting")) {
        if (facs_setting->size() == 1) {
          if (const base::DictValue* setting_dict =
                  facs_setting->front().GetIfDict()) {
            enabled = setting_dict->FindBool("dataRecordingEnabled");
          }
        }
      }
    } else {
      enabled = response->FindBool("history_recording_enabled");
    }
  }

  if (enabled.has_value()) {
    base::UmaHistogramBoolean("History.WebHistory.QueryWebAndAppActivityResult",
                              *enabled);
    std::move(callback).Run(/*web_and_app_activity_enabled=*/*enabled);
    return RequestOutcome::kSuccess;
  } else {
    std::move(callback).Run(/*web_and_app_activity_enabled=*/false);
    return RequestOutcome::kInvalidResponse;
  }
}

void WebHistoryService::QueryOtherFormsOfBrowsingHistoryCompletionCallback(
    WebHistoryService::QueryOtherFormsOfBrowsingHistoryCallback callback,
    WebHistoryService::Request* request,
    bool success) {
  std::unique_ptr<Request> request_ptr =
      std::move(pending_other_forms_of_browsing_history_requests_[request]);
  pending_other_forms_of_browsing_history_requests_.erase(request);

  bool has_other_forms_of_browsing_history = false;
  if (success && request->GetResponseCode() == net::HTTP_OK) {
    sync_pb::HistoryStatusResponse history_status;
    if (history_status.ParseFromString(request->GetResponseBody())) {
      has_other_forms_of_browsing_history = history_status.has_derived_data();
    }
  }

  std::move(callback).Run(has_other_forms_of_browsing_history);
}

}  // namespace history
