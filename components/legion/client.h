// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_CLIENT_H_
#define COMPONENTS_LEGION_CLIENT_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/functional/callback.h"
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

  // Callback for when a `SendRequest` operation completes.
  // If the operation is successful, the result will contain the server's
  // response. Otherwise, it will contain an `ErrorCode` error.
  using OnRequestCompletedCallback =
      base::OnceCallback<void(base::expected<BinaryEncodedProtoResponse, ErrorCode> result)>;

  // Callback for when a `SendTextRequest` operation completes.
  using OnTextRequestCompletedCallback =
      base::OnceCallback<void(base::expected<std::string, ErrorCode> result)>;

  // Callback for when a `SendGenerateContentRequest` operation completes.
  using OnGenerateContentRequestCompletedCallback = base::OnceCallback<void(
      base::expected<proto::GenerateContentResponse, ErrorCode> result)>;

  static std::unique_ptr<Client> Create(
      network::mojom::NetworkContext* network_context,
      proto::FeatureName feature_name);

  static std::unique_ptr<Client> CreateWithUrl(
      const GURL& url,
      network::mojom::NetworkContext* network_context,
      proto::FeatureName feature_name);

  // Takes a URL without scheme and an api_key and returns a URL.
  static GURL FormatUrl(const std::string& url, const std::string& api_key);

  ~Client();

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  // Sends a request with a single text content.
  void SendTextRequest(const std::string& text,
                       OnTextRequestCompletedCallback callback);

  // Sends a `GenerateContentRequest`. The caller is responsible for populating
  // the `request` proto, including setting the content's role to "user".
  void SendGenerateContentRequest(
      const proto::GenerateContentRequest& request,
      OnGenerateContentRequestCompletedCallback callback);

 private:
  friend class ClientTest;

  Client(std::unique_ptr<SecureChannel> secure_channel,
         proto::FeatureName feature_name);

  // Sends a request over the secure channel.
  void SendRequest(
      BinaryEncodedProtoRequest request,
      base::OnceCallback<void(base::expected<BinaryEncodedProtoResponse, ErrorCode>)> callback);

  std::unique_ptr<SecureChannel> secure_channel_;
  proto::FeatureName feature_name_;
  int32_t next_request_id_{1};
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_CLIENT_H_
