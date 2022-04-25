// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_RESOURCE_PROVIDER_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_RESOURCE_PROVIDER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/cast_streaming/public/mojom/renderer_controller.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace content {
class RenderFrame;
}  // namespace content

namespace media {
class Demuxer;
}  // namespace media

namespace cast_streaming {

class ResourceProvider {
 public:
  // Creates a new ResourceProvider instance.
  //
  // To be defined by the implementing class.
  static std::unique_ptr<ResourceProvider> Create();

  // Gets the receiver associated with a given |render_frame|. To be used by the
  // renderer-process PlaybackCommandForwardingRenderer to receive playback
  // commands from the browser.
  static mojo::PendingReceiver<media::mojom::Renderer> GetReceiver(
      content::RenderFrame* render_frame);

  virtual ~ResourceProvider();

  ResourceProvider(const ResourceProvider&) = delete;
  ResourceProvider& operator=(const ResourceProvider&) = delete;

  // Associates |render_frame| with its singleton resources.
  virtual void RenderFrameCreated(content::RenderFrame* render_frame) = 0;

  // Checks the |url| against a predefined constant, providing a
  // CastStreamingDemuxer instance in the case of a match.
  virtual std::unique_ptr<media::Demuxer> OverrideDemuxerForUrl(
      content::RenderFrame* render_frame,
      const GURL& url,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner) = 0;

 protected:
  ResourceProvider();

  // Gets the receiver associated with a given |render_frame|.
  virtual mojo::PendingReceiver<media::mojom::Renderer> GetReceiverImpl(
      content::RenderFrame* render_frame) = 0;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_RESOURCE_PROVIDER_H_
