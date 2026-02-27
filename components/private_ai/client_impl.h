// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_CLIENT_IMPL_H_
#define COMPONENTS_PRIVATE_AI_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/private_ai/client.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/connection.h"
#include "components/private_ai/connection_factory.h"
#include "components/private_ai/private_ai_common.h"
#include "components/private_ai/proto/private_ai.pb.h"

namespace private_ai {

class Connection;
class ConnectionFactory;

// Client for starting the session and sending requests.
class ClientImpl : public Client {
 public:
  ClientImpl(std::unique_ptr<ConnectionFactory> connection_factory,
             std::unique_ptr<PrivateAiLogger> logger);
  ~ClientImpl() override;

  ClientImpl(const ClientImpl&) = delete;
  ClientImpl& operator=(const ClientImpl&) = delete;

  // Client overrides:
  void EstablishConnection() override;
  void SendTextRequest(proto::FeatureName feature_name,
                       const std::string& text,
                       OnTextRequestCompletedCallback callback,
                       const RequestOptions& options) override;
  void SendGenerateContentRequest(
      proto::FeatureName feature_name,
      const proto::GenerateContentRequest& request,
      OnGenerateContentRequestCompletedCallback callback,
      const RequestOptions& options) override;
  void SendPaicRequest(proto::FeatureName feature_name,
                       const proto::PaicMessage& request,
                       OnPaicMessageRequestCompletedCallback callback,
                       const RequestOptions& options) override;

  PrivateAiLogger* GetLogger() override;

 private:
  // Callback for when a `SendRequest` operation completes.
  // If the operation is successful, the result will contain the server's
  // response. Otherwise, it will contain an `ErrorCode` error.
  using OnRequestCompletedCallback = base::OnceCallback<void(
      base::expected<proto::PrivateAiResponse, ErrorCode> result)>;

  // Returns the existing connection or creates a new one if it doesn't
  // exist.
  Connection* GetOrCreateConnection();

  void SendRequest(proto::FeatureName feature_name,
                   proto::PrivateAiRequest private_ai_request,
                   OnRequestCompletedCallback callback,
                   const RequestOptions& options);

  void OnReponseReceived(
      OnRequestCompletedCallback cb,
      base::expected<proto::PrivateAiResponse, ErrorCode> private_ai_response);

  void OnConnectionDisconnected(ErrorCode error_code);

  std::unique_ptr<PrivateAiLogger> logger_;

  std::unique_ptr<Connection> connection_;

  std::unique_ptr<ConnectionFactory> connection_factory_;

  base::WeakPtrFactory<ClientImpl> weak_factory_{this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CLIENT_IMPL_H_
