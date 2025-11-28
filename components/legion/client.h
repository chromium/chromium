// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_CLIENT_H_
#define COMPONENTS_LEGION_CLIENT_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/legion/legion_common.h"
#include "components/legion/proto/legion.pb.h"
#include "components/legion/secure_channel.h"
#include "url/gurl.h"

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace legion {

// Client for starting the session and sending requests.
class Client {
 public:
  using BinaryEncodedProtoRequest = Request;
  using BinaryEncodedProtoResponse = Response;

  static constexpr base::TimeDelta kDefaultTimeout = base::Seconds(120);

  // Callback for when a `SendRequest` operation completes.
  // If the operation is successful, the result will contain the server's
  // response. Otherwise, it will contain an `ErrorCode` error.
  using OnRequestCompletedCallback = base::OnceCallback<void(
      base::expected<BinaryEncodedProtoResponse, ErrorCode> result)>;

  // Callback for when a `SendTextRequest` operation completes.
  using OnTextRequestCompletedCallback =
      base::OnceCallback<void(base::expected<std::string, ErrorCode> result)>;

  // Callback for when a `SendGenerateContentRequest` operation completes.
  using OnGenerateContentRequestCompletedCallback = base::OnceCallback<void(
      base::expected<proto::GenerateContentResponse, ErrorCode> result)>;

  // Callback for when a `EstablishSession` operation completes.
  using OnEstablishSessionCompletedCallback =
      base::OnceCallback<void(base::expected<void, ErrorCode>)>;

  using SecureChannelFactory =
      base::RepeatingCallback<std::unique_ptr<SecureChannel>()>;

  static std::unique_ptr<Client> Create(
      network::mojom::NetworkContext* network_context);

  static std::unique_ptr<Client> CreateWithUrl(
      const GURL& url,
      network::mojom::NetworkContext* network_context);

  // Takes a URL without scheme and an api_key and returns a URL.
  static GURL FormatUrl(const std::string& url, const std::string& api_key);

  ~Client();

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  // Establishes a secure session without sending a request. The callback will
  // be invoked upon completion. Calling this function is optional as a session
  // will be established automatically when needed/first request is sent.
  void EstablishSession(OnEstablishSessionCompletedCallback callback);

  // Sends a request with a single text content.
  void SendTextRequest(proto::FeatureName feature_name,
                       const std::string& text,
                       OnTextRequestCompletedCallback callback,
                       base::TimeDelta timeout = kDefaultTimeout);

  // Sends a `GenerateContentRequest`. The caller is responsible for populating
  // the `request` proto, including setting the content's role to "user".
  void SendGenerateContentRequest(
      proto::FeatureName feature_name,
      const proto::GenerateContentRequest& request,
      OnGenerateContentRequestCompletedCallback callback,
      base::TimeDelta timeout = kDefaultTimeout);

 private:
  friend class ClientTest;

  explicit Client(SecureChannelFactory channel_factory);

  // Recreates the secure channel and sets the response callback.
  void RecreateSecureChannel();

  // Sends a request over the secure channel.
  void SendRequest(int32_t request_id,
                   BinaryEncodedProtoRequest request,
                   OnRequestCompletedCallback callback,
                   base::TimeDelta timeout = kDefaultTimeout);

  // Handles responses from the secure channel.
  void OnResponseReceived(
      base::expected<BinaryEncodedProtoResponse, ErrorCode> result);

  // Wraps a request callback to record latency metrics upon completion.
  void OnRequestCompleted(
      OnRequestCompletedCallback callback,
      base::TimeTicks start_time,
      base::expected<BinaryEncodedProtoResponse, ErrorCode> result);

  // Handles a request timeout.
  void OnRequestTimeout(int32_t request_id);

  // Fails all pending requests with the given error code.
  void FailAllPendingRequests(ErrorCode error_code);

  // Handles the result of a session establishment request.
  void OnSessionEstablished(OnEstablishSessionCompletedCallback callback,
                            base::expected<void, ErrorCode> result);

  std::unique_ptr<SecureChannel> secure_channel_;
  SecureChannelFactory secure_channel_factory_;
  int32_t next_request_id_{1};

  // Callbacks for requests that have been sent to the secure channel but have
  // not yet received a response.
  base::flat_map<int32_t, OnRequestCompletedCallback> pending_requests_;

  // The request_ids of requests that have timed out.
  base::flat_set<int32_t> timed_out_requests_;

  base::WeakPtrFactory<Client> weak_factory_{this};
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_CLIENT_H_
