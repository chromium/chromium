// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_ASSISTANT_BINDINGS_H_
#define CHROMECAST_RENDERER_ASSISTANT_BINDINGS_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromecast/common/mojom/assistant_messenger.mojom.h"
#include "chromecast/renderer/native_bindings_helper.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromecast {
namespace shell {

// When enabled, these bindings can be used to open a message channel with the
// Assistant. These bindings are only enabled for a small set of first-party
// apps.
class AssistantBindings : public CastBinding,
                          public chromecast::mojom::AssistantMessageClient {
 public:
  AssistantBindings(content::RenderFrame* frame,
                    const base::Value::Dict& feature_config);
  ~AssistantBindings() override;
  AssistantBindings(const AssistantBindings&) = delete;
  AssistantBindings& operator=(const AssistantBindings&) = delete;

 private:
  friend class ::chromecast::CastBinding;

  // chromecast::mojom::AssistantMessageClient implementation:
  void OnMessage(base::Value message) override;

  // CastBinding implementation:
  void Install(v8::Local<v8::Object> cast_platform,
               v8::Isolate* isolate) override;

  // Binding methods
  void SetAssistantMessageHandler(
      v8::Local<v8::Function> assistant_message_handler);
  void SendAssistantRequest(const std::string& request);

  void ReconnectMessagePipe();
  void OnAssistantConnectionError();

  void FlushV8ToAssistantQueue();

  const mojo::Remote<chromecast::mojom::AssistantMessageService>&
  GetMojoInterface();

  base::RepeatingTimer reconnect_assistant_timer_;
  mojo::Remote<chromecast::mojom::AssistantMessageService> assistant_;
  base::Value::Dict feature_config_;

  mojo::Receiver<chromecast::mojom::AssistantMessageClient>
      message_client_binding_;
  mojo::Remote<chromecast::mojom::AssistantMessagePipe> message_pipe_;
  std::vector<std::string> v8_to_assistant_queue_;

  v8::UniquePersistent<v8::Function> assistant_message_handler_;

  base::WeakPtr<AssistantBindings> weak_this_;
  base::WeakPtrFactory<AssistantBindings> weak_factory_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_ASSISTANT_BINDINGS_H_
