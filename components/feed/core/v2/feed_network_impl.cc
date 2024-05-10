// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_network_impl.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/wire/feed_query.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/proto/v2/wire/upload_actions_request.pb.h"
#include "components/feed/core/proto/v2/wire/upload_actions_response.pb.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/variations/net/variations_http_headers.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/zlib/google/compression_utils.h"

namespace feed {
namespace {
constexpr char kApplicationXProtobuf[] = "application/x-protobuf";
constexpr base::TimeDelta kNetworkTimeout = base::Seconds(30);
constexpr char kDiscoverHost[] = "https://discover-pa.googleapis.com/";

signin::ScopeSet GetAuthScopes() {
  return {GaiaConstants::kFeedOAuth2Scope};
}

int EstimateFeedQueryRequestSize(const network::ResourceRequest& request) {
  int total_size = 14 +  // GET <path> HTTP/1.1
                   request.url.path_piece().size() +
                   request.url.query_piece().size();
  for (const net::HttpRequestHeaders::HeaderKeyValuePair& header :
       request.headers.GetHeaderVector()) {
    total_size += header.key.size() + header.value.size() + 2;
  }
  for (const net::HttpRequestHeaders::HeaderKeyValuePair& header :
       request.cors_exempt_headers.GetHeaderVector()) {
    total_size += header.key.size() + header.value.size() + 2;
  }

  return total_size;
}

GURL GetFeedQueryURL(feedwire::FeedQuery::RequestReason reason) {
  // Add URLs for Bling when it is supported.
  switch (reason) {
    case feedwire::FeedQuery::SCHEDULED_REFRESH:
    case feedwire::FeedQuery::PREFETCHED_WEB_FEED:
    case feedwire::FeedQuery::APP_CLOSE_REFRESH:
      return GURL(
          "https://www.google.com/httpservice/noretry/TrellisClankService/"
          "FeedQuery");
    case feedwire::FeedQuery::NEXT_PAGE_SCROLL:
      return GURL(
          "https://www.google.com/httpservice/retry/TrellisClankService/"
          "NextPageQuery");
    case feedwire::FeedQuery::MANUAL_REFRESH:
    case feedwire::FeedQuery::INTERACTIVE_WEB_FEED:
      return GURL(
          "https://www.google.com/httpservice/retry/TrellisClankService/"
          "FeedQuery");
    case feedwire::FeedQuery::UNKNOWN_REQUEST_REASON:
      return GURL();
  }
}

GURL GetUrlWithoutQuery(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  return url.ReplaceComponents(replacements);
}

using RawResponse = FeedNetwork::RawResponse;

net::HttpRequestHeaders CreateApiRequestHeaders(
    const RequestMetadata& request_metadata) {
  const std::string encoded_client_info = base::Base64Encode(
      CreateClientInfo(request_metadata).SerializeAsString());
  net::HttpRequestHeaders headers;
  headers.SetHeader(kClientInfoHeader, encoded_client_info);
  return headers;
}

}  // namespace

namespace {

void ParseAndForwardQueryResponse(
    NetworkRequestType request_type,
    base::OnceCallback<void(FeedNetwork::QueryRequestResult)> result_callback,
    RawResponse raw_response) {
  MetricsReporter::NetworkRequestComplete(request_type,
                                          raw_response.response_info);
  FeedNetwork::QueryRequestResult result;
  result.response_info = raw_response.response_info;
  result.response_info.fetch_time_ticks = base::TimeTicks::Now();
  if (result.response_info.status_code == 200) {
    ::google::protobuf::io::CodedInputStream input_stream(
        reinterpret_cast<const uint8_t*>(raw_response.response_bytes.data()),
        raw_response.response_bytes.size());

    // The first few bytes of the body are a varint containing the size of the
    // message. We need to skip over them.
    int message_size;
    input_stream.ReadVarintSizeAsInt(&message_size);

    auto response_message = std::make_unique<feedwire::Response>();
    if (response_message->ParseFromCodedStream(&input_stream)) {
      result.response_body = std::move(response_message);
    }
  }
  std::move(result_callback).Run(std::move(result));
}

void AddMothershipPayloadQueryParams(const std::string& payload,
                                     const std::string& language_tag,
                                     GURL& url) {
  url = net::AppendQueryParameter(url, "reqpld", payload);
  url = net::AppendQueryParameter(url, "fmt", "bin");
  if (!language_tag.empty())
    url = net::AppendQueryParameter(url, "hl", language_tag);
}

// Compresses and attaches |request_body| for upload if it's not empty.
// Returns the compressed size of the request.
int PopulateRequestBody(const std::string& request_body,
                        network::SimpleURLLoader* loader) {
  if (request_body.empty())
    return 0;
  std::string compressed_request_body;
  compression::GzipCompress(request_body, &compressed_request_body);
  loader->AttachStringForUpload(compressed_request_body, kApplicationXProtobuf);
  return compressed_request_body.size();
}

GURL OverrideUrlSchemeHostPort(const GURL& url,
                               const GURL& override_scheme_host_port) {
  GURL::Replacements replacements;
  replacements.SetSchemeStr(override_scheme_host_port.scheme_piece());
  replacements.SetHostStr(override_scheme_host_port.host_piece());
  replacements.SetPortStr(override_scheme_host_port.port_piece());
  return url.ReplaceComponents(replacements);
}

}  // namespace

// Each NetworkFetch instance represents a single "logical" fetch that ends by
// calling the associated callback. Network fetches will actually attempt two
// fetches if there is a signed in user; the first to retrieve an access token,
// and the second to the specified url.
class FeedNetworkImpl::NetworkFetch {
 public:
  NetworkFetch(const GURL& url,
               std::string_view request_method,
               std::string request_body,
               FeedNetworkImpl::Delegate* delegate,
               signin::IdentityManager* identity_manager,
               network::SharedURLLoaderFactory* loader_factory,
               const std::string& api_key,
               const AccountInfo& account_info,
               net::HttpRequestHeaders headers,
               bool is_feed_query,
               bool allow_bless_auth)
      : url_(url),
        request_method_(request_method),
        request_body_(std::move(request_body)),
        delegate_(delegate),
        identity_manager_(identity_manager),
        loader_factory_(loader_factory),
        api_key_(api_key),
        entire_send_start_ticks_(base::TimeTicks::Now()),
        account_info_(account_info),
        headers_(std::move(headers)),
        is_feed_query_(is_feed_query),
        allow_bless_auth_(allow_bless_auth) {}
  ~NetworkFetch() = default;
  NetworkFetch(const NetworkFetch&) = delete;
  NetworkFetch& operator=(const NetworkFetch&) = delete;

