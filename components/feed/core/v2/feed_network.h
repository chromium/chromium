// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_NETWORK_H_
#define COMPONENTS_FEED_CORE_V2_FEED_NETWORK_H_

#include <memory>

#include "base/callback.h"
#include "components/feed/core/proto/v2/wire/consistency_token.pb.h"
#include "components/feed/core/proto/v2/wire/feed_query.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/proto/v2/wire/upload_actions_request.pb.h"
#include "components/feed/core/proto/v2/wire/upload_actions_response.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/public/types.h"

namespace feedwire {
class Request;
class Response;
}  // namespace feedwire

namespace feed {

// DiscoverApi types. Defines information about each discover API. For use with
// `FeedNetwork::SendApiRequest()`.

struct QueryInteractiveFeedDiscoverApi {
  using Request = feedwire::Request;
  using Response = feedwire::Response;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kQueryInteractiveFeed;
  static base::StringPiece Method() { return "POST"; }
  static base::StringPiece RequestPath(const Request&) {
    return "v1:queryInteractiveFeed";
  }
};

struct QueryBackgroundFeedDiscoverApi {
  using Request = feedwire::Request;
  using Response = feedwire::Response;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kQueryBackgroundFeed;
  static base::StringPiece Method() { return "POST"; }
  static base::StringPiece RequestPath(const Request&) {
    return "v1:queryBackgroundFeed";
  }
};

struct QueryNextPageDiscoverApi {
  using Request = feedwire::Request;
  using Response = feedwire::Response;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kQueryNextPage;
  static base::StringPiece Method() { return "POST"; }
  static base::StringPiece RequestPath(const Request&) {
    return "v1:queryNextPage";
  }
};

struct UploadActionsDiscoverApi {
  using Request = feedwire::UploadActionsRequest;
  using Response = feedwire::UploadActionsResponse;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kUploadActions;
  static base::StringPiece Method() { return "POST"; }
  static base::StringPiece RequestPath(const Request&) {
    return "v1/actions:upload";
  }
};

struct ListWebFeedsDiscoverApi {
  using Request = feedwire::webfeed::ListWebFeedsRequest;
  using Response = feedwire::webfeed::ListWebFeedsResponse;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kListWebFeeds;
  static base::StringPiece Method() { return "POST"; }
  static base::StringPiece RequestPath(const Request&) { return "v1/webFeeds"; }
};

struct ListRecommendedWebFeedDiscoverApi {
  using Request = feedwire::webfeed::ListRecommendedWebFeedsRequest;
  using Response = feedwire::webfeed::ListRecommendedWebFeedsResponse;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kListRecommendedWebFeeds;
  static base::StringPiece Method() { return "POST"; }
  static base::StringPiece RequestPath(const Request&) {
    return "v1/recommendedWebFeeds";
  }
};

struct FollowWebFeedDiscoverApi {
  using Request = feedwire::webfeed::FollowWebFeedRequest;
  using Response = feedwire::webfeed::FollowWebFeedResponse;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kFollowWebFeed;
  static base::StringPiece Method() { return "POST"; }
  static base::StringPiece RequestPath(const Request&) {
    return "v1:followWebFeed";
  }
};

struct UnfollowWebFeedDiscoverApi {
  using Request = feedwire::webfeed::UnfollowWebFeedRequest;
  using Response = feedwire::webfeed::UnfollowWebFeedResponse;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kUnfollowWebFeed;
  static base::StringPiece Method() { return "POST"; }
  static base::StringPiece RequestPath(const Request&) {
    return "v1:unfollowWebFeed";
  }
};

struct WebFeedListContentsDiscoverApi {
  using Request = feedwire::Request;
  using Response = feedwire::Response;
  static constexpr NetworkRequestType kRequestType =
      NetworkRequestType::kWebFeedListContents;
  static base::StringPiece Method() { return "POST"; }
  static base::StringPiece RequestPath(const Request&) { return "v1/contents"; }
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
      const std::string& gaia,
      base::OnceCallback<void(QueryRequestResult)> callback) = 0;

  // Send a Discover API request. Usage:
  // SendApiRequest<UploadActionsDiscoverApi>(request_message, callback).
  template <typename API>
  void SendApiRequest(
      const typename API::Request& request,
      const std::string& gaia,
      base::OnceCallback<void(ApiResult<typename API::Response>)> callback) {
    std::string binary_proto;
    request.SerializeToString(&binary_proto);
    SendDiscoverApiRequest(
        API::kRequestType, API::RequestPath(request), API::Method(),
        std::move(binary_proto), gaia,
        base::BindOnce(&ParseAndForwardApiResponse<API>, std::move(callback)));
  }

  // Cancels all pending requests immediately. This could be used, for example,
  // if there are pending requests for a user who just signed out.
  virtual void CancelRequests() = 0;

 protected:
  static void ParseAndForwardApiResponseStarted(
      NetworkRequestType request_type,
      const RawResponse& raw_response);
  virtual void SendDiscoverApiRequest(
      NetworkRequestType request_type,
      base::StringPiece api_path,
      base::StringPiece method,
      std::string request_bytes,
      const std::string& gaia,
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
