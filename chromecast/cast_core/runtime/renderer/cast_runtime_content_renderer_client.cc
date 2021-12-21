// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/renderer/cast_runtime_content_renderer_client.h"

#include "chromecast/cast_core/runtime/renderer/cast_runtime_url_loader_throttle_provider.h"
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

const scoped_refptr<url_rewrite::UrlRequestRewriteRules>&
CastRuntimeContentRendererClient::GetUrlRewriteRules(int routing_id) const {
  auto iter = url_rewrite_rules_providers_.find(routing_id);
  DCHECK(iter != url_rewrite_rules_providers_.end());
  return iter->second->GetCachedRules();
}

void CastRuntimeContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  CastContentRendererClient::RenderFrameCreated(render_frame);
  cast_streaming_demuxer_provider_.RenderFrameCreated(render_frame);

  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      cast_streaming_renderer_controller_proxy_->GetBinder(render_frame));

  // Using |this| in the callback is safe because the UrlRewriteRulesProvider's
  // lifetime is bound to RenderFrame lifetime - as frame is going to be
  // deleted, the UrlRewriteRulesProvider will call this callback which will
  // delete it.
  auto url_rewrite_rules_provider = std::make_unique<UrlRewriteRulesProvider>(
      render_frame,
      base::BindOnce(&CastRuntimeContentRendererClient::OnRenderFrameDeleted,
                     base::Unretained(this)));
  url_rewrite_rules_providers_.emplace(render_frame->GetRoutingID(),
                                       std::move(url_rewrite_rules_provider));
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
  return std::make_unique<CastRuntimeURLLoaderThrottleProvider>(type, this);
}

void CastRuntimeContentRendererClient::OnRenderFrameDeleted(int routing_id) {
  auto iter = url_rewrite_rules_providers_.find(routing_id);
  DCHECK(iter != url_rewrite_rules_providers_.end());
  url_rewrite_rules_providers_.erase(iter);
}

}  // namespace chromecast
