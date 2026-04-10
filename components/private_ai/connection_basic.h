// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_CONNECTION_BASIC_H_
#define COMPONENTS_PRIVATE_AI_CONNECTION_BASIC_H_

#include <cstdint>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "components/private_ai/connection.h"
#include "components/private_ai/private_ai_common.h"
#include "components/private_ai/secure_channel.h"
#include "components/private_ai/status_code.h"

namespace private_ai {

// A very basic implementation of the `Connection` interface that sends
// requests over a `SecureChannel`. It resolves PrivateAI responses to the
// corresponding callback based on the `request_id`.
class ConnectionBasic : public Connection {
 public:
  // When `on_disconnect` callback is invoked, all follow-up `Send()` calls will
  // fail immediately without attempting to send a request over the wire.
  ConnectionBasic(
      std::unique_ptr<SecureChannel::Factory> secure_channel_factory,
      base::OnceCallback<void(StatusCode)> on_disconnect);
  ~ConnectionBasic() override;

  ConnectionBasic(const ConnectionBasic&) = delete;
  ConnectionBasic& operator=(const ConnectionBasic&) = delete;

  // Connection override:

  // Sends requests to the PrivateAI server.
  //
  // `timeout` is not handled in `ConnectionBasic`.
  void Send(proto::PrivateAiRequest request,
            base::TimeDelta timeout,
            OnRequestCallback callback) override;

  void OnDestroy(StatusCode status_code) override;

 private:
  // Handles responses from the secure channel.
  void OnResponseReceived(base::expected<Response, StatusCode> result);

  // Handles disconnect by resolving all `pending_request_callbacks_` with
  // `status` and resolves `on_disconnect_` callback if not yet resolved.
  void CallOnDisconnect(StatusCode status_code);

  std::unique_ptr<SecureChannel> secure_channel_;

  // Called to trigger a disconnect and destruction of the connection.
  base::OnceCallback<void(StatusCode)> on_disconnect_;

  int32_t next_request_id_{1};

  // Callbacks for requests that have been sent to the secure channel, but have
  // not yet received a response.
  base::flat_map<int32_t, OnRequestCallback> pending_request_callbacks_;

  base::WeakPtrFactory<ConnectionBasic> weak_factory_{this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CONNECTION_BASIC_H_
