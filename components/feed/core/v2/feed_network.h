// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_NETWORK_H_
#define COMPONENTS_FEED_CORE_V2_FEED_NETWORK_H_

#include <memory>
#include <string_view>

#include "base/functional/callback.h"
#include "components/feed/core/proto/v2/wire/consistency_token.pb.h"
#include "components/feed/core/proto/v2/wire/feed_query.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/proto/v2/wire/upload_actions_request.pb.h"
#include "components/feed/core/proto/v2/wire/upload_actions_response.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/types.h"
#include "net/http/http_request_headers.h"

namespace feedwire {
class Request;
class Response;
}  // namespace feedwire

namespace feed {
struct AccountInfo;

// DiscoverApi types. Defines information about each discover API. For use with
// `FeedNetwork::SendApiRequest()`.
// Some APIs do not send request metadata because it is already included in the
// `feedwire::Request` proto and is therefore redundant.

struct QueryInteractiveFeedDiscoverApi {
  using Request = feedwire::Request;
  using Response = feedwire::Response;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kQueryInteractiveFeed;
  static std::string_view Method() { return "POST"; }
  static std::string_view RequestPath(const Request&) {
    return "v1:queryInteractiveFeed";
  }
  static bool SendRequestMetadata() { return false; }
};

struct QueryBackgroundFeedDiscoverApi {
  using Request = feedwire::Request;
  using Response = feedwire::Response;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kQueryBackgroundFeed;
  static std::string_view Method() { return "POST"; }
  static std::string_view RequestPath(const Request&) {
    return "v1:queryBackgroundFeed";
  }
  static bool SendRequestMetadata() { return false; }
};

struct QueryNextPageDiscoverApi {
  using Request = feedwire::Request;
  using Response = feedwire::Response;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kQueryNextPage;
  static std::string_view Method() { return "POST"; }
  static std::string_view RequestPath(const Request&) {
    return "v1:queryNextPage";
  }
  static bool SendRequestMetadata() { return false; }
};

struct UploadActionsDiscoverApi {
  using Request = feedwire::UploadActionsRequest;
  using Response = feedwire::UploadActionsResponse;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kUploadActions;
  static std::string_view Method() { return "POST"; }
  static std::string_view RequestPath(const Request&) {
    return "v1/actions:upload";
  }
  static bool SendRequestMetadata() { return true; }
};

struct ListWebFeedsDiscoverApi {
  using Request = feedwire::webfeed::ListWebFeedsRequest;
  using Response = feedwire::webfeed::ListWebFeedsResponse;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kListWebFeeds;
  static std::string_view Method() { return "POST"; }
  static std::string_view RequestPath(const Request&) { return "v1/webFeeds"; }
  static bool SendRequestMetadata() { return true; }
};

struct ListRecommendedWebFeedDiscoverApi {
  using Request = feedwire::webfeed::ListRecommendedWebFeedsRequest;
  using Response = feedwire::webfeed::ListRecommendedWebFeedsResponse;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kListRecommendedWebFeeds;
  static std::string_view Method() { return "POST"; }
  static std::string_view RequestPath(const Request&) {
    return "v1/recommendedWebFeeds";
  }
  static bool SendRequestMetadata() { return true; }
};

struct FollowWebFeedDiscoverApi {
  using Request = feedwire::webfeed::FollowWebFeedRequest;
  using Response = feedwire::webfeed::FollowWebFeedResponse;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kFollowWebFeed;
  static std::string_view Method() { return "POST"; }
  static std::string_view RequestPath(const Request&) {
    return "v1:followWebFeed";
  }
  static bool SendRequestMetadata() { return true; }
};

struct UnfollowWebFeedDiscoverApi {
  using Request = feedwire::webfeed::UnfollowWebFeedRequest;
  using Response = feedwire::webfeed::UnfollowWebFeedResponse;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kUnfollowWebFeed;
  static std::string_view Method() { return "POST"; }
  static std::string_view RequestPath(const Request&) {
    return "v1:unfollowWebFeed";
  }
  static bool SendRequestMetadata() { return true; }
};

struct WebFeedListContentsDiscoverApi {
  using Request = feedwire::Request;
  using Response = feedwire::Response;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kWebFeedListContents;
  static std::string_view Method() { return "POST"; }
  static std::string_view RequestPath(const Request&) { return "v1/contents"; }
  static bool SendRequestMetadata() { return false; }
};

struct SingleWebFeedListContentsDiscoverApi {
  using Request = feedwire::Request;
  using Response = feedwire::Response;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kSingleWebFeedListContents;
  static std::string_view Method() { return "POST"; }
  static std::string_view RequestPath(const Request&) { return "v1/contents"; }
  static bool SendRequestMetadata() { return false; }
};

struct QueryWebFeedDiscoverApi {
  using Request = feedwire::webfeed::QueryWebFeedRequest;
  using Response = feedwire::webfeed::QueryWebFeedResponse;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kQueryWebFeed;
  static std::string_view Method() { return "POST"; }
  static std::string_view RequestPath(const Request&) {
    return "v1:queryWebFeed";
  }
  static bool SendRequestMetadata() { return true; }
};

class FeedNetwork {
 public:
  struct RawResponse {
    // HTTP response body.
    std::string response_bytes;
    NetworkResponseInfo response_info;
  };
  // Result of SendQueryRequest.
  struct QueryRequestResult {
    QueryRequestResult();
    ~QueryRequestResult();
    QueryRequestResult(QueryRequestResult&&);
    QueryRequestResult& operator=(QueryRequestResult&&);
    NetworkResponseInfo response_info;
    // Response body if one was received.
    std::unique_ptr<feedwire::Response> response_body;
    // Whether the request was signed in.
    bool was_signed_in;
  };

