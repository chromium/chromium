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
#include "components/private_ai/proto/legion.pb.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "url/gurl.h"

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace private_ai {

class LegionLogger;

// Interface for the legion client.
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

  // Callback for when a `EstablishSession` operation completes.
  using OnEstablishSessionCompletedCallback =
      base::OnceCallback<void(base::expected<void, ErrorCode>)>;

  struct RequestOptions {
    base::TimeDelta timeout = kDefaultTimeout;
  };

  static constexpr base::TimeDelta kDefaultTimeout = base::Seconds(120);

  static std::unique_ptr<Client> CreateWithApiKey(
      const GURL& url,
      network::mojom::NetworkContext* network_context,
      std::unique_ptr<LegionLogger> logger);

  static std::unique_ptr<Client> CreateWithToken(
      const GURL& url,
      network::mojom::NetworkContext* network_context,
      phosphor::TokenManager* token_manager,
      std::unique_ptr<LegionLogger> logger);

  static std::unique_ptr<Client> CreateWithProxyAndToken(
      const GURL& url,
      const GURL& proxy_url,
      network::mojom::NetworkService* network_service,
      phosphor::TokenManager* token_manager,
      std::unique_ptr<LegionLogger> logger);

  // Creates a client based on the provided configuration. This is a helper to
  // consolidate client creation logic.
  // - If `api_key` is not empty, it creates an API key based client.
  // - Otherwise, it creates a token based client.
  // - If `proxy_url_string` is not empty, the token based client will be
  // wrapped in a proxy.
  static std::unique_ptr<Client> Create(
      const std::string& url,
      const std::string& api_key,
      const std::string& proxy_url_string,
      network::mojom::NetworkContext* network_context,
      phosphor::TokenManager* token_manager,
      network::mojom::NetworkService* network_service,
      std::unique_ptr<LegionLogger> logger);

  virtual ~Client() = default;

  // Takes a URL without scheme and returns a URL.
  static GURL FormatUrl(const std::string& url);

  // Takes a URL without scheme and an api_key and returns a URL.
  static GURL FormatUrl(const std::string& url, const std::string& api_key);

  // Establishes a secure session without sending a request. The callback will
  // be invoked upon completion. Calling this function is optional as a session
  // will be established automatically when needed/first request is sent.
  virtual void EstablishSession(
      OnEstablishSessionCompletedCallback callback) = 0;

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

  virtual LegionLogger* GetLogger() = 0;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CLIENT_H_
