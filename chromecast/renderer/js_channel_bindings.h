// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_JS_CHANNEL_BINDINGS_H_
#define CHROMECAST_RENDERER_JS_CHANNEL_BINDINGS_H_

#include "base/macros.h"
#include "chromecast/common/mojom/js_channel.mojom.h"
#include "content/public/renderer/render_frame_observer.h"

namespace chromecast {

class JsChannelBindings : public content::RenderFrameObserver,
                          public mojom::JsChannelClient {
 public:
  static void Create(content::RenderFrame* render_frame);

  explicit JsChannelBindings(
      content::RenderFrame* render_frame,
      mojo::PendingReceiver<mojom::JsChannelClient> receiver);
  ~JsChannelBindings() override;

 private:
  // content::RenderFrameObserver implementation:
  void DidClearWindowObject() final;
  void OnDestruct() final;

  // mojom::JsChannelClient implementation:
  void CreateChannel(const std::string& channel,
                     mojo::PendingRemote<mojom::JsChannel> pipe) override;
  void RemoveChannel(const std::string& channel) override;

  void Install(const std::string& channel);

  void Func(const std::string& channel, v8::Local<v8::Value> message);

  std::vector<std::pair<std::string, mojo::Remote<mojom::JsChannel>>> channels_;

  mojo::Receiver<mojom::JsChannelClient> receiver_;

  DISALLOW_COPY_AND_ASSIGN(JsChannelBindings);
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_JS_CHANNEL_BINDINGS_H_
