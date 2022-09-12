// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/renderer/cast_runtime_content_renderer_client.h"

#include "base/bind.h"
#include "chromecast/cast_core/runtime/common/cors_exempt_headers.h"
#include "chromecast/renderer/cast_url_loader_throttle_provider.h"
#include "components/cast_streaming/renderer/public/resource_provider.h"
#include "components/cast_streaming/renderer/public/resource_provider_factory.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_media_playback_options.h"
#include "media/base/demuxer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace chromecast {

CastRuntimeContentRendererClient::CastRuntimeContentRendererClient() = default;

CastRuntimeContentRendererClient::~CastRuntimeContentRendererClient() = default;

std::unique_ptr<blink::URLLoaderThrottleProvider>
CastRuntimeContentRendererClient::CreateURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType type) {
  return std::make_unique<CastURLLoaderThrottleProvider>(
      type, /*url_filter_manager=*/nullptr, this,
      base::BindRepeating(&IsHeaderCorsExempt));
}

std::unique_ptr<cast_streaming::ResourceProvider>
CastRuntimeContentRendererClient::CreateCastStreamingResourceProvider() {
  return cast_streaming::CreateResourceProvider();
}

}  // namespace chromecast
