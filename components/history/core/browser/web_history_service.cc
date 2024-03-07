// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/web_history_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/history/core/browser/web_history_service_observer.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/sync/base/features.h"
#include "components/sync/base/sync_util.h"
#include "components/sync/protocol/history_status.pb.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace history {

namespace {

const char kHistoryOAuthScope[] = "https://www.googleapis.com/auth/chromesync";

const char kHistoryQueryHistoryUrl[] =
    "https://history.google.com/history/api/lookup?client=chrome";

const char kHistoryDeleteHistoryUrl[] =
    "https://history.google.com/history/api/delete?client=chrome";

const char kHistoryAudioHistoryUrl[] =
    "https://history.google.com/history/api/lookup?client=audio";

const char kHistoryAudioHistoryChangeUrl[] =
    "https://history.google.com/history/api/change";

const char kQueryWebAndAppActivityUrl[] =
    "https://history.google.com/history/api/lookup?client=web_app";

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
  int GetResponseCode() override { return response_code_; }

  // Returns the contents of the response body received from the server.
  const std::string& GetResponseBody() override { return response_body_; }

  bool IsPending() override { return is_pending_; }

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
    signin::ScopeSet oauth_scopes;
    oauth_scopes.insert(kHistoryOAuthScope);

    // TODO(crbug.com/40066882): Simplify this once
    // kReplaceSyncPromosWithSignInPromos has rolled out on all platforms.
    signin::ConsentLevel consent_level = signin::ConsentLevel::kSync;
    if (base::FeatureList::IsEnabled(
            syncer::kReplaceSyncPromosWithSignInPromos)) {
      consent_level = signin::ConsentLevel::kSignin;
    }

    access_token_fetcher_ =
        std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
            "web_history", identity_manager_, oauth_scopes,
            base::BindOnce(&RequestImpl::OnAccessTokenFetchComplete,
                           base::Unretained(this)),
            signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
            consent_level);
    is_pending_ = true;
  }

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body) {
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
      signin::ScopeSet oauth_scopes;
      oauth_scopes.insert(kHistoryOAuthScope);
      identity_manager_->RemoveAccessTokenFromCache(
          identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          oauth_scopes, access_token_);

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
// history server.
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
                 const std::string& version_info) {
  GURL url = GURL(kHistoryQueryHistoryUrl);
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

// Creates a dictionary to hold the parameters for a deletion.
// `url` may be empty, indicating a time-range deletion.
base::Value::Dict CreateDeletion(const std::string& min_time,
                                 const std::string& max_time,
                                 const GURL& url) {
  base::Value::Dict deletion;
  deletion.Set("type", "CHROME_HISTORY");
  if (url.is_valid()) {
    deletion.Set("url", url.spec());
  }
  deletion.Set("min_timestamp_usec", min_time);
  deletion.Set("max_timestamp_usec", max_time);
  return deletion;
}

}  // namespace

WebHistoryService::Request::Request() = default;

WebHistoryService::Request::~Request() = default;

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

WebHistoryService::Request* WebHistoryService::CreateRequest(
    const GURL& url,
    CompletionCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  return new RequestImpl(identity_manager_, url_loader_factory_, url,
                         std::move(callback), partial_traffic_annotation);
}

// static
std::optional<base::Value::Dict> WebHistoryService::ReadResponse(
    WebHistoryService::Request* request) {
  if (request->GetResponseCode() != net::HTTP_OK) {
    return std::nullopt;
  }
  std::optional<base::Value> value =
      base::JSONReader::Read(request->GetResponseBody());
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
  request->Start();
  return request;
}

