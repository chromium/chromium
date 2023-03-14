// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_RENDERER_CONTROLLER_CONFIG_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_RENDERER_CONTROLLER_CONFIG_H_

#include "components/cast_streaming/common/public/mojom/renderer_controller.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace cast_streaming {

struct RendererControllerConfig {
  RendererControllerConfig();
  RendererControllerConfig(
      mojo::AssociatedRemote<mojom::RendererController> control_config,
      mojo::PendingReceiver<media::mojom::Renderer> external_renderer_control);
  RendererControllerConfig(RendererControllerConfig&& other);

  ~RendererControllerConfig();

  RendererControllerConfig& operator=(RendererControllerConfig&& other);

  // Mojo pipe required to set up a connection between the browser process
  // controls and the renderer-process PlaybackCommandForwardingRenderer.
  mojo::AssociatedRemote<mojom::RendererController> control_configuration;

  // Provides an external channel by which the underlying Renderer may be
  // controlled outside of those used during a Remoting session.
  mojo::PendingReceiver<media::mojom::Renderer> external_renderer_controls;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_RENDERER_CONTROLLER_CONFIG_H_
