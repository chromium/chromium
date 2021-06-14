// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_CAST_RUNTIME_CONTENT_RENDERER_CLIENT_H_
#define CHROMECAST_CAST_CORE_CAST_RUNTIME_CONTENT_RENDERER_CLIENT_H_

#include <memory>

#include "chromecast/renderer/cast_content_renderer_client.h"
#include "components/cast_streaming/renderer/public/demuxer_provider.h"

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

 private:
  cast_streaming::DemuxerProvider cast_streaming_demuxer_provider_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_CAST_RUNTIME_CONTENT_RENDERER_CLIENT_H_
