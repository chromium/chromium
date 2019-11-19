// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_WEBRTC_LOGGING_AGENT_IMPL_H_
#define CHROME_RENDERER_MEDIA_WEBRTC_LOGGING_AGENT_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/media/webrtc_logging.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chrome {

class WebRtcLoggingAgentImpl : public mojom::WebRtcLoggingAgent {
 public:
  WebRtcLoggingAgentImpl();
  ~WebRtcLoggingAgentImpl() override;

  void AddReceiver(mojo::PendingReceiver<mojom::WebRtcLoggingAgent> receiver);

  // mojom::WebRtcLoggingAgent methods:
  void Start(
      mojo::PendingRemote<mojom::WebRtcLoggingClient> pending_client) override;
  void Stop() override;

 private:
  void OnNewMessage(mojom::WebRtcLoggingMessagePtr message);
  void SendLogBuffer();

  mojo::ReceiverSet<mojom::WebRtcLoggingAgent> self_receiver_set_;
  mojo::Remote<mojom::WebRtcLoggingClient> client_;
  std::vector<mojom::WebRtcLoggingMessagePtr> log_buffer_;
  base::TimeTicks last_log_buffer_send_;

  base::WeakPtrFactory<WebRtcLoggingAgentImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebRtcLoggingAgentImpl);
};

}  // namespace chrome

#endif  // CHROME_RENDERER_MEDIA_WEBRTC_LOGGING_AGENT_IMPL_H_
