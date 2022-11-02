// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_STREAMING_RUNTIME_APPLICATION_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_STREAMING_RUNTIME_APPLICATION_H_

#include "chromecast/cast_core/runtime/browser/runtime_application_base.h"
#include "chromecast/cast_core/runtime/browser/streaming_receiver_session_client.h"
#include "components/cast_streaming/browser/public/network_context_getter.h"

namespace cast_receiver {
class ApplicationClient;
}

namespace chromecast {

class MessagePortService;

class StreamingRuntimeApplication final
    : public RuntimeApplicationBase,
      public StreamingReceiverSessionClient::Handler {
 public:
  // |web_service| and |application_client| are expected to exist for the
  // lifetime of this instance.
  StreamingRuntimeApplication(
      std::string cast_session_id,
      cast::common::ApplicationConfig app_config,
      cast_receiver::ApplicationClient& application_client);
  ~StreamingRuntimeApplication() override;

 private:
  // RuntimeApplicationBase implementation:
  void Launch(StatusCallback callback) override;
  bool OnMessagePortMessage(cast::web::Message message) override;
  void StopApplication(cast::common::StopReason::Type stop_reason,
                       int32_t net_error_code) override;
  bool IsStreamingApplication() const override;

  // StreamingReceiverSessionClient::Handler implementation:
  void OnStreamingSessionStarted() override;
  void OnError() override;
  void StartAvSettingsQuery(
      std::unique_ptr<cast_api_bindings::MessagePort> message_port) override;
  void OnResolutionChanged(
      const gfx::Rect& size,
      const ::media::VideoTransformation& transformation) override;

  base::raw_ref<cast_receiver::ApplicationClient> const application_client_;

  // Returns the network context used by |receiver_session_client_|.
  const cast_streaming::NetworkContextGetter network_context_getter_;

  // Handles communication with cast core over gRPC.
  std::unique_ptr<MessagePortService> message_port_service_;

  // Object responsible for maintaining the lifetime of the streaming session.
  std::unique_ptr<StreamingReceiverSessionClient> receiver_session_client_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<StreamingRuntimeApplication> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_STREAMING_RUNTIME_APPLICATION_H_
