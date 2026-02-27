// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_CLIENT_H_
#define COMPONENTS_PRIVATE_AI_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/private_ai/error_code.h"
#include "components/private_ai/phosphor/token_manager.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "url/gurl.h"

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace private_ai {

class PrivateAiLogger;

// Interface for the PrivateAI client.
class Client {
 public:
  // Callback for when a `SendTextRequest` operation completes.
  using OnTextRequestCompletedCallback =
      base::OnceCallback<void(base::expected<std::string, ErrorCode> result)>;

  // Callback for when a `SendGenerateContentRequest` operation completes.
  using OnGenerateContentRequestCompletedCallback = base::OnceCallback<void(
      base::expected<proto::GenerateContentResponse, ErrorCode> result)>;

  // Callback for when a `SendPaicRequest` operation completes.
  using OnPaicMessageRequestCompletedCallback = base::OnceCallback<void(
      base::expected<proto::PaicMessage, ErrorCode> result)>;

  struct RequestOptions {
    base::TimeDelta timeout = kDefaultTimeout;
  };

  static constexpr base::TimeDelta kDefaultTimeout = base::Seconds(120);

  // Creates a client based on the provided configuration.
  // `url`: The URL for the PrivateAI service.
  // `api_key`: The API key for the PrivateAI service.
  // `proxy_url_string`: Optional URL for the proxy server.
  // `use_token_attestation`: Whether to use token attestation.
  // `network_context`: The network context to use for connections.
  // `token_manager`: Required if `use_token_attestation` is true.
  // `network_service`: Required if `proxy_url_string` is not empty.
  // `logger`: The logger for the client.
  static std::unique_ptr<Client> Create(
      const std::string& url,
      const std::string& api_key,
      const std::string& proxy_url_string,
      bool use_token_attestation,
      network::mojom::NetworkContext* network_context,
      phosphor::TokenManager* token_manager,
      network::mojom::NetworkService* network_service,
      std::unique_ptr<PrivateAiLogger> logger);

  virtual ~Client() = default;

  // Takes a URL without scheme and returns a URL.
  static GURL FormatUrl(const std::string& url);

  // Takes a URL without scheme and an api_key and returns a URL.
  static GURL FormatUrl(const std::string& url, const std::string& api_key);

  // Establishes a secure connection without sending a request. Calling this
  // function is optional as a connection will be established automatically
  // when needed/first request is sent.
  virtual void EstablishConnection() = 0;

  // Sends a request with a single text content.
  virtual void SendTextRequest(proto::FeatureName feature_name,
                               const std::string& text,
                               OnTextRequestCompletedCallback callback,
                               const RequestOptions& options) = 0;

  // Sends a `GenerateContentRequest`. The caller is responsible for populating
  // the `request` proto, including setting the content's role to "user".
  virtual void SendGenerateContentRequest(
      proto::FeatureName feature_name,
      const proto::GenerateContentRequest& request,
      OnGenerateContentRequestCompletedCallback callback,
      const RequestOptions& options) = 0;

  // Sends a `PaicMessage` request.
  virtual void SendPaicRequest(proto::FeatureName feature_name,
                               const proto::PaicMessage& request,
                               OnPaicMessageRequestCompletedCallback callback,
                               const RequestOptions& options) = 0;

  virtual PrivateAiLogger* GetLogger() = 0;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CLIENT_H_
