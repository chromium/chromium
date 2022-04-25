// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_RENDERER_CAST_RUNTIME_CONTENT_RENDERER_CLIENT_H_
#define CHROMECAST_CAST_CORE_RUNTIME_RENDERER_CAST_RUNTIME_CONTENT_RENDERER_CLIENT_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "chromecast/renderer/cast_content_renderer_client.h"
#include "chromecast/renderer/url_rewrite_rules_provider.h"

namespace cast_streaming {
class ResourceProvider;
}

namespace media {
class Demuxer;
}

namespace chromecast {

class CastRuntimeContentRendererClient
    : public shell::CastContentRendererClient {
 public:
  CastRuntimeContentRendererClient();
  CastRuntimeContentRendererClient(const CastRuntimeContentRendererClient&) =
      delete;
  CastRuntimeContentRendererClient(CastRuntimeContentRendererClient&&) = delete;
  ~CastRuntimeContentRendererClient() override;

  CastRuntimeContentRendererClient& operator=(
      const CastRuntimeContentRendererClient&) = delete;
  CastRuntimeContentRendererClient& operator=(
      CastRuntimeContentRendererClient&&) = delete;

  // content::ContentRendererClient overrides.
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  std::unique_ptr<::media::Demuxer> OverrideDemuxerForUrl(
      content::RenderFrame* render_frame,
      const GURL& url,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner) override;
  std::unique_ptr<blink::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType type) override;

 private:
  std::unique_ptr<cast_streaming::ResourceProvider>
      cast_streaming_resource_provider_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_RENDERER_CAST_RUNTIME_CONTENT_RENDERER_CLIENT_H_
