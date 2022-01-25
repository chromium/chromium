// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/renderer/cast_runtime_content_renderer_client.h"

#include "base/bind.h"
#include "chromecast/cast_core/runtime/common/cors_exempt_headers.h"
#include "chromecast/renderer/cast_url_loader_throttle_provider.h"
#include "components/cast_streaming/public/cast_streaming_url.h"
#include "components/cast_streaming/renderer/public/renderer_controller_proxy.h"
#include "components/cast_streaming/renderer/public/renderer_controller_proxy_factory.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_media_playback_options.h"
#include "media/base/demuxer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace chromecast {

CastRuntimeContentRendererClient::CastRuntimeContentRendererClient()
    : cast_streaming_renderer_controller_proxy_(
          cast_streaming::CreateRendererControllerProxy()) {
  DCHECK(cast_streaming_renderer_controller_proxy_);
}

CastRuntimeContentRendererClient::~CastRuntimeContentRendererClient() = default;

void CastRuntimeContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  CastContentRendererClient::RenderFrameCreated(render_frame);
  cast_streaming_demuxer_provider_.RenderFrameCreated(render_frame);

  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      cast_streaming_renderer_controller_proxy_->GetBinder(render_frame));
}

std::unique_ptr<::media::Demuxer>
CastRuntimeContentRendererClient::OverrideDemuxerForUrl(
    content::RenderFrame* render_frame,
    const GURL& url,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner) {
  if (!cast_streaming::IsCastStreamingMediaSourceUrl(url)) {
    return nullptr;
  }

  LOG(INFO) << "Overriding demuxer for URL: " << url;
  return cast_streaming_demuxer_provider_.OverrideDemuxerForUrl(
      render_frame, url, std::move(media_task_runner));
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
CastRuntimeContentRendererClient::CreateURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType type) {
  return std::make_unique<CastURLLoaderThrottleProvider>(
      type, /*url_filter_manager=*/nullptr, this,
      base::BindRepeating(&IsHeaderCorsExempt));
}

}  // namespace chromecast
