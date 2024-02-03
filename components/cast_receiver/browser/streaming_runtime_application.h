// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_RUNTIME_APPLICATION_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_RUNTIME_APPLICATION_H_

#include "components/cast_receiver/browser/public/application_config.h"
#include "components/cast_receiver/browser/runtime_application_base.h"
#include "components/cast_receiver/browser/streaming_receiver_session_client.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/network_context_getter.h"

namespace cast_receiver {

class ApplicationClient;
class MessagePortService;

class StreamingRuntimeApplication final
    : public RuntimeApplicationBase,
      public StreamingReceiverSessionClient::Handler {
 public:
  // |application_client| is expected to exist for the lifetime of this
  // instance.
  StreamingRuntimeApplication(std::string cast_session_id,
                              ApplicationConfig app_config,
                              ApplicationClient& application_client);
  ~StreamingRuntimeApplication() override;

  StreamingRuntimeApplication(StreamingRuntimeApplication& other) = delete;
  StreamingRuntimeApplication& operator=(StreamingRuntimeApplication& other) =
      delete;

 private:
  // RuntimeApplicationBase implementation:
  void Launch(StatusCallback callback) override;
  void StopApplication(EmbedderApplication::ApplicationStopReason stop_reason,
                       net::Error net_error_code) override;
  bool IsStreamingApplication() const override;

  // StreamingReceiverSessionClient::Handler implementation:
  void OnStreamingSessionStarted() override;
  void OnError() override;
  void OnResolutionChanged(
      const gfx::Rect& size,
      const media::VideoTransformation& transformation) override;

  // Returns the network context used by |receiver_session_client_|.
  const network::NetworkContextGetter network_context_getter_;

  // Handles communication with other MessagePort endpoints.
  std::unique_ptr<MessagePortService> message_port_service_;

  // Object responsible for maintaining the lifetime of the streaming session.
  std::unique_ptr<StreamingReceiverSessionClient> receiver_session_client_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<StreamingRuntimeApplication> weak_factory_{this};
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_RUNTIME_APPLICATION_H_
