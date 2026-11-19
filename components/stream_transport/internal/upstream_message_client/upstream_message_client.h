// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_INTERNAL_UPSTREAM_MESSAGE_CLIENT_UPSTREAM_MESSAGE_CLIENT_H_
#define COMPONENTS_STREAM_TRANSPORT_INTERNAL_UPSTREAM_MESSAGE_CLIENT_UPSTREAM_MESSAGE_CLIENT_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/stream_transport/internal/metrics_utils.h"
#include "components/stream_transport/public/common.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"
#include "url/gurl.h"

class GoogleServiceAuthError;

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

namespace browser_actuator {
class ActuatorUpstreamMessage;
}  // namespace browser_actuator

namespace stream_transport {

// Encapsulates the network logic to send upstream actuator messages to
// OnePlatform via authenticated HTTPS POST requests (`SimpleURLLoader`).
class UpstreamMessageClient {
 public:
  // Invoked with the HTTP response status code (e.g. 200, 204, 409), or -1
  // if rejected before dispatch or no HTTP response was received.
  using SendCompleteCallback = base::OnceCallback<void(int http_status_code)>;

  UpstreamMessageClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      GURL endpoint,
      net::NetworkTrafficAnnotationTag traffic_annotation);
  virtual ~UpstreamMessageClient();

  UpstreamMessageClient(const UpstreamMessageClient&) = delete;
  UpstreamMessageClient& operator=(const UpstreamMessageClient&) = delete;

  // Assembles an `ActuatorUpstreamMessage` envelope for `message`, initiates
  // the HTTPS POST to the server, and returns the assembled envelope. If the
  // request is rejected before network dispatch (e.g. untrustworthy endpoint or
  // pending-request cap reached), `callback` is invoked asynchronously with
  // -1 and the assembled envelope is still returned.
  virtual browser_actuator::ActuatorUpstreamMessage SendUpstreamMessage(
      std::string_view session_id,
      int64_t client_sequence_number,
      std::optional<int64_t> responding_to_sequence_number,
      PayloadType payload_type,
      const google::protobuf::MessageLite& message,
      SendCompleteCallback callback);

  // Cancels all in-flight upstream requests without invoking their completion
  // callbacks. Used when the primary account signs out or changes.
  void CancelPendingRequests();

  size_t GetPendingRequestsCountForTesting() const;

 private:
  struct PendingRequest;

  void StartAccessTokenFetch(PendingRequest* pending);
  void OnAccessTokenFetched(PendingRequest* pending,
                            GoogleServiceAuthError error,
                            signin::AccessTokenInfo access_token_info);
  void OnSimpleLoaderComplete(PendingRequest* pending,
                              scoped_refptr<net::HttpResponseHeaders> headers);
  void CompleteRequest(PendingRequest* pending,
                       std::optional<int> http_status_code);

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const GURL endpoint_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  base::flat_map<PendingRequest*, std::unique_ptr<PendingRequest>>
      pending_requests_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<UpstreamMessageClient> weak_ptr_factory_{this};
};

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_INTERNAL_UPSTREAM_MESSAGE_CLIENT_UPSTREAM_MESSAGE_CLIENT_H_
