// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_UPSTREAM_MESSAGE_CLIENT_UPSTREAM_MESSAGE_CLIENT_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_UPSTREAM_MESSAGE_CLIENT_UPSTREAM_MESSAGE_CLIENT_H_

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
#include "components/browser_actuator/public/common.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace endpoint_fetcher {
class EndpointFetcher;
struct EndpointResponse;
}  // namespace endpoint_fetcher

namespace browser_actuator {

// Encapsulates the network logic to send upstream actuator messages to
// OnePlatform via standard HTTPS POST requests (EndpointFetcher).
class UpstreamMessageClient {
 public:
  using SendCompleteCallback =
      base::OnceCallback<void(bool success, int response_code)>;

  UpstreamMessageClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      GURL endpoint,
      net::NetworkTrafficAnnotationTag traffic_annotation);
  virtual ~UpstreamMessageClient();

  UpstreamMessageClient(const UpstreamMessageClient&) = delete;
  UpstreamMessageClient& operator=(const UpstreamMessageClient&) = delete;

  // Sends an upstream message payload to the server.
  virtual void SendUpstreamMessage(
      std::string_view session_id,
      int64_t client_sequence_number,
      std::optional<int64_t> responding_to_sequence_number,
      PayloadType payload_type,
      const google::protobuf::MessageLite& message,
      SendCompleteCallback callback);

 private:
  void OnMessageSent(
      endpoint_fetcher::EndpointFetcher* fetcher,
      SendCompleteCallback callback,
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const GURL endpoint_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  base::flat_map<endpoint_fetcher::EndpointFetcher*,
                 std::unique_ptr<endpoint_fetcher::EndpointFetcher>>
      pending_requests_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<UpstreamMessageClient> weak_ptr_factory_{this};
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_UPSTREAM_MESSAGE_CLIENT_UPSTREAM_MESSAGE_CLIENT_H_