  void Start(base::OnceCallback<void(RawResponse)> done_callback) {
    done_callback_ = std::move(done_callback);

    if (delegate_->IsOffline()) {
      std::move(done_callback_)
          .Run(MakeFailureResponse(net::ERR_INTERNET_DISCONNECTED,
                                   AccountTokenFetchStatus::kUnspecified));
      return;
    }
    if (account_info_.IsEmpty()) {
      StartLoader();
      return;
    }

    StartAccessTokenFetch();
  }

 private:
  void StartAccessTokenFetch() {
    DVLOG(1) << "Feed access token fetch started.";
    token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
        "feed", identity_manager_, GetAuthScopes(),
        base::BindOnce(&NetworkFetch::AccessTokenFetchFinished, GetWeakPtr(),
                       base::TimeTicks::Now()),
        signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
        GetConsentLevelNeededForPersonalizedFeed());
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&NetworkFetch::AccessTokenTimeout, GetWeakPtr()),
        kAccessTokenFetchTimeout);
  }

  static RawResponse MakeFailureResponse(
      int32_t status_code,
      AccountTokenFetchStatus token_fetch_status) {
    NetworkResponseInfo response_info;
    RawResponse raw_response;
    response_info.status_code = status_code;
    response_info.account_token_fetch_status = token_fetch_status;
    raw_response.response_info = std::move(response_info);
    return raw_response;
  }

  void AccessTokenTimeout() {
    if (access_token_fetch_complete_)
      return;
    DVLOG(1) << "Feed access token fetch timed out.";
    access_token_fetch_complete_ = true;
    std::move(done_callback_)
        .Run(MakeFailureResponse(net::ERR_TIMED_OUT,
                                 AccountTokenFetchStatus::kTimedOut));
  }

  void AccessTokenFetchFinished(base::TimeTicks token_start_ticks,
                                GoogleServiceAuthError error,
                                signin::AccessTokenInfo access_token_info) {
    DCHECK(!account_info_.IsEmpty());
    if (access_token_fetch_complete_)
      return;
    DVLOG(1) << "Feed access token fetch complete.";
    access_token_fetch_complete_ = true;
    UMA_HISTOGRAM_ENUMERATION(
        "ContentSuggestions.Feed.Network.TokenFetchStatus", error.state(),
        GoogleServiceAuthError::NUM_STATES);

    base::TimeDelta token_duration = base::TimeTicks::Now() - token_start_ticks;
    UMA_HISTOGRAM_MEDIUM_TIMES("ContentSuggestions.Feed.Network.TokenDuration",
                               token_duration);

    access_token_ = access_token_info.token;

    // Abort if the signed-in user doesn't match.
    if (delegate_->GetAccountInfo() != account_info_) {
      DVLOG(1) << "Feed fetch failed due to account mismatch.";
      std::move(done_callback_)
          .Run(
              MakeFailureResponse(net::ERR_INVALID_ARGUMENT,
                                  AccountTokenFetchStatus::kUnexpectedAccount));
      return;
    }

    StartLoader();
  }

  void StartLoader() {
    DVLOG(1) << "Feed fetch started.";
    loader_only_start_ticks_ = base::TimeTicks::Now();
    simple_loader_ = MakeLoader();
    simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        loader_factory_, base::BindOnce(&NetworkFetch::OnSimpleLoaderComplete,
                                        base::Unretained(this)));
  }

  std::unique_ptr<network::SimpleURLLoader> MakeLoader() {
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("interest_feedv2_send", R"(
      semantics {
        sender: "Feed Library"
        description: "Chrome can show content suggestions (e.g. articles) "
          "in the form of a feed. For signed-in users, these may be "
          "personalized based on interest signals in the user's account."
        trigger: "Triggered periodically in the background, or upon "
          "explicit user request."
        data: "The locale of the device and data describing the suggested "
          "content that the user interacted with. For signed-in users "
          "the request is authenticated. "
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "This can be disabled from the New Tab Page by collapsing "
        "the articles section."
        chrome_policy {
          NTPContentSuggestionsEnabled {
            policy_options {mode: MANDATORY}
            NTPContentSuggestionsEnabled: false
          }
        }
      })");

    GURL url(url_);
    if (access_token_.empty() && !api_key_.empty())
      url = net::AppendQueryParameter(url_, "key", api_key_);

    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url;

    resource_request->load_flags = net::LOAD_BYPASS_CACHE;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    resource_request->method = request_method_;

    if (allow_bless_auth_) {
      // Include credentials ONLY if the user has overridden the feed host
      // through the internals page. This allows for some authentication
      // workflows we need for testing.
      resource_request->credentials_mode =
          network::mojom::CredentialsMode::kInclude;
      resource_request->site_for_cookies = net::SiteForCookies::FromUrl(url);
    } else {
      // Otherwise, isolate feed traffic from other requests the browser might
      // be making. This prevents the browser from reusing network connections
      // which may not match the signed-in/out status of the feed.
      resource_request->trusted_params =
          network::ResourceRequest::TrustedParams();
      resource_request->trusted_params->isolation_info =
          net::IsolationInfo::CreateTransient();
    }

    SetRequestHeaders(!request_body_.empty(), *resource_request);

    DVLOG(1) << "Feed Request url=" << url;
    DVLOG(1) << "Feed Request headers=" << resource_request->headers.ToString();

    if (is_feed_query_) {
      base::UmaHistogramCustomCounts(
          "ContentSuggestions.Feed.Network.FeedQueryRequestSize",
          EstimateFeedQueryRequestSize(*resource_request), 1000, 50000, 50);
    }

    auto simple_loader = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    simple_loader->SetAllowHttpErrorResults(true);
    simple_loader->SetTimeoutDuration(kNetworkTimeout);

    const int compressed_size =
        PopulateRequestBody(request_body_, simple_loader.get());
    UMA_HISTOGRAM_COUNTS_1M(
        "ContentSuggestions.Feed.Network.RequestSizeKB.Compressed",
        compressed_size / 1024);
    return simple_loader;
  }

  void SetRequestHeaders(bool has_request_body,
                         network::ResourceRequest& request) const {
    // content-type in the request header affects server response, so include it
    // even if there's no body.
    request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                              kApplicationXProtobuf);
    if (has_request_body) {
      request.headers.SetHeader("Content-Encoding", "gzip");
    }

    request.headers.MergeFrom(headers_);

    variations::SignedIn signed_in_status = variations::SignedIn::kNo;
    if (!access_token_.empty()) {
      std::string_view token = access_token_;
      std::string token_override =
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              "feed-token-override");
      if (!token_override.empty()) {
        token = token_override;
      }
      request.headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                base::StrCat({"Bearer ", token}));
      signed_in_status = variations::SignedIn::kYes;
    }

    // Add X-Client-Data header with experiment IDs from field trials.
    variations::AppendVariationsHeader(url_, variations::InIncognito::kNo,
                                       signed_in_status, &request);
  }

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response) {
    const network::mojom::URLResponseHead* loader_response_info =
        simple_loader_->ResponseInfo();
    std::optional<network::URLLoaderCompletionStatus> completion_status =
        simple_loader_->CompletionStatus();

    NetworkResponseInfo response_info;
    response_info.status_code = simple_loader_->NetError();
    response_info.fetch_duration =
        base::TimeTicks::Now() - entire_send_start_ticks_;
    response_info.fetch_time = base::Time::Now();
    response_info.base_request_url = GetUrlWithoutQuery(url_);
    if (!access_token_.empty()) {
      response_info.account_info = account_info_;
    }
    response_info.loader_start_time_ticks = loader_only_start_ticks_;
    response_info.encoded_size_bytes =
        completion_status ? completion_status->encoded_data_length : 0;

    if (loader_response_info) {
      size_t iter = 0;
      std::string name;
      std::string value;
      while (loader_response_info->headers->EnumerateHeaderLines(&iter, &name,
                                                                 &value)) {
        // If overriding the feed host, try to grab the Bless nonce. This is
        // strictly informational, and only displayed in snippets-internals.
        if (allow_bless_auth_ && name == "www-authenticate" &&
            response_info.bless_nonce.empty()) {
          size_t pos = value.find("nonce=\"");
          if (pos != std::string::npos) {
            std::string nonce = value.substr(pos + 7, 16);
            if (nonce.size() == 16) {
              response_info.bless_nonce = nonce;
            }
          }
        }
        response_info.response_header_names_and_values.push_back(
            std::move(name));
        response_info.response_header_names_and_values.push_back(
            std::move(value));
      }
    }

    std::string response_body;
    if (response) {
      response_info.status_code =
          loader_response_info->headers->response_code();
      response_info.response_body_bytes = response->size();

      response_body = std::move(*response);

      if (response_info.status_code == net::HTTP_UNAUTHORIZED) {
        CoreAccountId account_id = identity_manager_->GetPrimaryAccountId(
            signin::ConsentLevel::kSignin);
        if (!account_id.empty()) {
          identity_manager_->RemoveAccessTokenFromCache(
              account_id, GetAuthScopes(), access_token_);
        }
      }
    }

    UMA_HISTOGRAM_MEDIUM_TIMES("ContentSuggestions.Feed.Network.Duration",
                               response_info.fetch_duration);

    base::TimeDelta loader_only_duration =
        base::TimeTicks::Now() - loader_only_start_ticks_;
    // This histogram purposefully matches name and bucket size used in
    // RemoteSuggestionsFetcherImpl.
    UMA_HISTOGRAM_TIMES("NewTabPage.Snippets.FetchTime", loader_only_duration);

    // The below is true even if there is a protocol error, so this will
    // record response size as long as the request completed.
    if (response_info.status_code >= 200) {
      UMA_HISTOGRAM_COUNTS_1M("ContentSuggestions.Feed.Network.ResponseSizeKB",
                              static_cast<int>(response_body.size() / 1024));
    }

    RawResponse raw_response;
    raw_response.response_info = std::move(response_info);
    raw_response.response_bytes = std::move(response_body);
    std::move(done_callback_).Run(std::move(raw_response));
  }
  base::WeakPtr<NetworkFetch> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  GURL url_;
  const std::string request_method_;
  std::string access_token_;
  bool access_token_fetch_complete_ = false;
  const std::string request_body_;
  raw_ptr<FeedNetworkImpl::Delegate> delegate_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
  base::OnceCallback<void(RawResponse)> done_callback_;
  raw_ptr<network::SharedURLLoaderFactory> loader_factory_;
  const std::string api_key_;

  // Set when the NetworkFetch is constructed, before token and article fetch.
  const base::TimeTicks entire_send_start_ticks_;

  const AccountInfo account_info_;
  const net::HttpRequestHeaders headers_;

  // Should be set right before the article fetch, and after the token fetch if
  // there is one.
  base::TimeTicks loader_only_start_ticks_;
  bool is_feed_query_ = false;
  bool allow_bless_auth_ = false;
  base::WeakPtrFactory<NetworkFetch> weak_ptr_factory_{this};
};

