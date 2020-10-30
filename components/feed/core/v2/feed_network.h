// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_NETWORK_H_
#define COMPONENTS_FEED_CORE_V2_FEED_NETWORK_H_

#include <memory>

#include "base/callback.h"
#include "components/feed/core/v2/public/types.h"

namespace feedwire {
class UploadActionsResponse;
class UploadActionsRequest;
class Request;
class Response;
}  // namespace feedwire

namespace feed {

class FeedNetwork {
 public:
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

  // Result of SendActionRequest.
  struct ActionRequestResult {
    ActionRequestResult();
    ~ActionRequestResult();
    ActionRequestResult(ActionRequestResult&&);
    ActionRequestResult& operator=(ActionRequestResult&&);
    NetworkResponseInfo response_info;
    // Response body if one was received.
    std::unique_ptr<feedwire::UploadActionsResponse> response_body;
  };

  virtual ~FeedNetwork();

  // Send a feedwire::Request, and receive the response in |callback|.
  // |callback| will be called unless the request is canceled with
  // |CancelRequests()|.
  virtual void SendQueryRequest(
      const feedwire::Request& request,
      bool force_signed_out_request,
      base::OnceCallback<void(QueryRequestResult)> callback) = 0;

  // Send a feedwire::UploadActionsRequest, and receive the response in
  // |callback|. |callback| will be called unless the request is canceled with
  // |CancelRequests()|.
  virtual void SendActionRequest(
      const feedwire::UploadActionsRequest& request,
      base::OnceCallback<void(ActionRequestResult)> callback) = 0;

  // Cancels all pending requests immediately. This could be used, for example,
  // if there are pending requests for a user who just signed out.
  virtual void CancelRequests() = 0;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_FEED_NETWORK_H_