void WebHistoryService::ExpireHistory(
    const std::vector<ExpireHistoryArgs>& expire_list,
    ExpireWebHistoryCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  base::Value::Dict delete_request;
  base::Value::List deletions;
  base::Time now = base::Time::Now();

  for (const auto& expire : expire_list) {
    // Convert the times to server timestamps.
    std::string min_timestamp = ServerTimeString(expire.begin_time);
    // TODO(dubroy): Use sane time (crbug.com/146090) here when it's available.
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
  delete_request.Set("del", std::move(deletions));
  std::string post_data;
  base::JSONWriter::Write(delete_request, &post_data);

  GURL url(kHistoryDeleteHistoryUrl);

  // Append the version info token, if it is available, to help ensure
  // consistency with any previous deletions.
  if (!server_version_info_.empty()) {
    url = net::AppendQueryParameter(url, "kvi", server_version_info_);
  }

  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback =
      base::BindOnce(&WebHistoryService::ExpireHistoryCompletionCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  std::unique_ptr<Request> request(CreateRequest(
      url, std::move(completion_callback), partial_traffic_annotation));
  request->SetPostData(post_data);
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

void WebHistoryService::GetAudioHistoryEnabled(
    AudioWebHistoryCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback =
      base::BindOnce(&WebHistoryService::AudioHistoryCompletionCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  GURL url(kHistoryAudioHistoryUrl);

  std::unique_ptr<Request> request(CreateRequest(
      url, std::move(completion_callback), partial_traffic_annotation));
  request->Start();
  Request* request_ptr = request.get();
  pending_audio_history_requests_[request_ptr] = std::move(request);
}

void WebHistoryService::SetAudioHistoryEnabled(
    bool new_enabled_value,
    AudioWebHistoryCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback =
      base::BindOnce(&WebHistoryService::AudioHistoryCompletionCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  GURL url(kHistoryAudioHistoryChangeUrl);
  std::unique_ptr<Request> request(CreateRequest(
      url, std::move(completion_callback), partial_traffic_annotation));

  base::Value::Dict enable_audio_history =
      base::Value::Dict()
          .Set("enable_history_recording", new_enabled_value)
          .Set("client", "audio");
  std::string post_data;
  base::JSONWriter::Write(enable_audio_history, &post_data);
  request->SetPostData(post_data);

  request->Start();
  Request* request_ptr = request.get();
  pending_audio_history_requests_[request_ptr] = std::move(request);
}

size_t WebHistoryService::GetNumberOfPendingAudioHistoryRequests() {
  return pending_audio_history_requests_.size();
}

void WebHistoryService::QueryWebAndAppActivity(
    QueryWebAndAppActivityCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback = base::BindOnce(
      &WebHistoryService::QueryWebAndAppActivityCompletionCallback,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  GURL url(kQueryWebAndAppActivityUrl);
  Request* request = CreateRequest(url, std::move(completion_callback),
                                   partial_traffic_annotation);
  pending_web_and_app_activity_requests_[request] = base::WrapUnique(request);
  request->Start();
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
      url.path() + kQueryOtherFormsOfBrowsingHistoryUrlSuffix;
  replace_path.SetPathStr(new_path);
  url = url.ReplaceComponents(replace_path);
  DCHECK(url.is_valid());

  Request* request = CreateRequest(url, std::move(completion_callback),
                                   partial_traffic_annotation);

  // Set the Sync-specific user agent.
  request->SetUserAgent(syncer::MakeUserAgentForSync(channel));

  pending_other_forms_of_browsing_history_requests_[request] =
      base::WrapUnique(request);

  // Set the request protobuf.
  sync_pb::HistoryStatusRequest request_proto;
  std::string post_data;
  request_proto.SerializeToString(&post_data);
  request->SetPostDataAndType(post_data, kSyncProtoMimeType);

  request->Start();
}

// static
void WebHistoryService::QueryHistoryCompletionCallback(
    WebHistoryService::QueryWebHistoryCallback callback,
    WebHistoryService::Request* request,
    bool success) {
  std::move(callback).Run(request,
                          success ? ReadResponse(request) : std::nullopt);
}

void WebHistoryService::ExpireHistoryCompletionCallback(
    WebHistoryService::ExpireWebHistoryCallback callback,
    WebHistoryService::Request* request,
    bool success) {
  std::unique_ptr<Request> request_ptr =
      std::move(pending_expire_requests_[request]);
  pending_expire_requests_.erase(request);

  if (!success) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::optional<base::Value::Dict> response = ReadResponse(request);
  if (!response) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (const auto* version = response->FindString("version_info")) {
    server_version_info_ = *version;
  }
  // Inform the observers about the history deletion.
  for (WebHistoryServiceObserver& observer : observer_list_) {
    observer.OnWebHistoryDeleted();
  }
  std::move(callback).Run(/*success=*/true);
}

void WebHistoryService::AudioHistoryCompletionCallback(
    WebHistoryService::AudioWebHistoryCallback callback,
    WebHistoryService::Request* request,
    bool success) {
  std::unique_ptr<Request> request_ptr =
      std::move(pending_audio_history_requests_[request]);
  pending_audio_history_requests_.erase(request);

  if (!success) {
    std::move(callback).Run(/*success=*/false, /*new_enabled_value=*/false);
    return;
  }

  if (std::optional<base::Value::Dict> response = ReadResponse(request)) {
    bool enabled_value = false;
    if (std::optional<bool> enabled =
            response->FindBool("history_recording_enabled")) {
      enabled_value = *enabled;
    }
    std::move(callback).Run(/*success=*/true, enabled_value);
    return;
  }

  // If there is no response_value, then for our purposes, the request has
  // failed, despite receiving a true `success` value. This can happen if
  // the user is offline.
  std::move(callback).Run(/*success=*/false, /*new_enabled_value=*/false);
}

void WebHistoryService::QueryWebAndAppActivityCompletionCallback(
    WebHistoryService::QueryWebAndAppActivityCallback callback,
    WebHistoryService::Request* request,
    bool success) {
  std::unique_ptr<Request> request_ptr =
      std::move(pending_web_and_app_activity_requests_[request]);
  pending_web_and_app_activity_requests_.erase(request);

  if (!success) {
    std::move(callback).Run(/*web_and_app_activity_enabled=*/false);
    return;
  }

  if (std::optional<base::Value::Dict> response = ReadResponse(request)) {
    if (std::optional<bool> enabled =
            response->FindBool("history_recording_enabled")) {
      std::move(callback).Run(
          /*web_and_app_activity_enabled=*/*enabled);
      return;
    }
  }

  std::move(callback).Run(/*web_and_app_activity_enabled=*/false);
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
