// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_CLIENT_IMPL_H_
#define COMPONENTS_LEGION_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/legion/client.h"
#include "components/legion/common/legion_logger.h"
#include "components/legion/connection.h"
#include "components/legion/connection_factory.h"
#include "components/legion/legion_common.h"
#include "components/legion/proto/legion.pb.h"

namespace legion {

class Connection;
class ConnectionFactory;

// Client for starting the session and sending requests.
class ClientImpl : public Client {
 public:
  ClientImpl(std::unique_ptr<ConnectionFactory> connection_factory,
             std::unique_ptr<LegionLogger> logger);
  ~ClientImpl() override;

  ClientImpl(const ClientImpl&) = delete;
  ClientImpl& operator=(const ClientImpl&) = delete;

  // Client overrides:
  void EstablishSession(OnEstablishSessionCompletedCallback callback) override;
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

  LegionLogger* GetLogger() override;

 private:
  // Callback for when a `SendRequest` operation completes.
  // If the operation is successful, the result will contain the server's
  // response. Otherwise, it will contain an `ErrorCode` error.
  using OnRequestCompletedCallback = base::OnceCallback<void(
      base::expected<proto::LegionResponse, ErrorCode> result)>;

  // Returns the existing connection or creates a new one if it doesn't
  // exist.
  Connection* GetOrCreateConnection();

  void SendRequest(proto::FeatureName feature_name,
                   proto::LegionRequest legion_request,
                   OnRequestCompletedCallback callback,
                   const RequestOptions& options);

  void OnReponseReceived(
      OnRequestCompletedCallback cb,
      base::expected<proto::LegionResponse, ErrorCode> legion_response);

  void OnConnectionDisconnected();

  std::unique_ptr<Connection> connection_;

  std::unique_ptr<ConnectionFactory> connection_factory_;

  std::unique_ptr<LegionLogger> logger_;

  base::WeakPtrFactory<ClientImpl> weak_factory_{this};
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_CLIENT_IMPL_H_
