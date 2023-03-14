// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/control/renderer_controller_config.h"

namespace cast_streaming {

RendererControllerConfig::RendererControllerConfig() = default;

RendererControllerConfig::RendererControllerConfig(
    mojo::AssociatedRemote<mojom::RendererController> control_config,
    mojo::PendingReceiver<media::mojom::Renderer> external_renderer_control)
    : control_configuration(std::move(control_config)),
      external_renderer_controls(std::move(external_renderer_control)) {}

RendererControllerConfig::RendererControllerConfig(
    RendererControllerConfig&& other) = default;

RendererControllerConfig::~RendererControllerConfig() = default;

RendererControllerConfig& RendererControllerConfig::operator=(
    RendererControllerConfig&& other) = default;

}  // namespace cast_streaming