FeedNetworkImpl::FeedNetworkImpl(
    Delegate* delegate,
    signin::IdentityManager* identity_manager,
    const std::string& api_key,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    PrefService* pref_service)
    : delegate_(delegate),
      identity_manager_(identity_manager),
      api_key_(api_key),
      loader_factory_(loader_factory),
      pref_service_(pref_service) {}

FeedNetworkImpl::~FeedNetworkImpl() = default;

void FeedNetworkImpl::SendQueryRequest(
    NetworkRequestType request_type,
    const feedwire::Request& request,
    const AccountInfo& account_info,
    base::OnceCallback<void(QueryRequestResult)> callback) {
  std::string binary_proto;
  request.SerializeToString(&binary_proto);
  std::string base64proto;
  base::Base64UrlEncode(
      binary_proto, base::Base64UrlEncodePolicy::INCLUDE_PADDING, &base64proto);

  GURL url = GetFeedQueryURL(request.feed_request().feed_query().reason());
  if (url.is_empty())
    return std::move(callback).Run({});

  // Override url if requested from internals page.
  bool host_overridden = false;
  std::string host_override =
      pref_service_->GetString(feed::prefs::kHostOverrideHost);

  if (host_override.empty()) {
    host_override = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        "feedv2-host-override");
  }

  if (!host_override.empty()) {
    GURL override_host_url(host_override);
    if (override_host_url.is_valid()) {
      GURL::Replacements replacements;
      replacements.SetSchemeStr(override_host_url.scheme_piece());
      replacements.SetHostStr(override_host_url.host_piece());
      replacements.SetPortStr(override_host_url.port_piece());
      // Allow the host override to also add a prefix for the path. Ignore
      // trailing slashes if they are provided, as the path part of |url| will
      // always include "/".
      std::string_view trimmed_path_prefix = base::TrimString(
          override_host_url.path_piece(), "/", base::TRIM_TRAILING);
      std::string replacement_path =
          base::StrCat({trimmed_path_prefix, url.path_piece()});

      replacements.SetPathStr(replacement_path);

      url = url.ReplaceComponents(replacements);
      host_overridden = true;
    }
  }

  AddMothershipPayloadQueryParams(base64proto, delegate_->GetLanguageTag(),
                                  url);
  Send(url, "GET", /*request_body=*/{},
       /*allow_bless_auth=*/host_overridden, account_info,
       net::HttpRequestHeaders(),
       /*is_feed_query=*/true,
       base::BindOnce(&ParseAndForwardQueryResponse, request_type,
                      std::move(callback)));
}

