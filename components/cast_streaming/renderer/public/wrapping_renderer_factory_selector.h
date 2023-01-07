// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_WRAPPING_RENDERER_FACTORY_SELECTOR_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_WRAPPING_RENDERER_FACTORY_SELECTOR_H_

#include <memory>

#include "media/base/renderer_factory_selector.h"

namespace cast_streaming {

class ResourceProvider;
class PlaybackCommandForwardingRendererFactory;

// This class provides an implementation of RendererFactorySelector to be used
// when the PlaybackCommandForwardingRenderer is desired. All functionality is
// as in the base class, with the exception that the GetCurrentRendererFactory()
// method always returns a PlaybackCommandForwardingRendererFactory, which is
// set to wrap the RendererFactory which would have otherwise been returned, and
// GetCurrentRendererType() always returns kCastStreaming.
class WrappingRendererFactorySelector : public media::RendererFactorySelector {
 public:
  explicit WrappingRendererFactorySelector(ResourceProvider* resource_provider);
  ~WrappingRendererFactorySelector() override;

  // media::RendererFactorySelector overrides.
  media::RendererFactory* GetCurrentFactory() override;

 private:
  std::unique_ptr<PlaybackCommandForwardingRendererFactory> wrapping_factory_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_WRAPPING_RENDERER_FACTORY_SELECTOR_H_
