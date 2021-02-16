// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_NETWORK_H_
#define COMPONENTS_FEED_CORE_V2_FEED_NETWORK_H_

#include <memory>

#include "base/callback.h"
#include "components/feed/core/proto/v2/wire/discover_actions_service.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/public/types.h"

namespace feedwire {
class Request;
class Response;
}  // namespace feedwire

namespace feed {

// DiscoverApi types. Defines information about each discover API. For use with
// `FeedNetwork::SendApiRequest()`.

struct UploadActionsDiscoverApi {
  using Request = feedwire::UploadActionsRequest;
  using Response = feedwire::UploadActionsResponse;
  static const NetworkRequestType kRequestType =
      NetworkRequestType::kUploadActions;
  static base::StringPiece Method() { return "POST"; }
  static base::StringPiece RequestPath() { return "v1/actions:upload"; }
};

struct ListFollowedWebFeedDiscoverApi {
  using Request = feedwire::ListFollowedWebFeedRequest;
  using Response = feedwire::ListFollowedWebFeedResponse;
  static const NetworkRequestType kRequestType =
      NetworkRequestType::kListFollowedWebFeeds;
  static base::StringPiece Method() { return "GET"; }
  // TODO(harringtond): Path TDB.
  static base::StringPiece RequestPath() { return "v1/follow:listFollowed"; }
};

struct UnfollowWebFeedDiscoverApi {
  using Request = feedwire::UnfollowWebFeedRequest;
  using Response = feedwire::UnfollowWebFeedResponse;
  static const NetworkRequestType kRequestType =
      NetworkRequestType::kUnfollowWebFeed;
  static base::StringPiece Method() { return "POST"; }
  // TODO(harringtond): Path TDB.
  static base::StringPiece RequestPath() { return "v1/follow:unfollowWebFeed"; }
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
      bool force_signed_out_request,
      base::OnceCallback<void(QueryRequestResult)> callback) = 0;

  // Send a Discover API request. Usage:
  // SendApiRequest<UploadActionsDiscoverApi>(request_message, callback).
  template <typename API>
  void SendApiRequest(
      const typename API::Request& request,
      base::OnceCallback<void(ApiResult<typename API::Response>)> callback) {
    std::string binary_proto;
    request.SerializeToString(&binary_proto);
    SendDiscoverApiRequest(
        API::RequestPath(), API::Method(), std::move(binary_proto),
        base::BindOnce(&ParseAndForwardApiResponse<API>, std::move(callback)));
  }

  // Cancels all pending requests immediately. This could be used, for example,
  // if there are pending requests for a user who just signed out.
  virtual void CancelRequests() = 0;

 protected:
  virtual void SendDiscoverApiRequest(
      base::StringPiece api_path,
      base::StringPiece method,
      std::string request_bytes,
      base::OnceCallback<void(RawResponse)> callback) = 0;

  template <typename API>
  static void ParseAndForwardApiResponse(
      base::OnceCallback<void(FeedNetwork::ApiResult<typename API::Response>)>
          result_callback,
      RawResponse raw_response) {
    MetricsReporter::NetworkRequestComplete(
        API::kRequestType, raw_response.response_info.status_code);
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
