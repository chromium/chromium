// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_DEMUXER_PROVIDER_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_DEMUXER_PROVIDER_H_

#include <map>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {
class RenderFrame;
}  // namespace content

namespace media {
class Demuxer;
}  // namespace media

namespace cast_streaming {

class CastStreamingRenderFrameObserver;

// Provides a media::Demuxer for a Cast Streaming receiver.
// Both public methods of this class should be called from the method of
// matching name in the embedder's ContentRendererClient implementation.
class DemuxerProvider {
 public:
  DemuxerProvider();
  ~DemuxerProvider();

  DemuxerProvider(const DemuxerProvider&) = delete;
  DemuxerProvider& operator=(const DemuxerProvider&) = delete;

  // Associates |render_frame| with a CastStreamingReceiver.
  void RenderFrameCreated(content::RenderFrame* render_frame);

  // Checks the |url| against a predefined constant, providing a
  // CastStreamingDemuxer instance in the case of a match.
  std::unique_ptr<media::Demuxer> OverrideDemuxerForUrl(
      content::RenderFrame* render_frame,
      const GURL& url,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner);

 private:
  // Called by CastStreamingRenderFrameObserver when its corresponding
  // RenderFrame is in the process of being deleted.
  void OnRenderFrameDeleted(int render_frame_id);

  // Map of RenderFrame ID to CastStreamingRenderFrameObserver.
  std::map<int, std::unique_ptr<CastStreamingRenderFrameObserver>>
      render_frame_id_to_observer_map_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_DEMUXER_PROVIDER_H_