  template <typename RESPONSE_MESSAGE>
  struct ApiResult {
    ApiResult() = default;
    ~ApiResult() = default;
    ApiResult(ApiResult&&) = default;
    ApiResult& operator=(ApiResult&&) = default;

    NetworkResponseInfo response_info;
    // Response body if one was received.
    std::unique_ptr<RESPONSE_MESSAGE> response_body;
  };

  virtual ~FeedNetwork();

  // Send functions. These send a request and call the callback with the result.
  // If |CancelRequests()| is called, the result callback may never be called.

  // Send a feedwire::Request, and receive the response in |callback|.
  virtual void SendQueryRequest(
      NetworkRequestType request_type,
      const feedwire::Request& request,
      const AccountInfo& account_info,
      base::OnceCallback<void(QueryRequestResult)> callback) = 0;

  // Send a Discover API request. Usage:
  // SendApiRequest<UploadActionsDiscoverApi>(request_message, callback).
  template <typename API>
  void SendApiRequest(
      const typename API::Request& request,
      const AccountInfo& account_info,
      RequestMetadata request_metadata,
      base::OnceCallback<void(ApiResult<typename API::Response>)> callback) {
    std::string binary_proto;
    request.SerializeToString(&binary_proto);
    std::optional<RequestMetadata> optional_request_metadata;
    if (API::SendRequestMetadata()) {
      optional_request_metadata =
          std::make_optional(std::move(request_metadata));
    }

    SendDiscoverApiRequest(
        API::kRequestType, API::RequestPath(request), API::Method(),
        std::move(binary_proto), account_info,
        std::move(optional_request_metadata),
        base::BindOnce(&ParseAndForwardApiResponse<API>, std::move(callback)));
  }

  virtual void SendAsyncDataRequest(
      const GURL& url,
      std::string_view request_method,
      net::HttpRequestHeaders request_headers,
      std::string request_body,
      const AccountInfo& account_info,
      base::OnceCallback<void(RawResponse)> callback) = 0;

  // Cancels all pending requests immediately. This could be used, for example,
  // if there are pending requests for a user who just signed out.
  virtual void CancelRequests() = 0;

 protected:
  static void ParseAndForwardApiResponseStarted(
      NetworkRequestType request_type,
      const RawResponse& raw_response);
  virtual void SendDiscoverApiRequest(
      NetworkRequestType request_type,
      std::string_view api_path,
      std::string_view method,
      std::string request_bytes,
      const AccountInfo& account_info,
      std::optional<RequestMetadata> request_metadata,
      base::OnceCallback<void(RawResponse)> callback) = 0;

  template <typename API>
  static void ParseAndForwardApiResponse(
      base::OnceCallback<void(FeedNetwork::ApiResult<typename API::Response>)>
          result_callback,
      RawResponse raw_response) {
    ParseAndForwardApiResponseStarted(API::kRequestType, raw_response);
    FeedNetwork::ApiResult<typename API::Response> result;
    result.response_info = raw_response.response_info;
    if (result.response_info.status_code == 200) {
      auto response_message = std::make_unique<typename API::Response>();
      if (response_message->ParseFromString(raw_response.response_bytes)) {
        result.response_body = std::move(response_message);
      }
    }
    std::move(result_callback).Run(std::move(result));
  }
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_FEED_NETWORK_H_