void FeedNetworkImpl::CancelRequests() {
  pending_requests_.clear();
}

void FeedNetworkImpl::Send(const GURL& url,
                           std::string_view request_method,
                           std::string request_body,
                           bool allow_bless_auth,
                           const AccountInfo& account_info,
                           net::HttpRequestHeaders headers,
                           bool is_feed_query,
                           base::OnceCallback<void(RawResponse)> callback) {
  TRACE_EVENT_BEGIN("android.ui.jank", "FeedNetwork",
                    perfetto::Track::FromPointer(this), "url", url);
  auto fetch = std::make_unique<NetworkFetch>(
      url, request_method, std::move(request_body), delegate_,
      identity_manager_, loader_factory_.get(), api_key_, account_info,
      std::move(headers), is_feed_query, allow_bless_auth);
  NetworkFetch* fetch_unowned = fetch.get();
  pending_requests_.emplace(std::move(fetch));

  // It's safe to pass base::Unretained(this) since deleting the network fetch
  // will prevent the callback from being completed.
  fetch_unowned->Start(base::BindOnce(&FeedNetworkImpl::SendComplete,
                                      base::Unretained(this), fetch_unowned,
                                      std::move(callback)));
}

void FeedNetworkImpl::SendDiscoverApiRequest(
    NetworkRequestType request_type,
    std::string_view request_path,
    std::string_view method,
    std::string request_body,
    const AccountInfo& account_info,
    std::optional<RequestMetadata> request_metadata,
    base::OnceCallback<void(RawResponse)> callback) {
  GURL url =
      GetOverriddenUrl(GURL(base::StrCat({kDiscoverHost, request_path})));

  net::HttpRequestHeaders headers =
      request_metadata ? CreateApiRequestHeaders(*request_metadata)
                       : net::HttpRequestHeaders();

  // Set the x-response-encoding header to enable compression for DiscoFeed.
  headers.SetHeader("x-response-encoding", "gzip");

  Send(url, method, std::move(request_body),
       /*allow_bless_auth=*/false, account_info, std::move(headers),
       /*is_feed_query=*/false, std::move(callback));
}

