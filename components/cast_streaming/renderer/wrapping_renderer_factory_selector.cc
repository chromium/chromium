// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/public/wrapping_renderer_factory_selector.h"

#include <memory>

#include "components/cast_streaming/renderer/playback_command_forwarding_renderer_factory.h"
#include "components/cast_streaming/renderer/public/renderer_controller_proxy.h"

namespace cast_streaming {

WrappingRendererFactorySelector::WrappingRendererFactorySelector(
    content::RenderFrame* render_frame) {
  DCHECK(render_frame);
  auto* renderer_controller_proxy = RendererControllerProxy::GetInstance();
  DCHECK(renderer_controller_proxy);
  wrapping_factory_ =
      std::make_unique<PlaybackCommandForwardingRendererFactory>(
          renderer_controller_proxy->GetReceiver(render_frame));
}

WrappingRendererFactorySelector::~WrappingRendererFactorySelector() = default;

media::RendererType WrappingRendererFactorySelector::GetCurrentRendererType() {
  return media::RendererType::kCastStreaming;
}

media::RendererFactory* WrappingRendererFactorySelector::GetCurrentFactory() {
  DCHECK(wrapping_factory_);
  media::RendererFactory* wrapped_factory =
      RendererFactorySelector::GetCurrentFactory();
  // NOTE: |wrapped_factory| will outlive |wrapping_factory_|, as it is owned
  // by the base class.
  wrapping_factory_->SetWrappedRendererFactory(wrapped_factory);
  return wrapping_factory_.get();
}

}  // namespace cast_streaming
