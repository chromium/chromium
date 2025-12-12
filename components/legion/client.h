// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_CLIENT_H_
#define COMPONENTS_LEGION_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/legion/error_code.h"
#include "components/legion/legion_common.h"
#include "components/legion/proto/legion.pb.h"
#include "url/gurl.h"

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace legion {

// Interface for the legion client.
class Client {
 public:
  // Callback for when a `SendTextRequest` operation completes.
  using OnTextRequestCompletedCallback =
      base::OnceCallback<void(base::expected<std::string, ErrorCode> result)>;

  // Callback for when a `SendGenerateContentRequest` operation completes.
  using OnGenerateContentRequestCompletedCallback = base::OnceCallback<void(
      base::expected<proto::GenerateContentResponse, ErrorCode> result)>;

  // Callback for when a `EstablishSession` operation completes.
  using OnEstablishSessionCompletedCallback =
      base::OnceCallback<void(base::expected<void, ErrorCode>)>;

  struct RequestOptions {
    base::TimeDelta timeout = kDefaultTimeout;
  };

  static constexpr base::TimeDelta kDefaultTimeout = base::Seconds(120);

  static std::unique_ptr<Client> Create(
      network::mojom::NetworkContext* network_context);

  static std::unique_ptr<Client> CreateWithUrl(
      const GURL& url,
      network::mojom::NetworkContext* network_context);

  virtual ~Client() = default;

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
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_CLIENT_H_