void FeedNetworkImpl::SendAsyncDataRequest(
    const GURL& url,
    std::string_view request_method,
    net::HttpRequestHeaders request_headers,
    std::string request_body,
    const AccountInfo& account_info,
    base::OnceCallback<void(RawResponse)> callback) {
  GURL request_url = GetOverriddenUrl(url);
  Send(request_url, request_method, std::move(request_body),
       /*allow_bless_auth=*/false, account_info, request_headers,
       /*is_feed_query=*/false, std::move(callback));
}

void FeedNetworkImpl::SendComplete(
    NetworkFetch* fetch,
    base::OnceCallback<void(RawResponse)> callback,
    RawResponse raw_response) {
  DCHECK_EQ(1UL, pending_requests_.count(fetch));
  TRACE_EVENT_END("android.ui.jank", perfetto::Track::FromPointer(this),
                  "bytes", raw_response.response_info.response_body_bytes);
  pending_requests_.erase(fetch);

  std::move(callback).Run(std::move(raw_response));
}

GURL FeedNetworkImpl::GetOverriddenUrl(const GURL& url) const {
  // Override url if requested.
  std::string host_override =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "feedv2-discover-host-override");
  if (host_override.empty()) {
    host_override =
        pref_service_->GetString(feed::prefs::kDiscoverAPIEndpointOverride);
  }
  if (!host_override.empty()) {
    GURL override_url(host_override);
    if (override_url.is_valid()) {
      return OverrideUrlSchemeHostPort(url, override_url);
    }
  }
  return url;
}

}  // namespace feed
