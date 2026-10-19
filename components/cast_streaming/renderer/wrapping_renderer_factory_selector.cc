// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/public/wrapping_renderer_factory_selector.h"

#include <memory>
#include <utility>

#include "components/cast_streaming/renderer/control/playback_command_forwarding_renderer_factory.h"
#include "components/cast_streaming/renderer/public/resource_provider.h"

namespace cast_streaming {

WrappingRendererFactorySelector::WrappingRendererFactorySelector(
    ResourceProvider* resource_provider) {
  DCHECK(resource_provider);

  // The receiver is unbound once the RendererController connection has been
  // lost; there is nothing to forward commands from in that case.
  auto receiver = resource_provider->GetRendererCommandReceiver();
  if (receiver) {
    wrapping_factory_ =
        std::make_unique<PlaybackCommandForwardingRendererFactory>(
            std::move(receiver));
  }
}

WrappingRendererFactorySelector::~WrappingRendererFactorySelector() = default;

media::RendererFactory* WrappingRendererFactorySelector::GetCurrentFactory() {
  media::RendererFactory* wrapped_factory =
      RendererFactorySelector::GetCurrentFactory();
  if (!wrapping_factory_) {
    return wrapped_factory;
  }
  // NOTE: |wrapped_factory| will outlive |wrapping_factory_|, as it is owned
  // by the base class.
  wrapping_factory_->SetWrappedRendererFactory(wrapped_factory);
  return wrapping_factory_.get();
}

}  // namespace cast_streaming
