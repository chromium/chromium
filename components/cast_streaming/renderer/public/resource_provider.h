// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_RESOURCE_PROVIDER_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_RESOURCE_PROVIDER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast_streaming/common/public/mojom/demuxer_connector.mojom.h"
#include "components/cast_streaming/common/public/mojom/renderer_controller.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "url/gurl.h"

namespace media {
class Demuxer;
}  // namespace media

namespace cast_streaming {

// This class is responsible for initiating all per-RenderFrame mojo connections
// with the browser process, as required to render cast streaming media. This
// includes:
// - The DemuxerConnector API, feeding frames to the media::Demuxer instantiated
//   through this instance's MaybeGetDemuxerOverride() method.
// - The RendererController API, for sending media::Renderer commands from the
//   browser process, both for use when remoting or for the embedder to inject
//   playback commands.
// Additionally, this class provides the hooks required for using these mojo
// APIs.
class ResourceProvider {
 public:
  template <typename TMojoInterfaceType>
  using ReceiverBinder = base::RepeatingCallback<void(
      mojo::PendingAssociatedReceiver<TMojoInterfaceType>)>;

  virtual ~ResourceProvider();

  ResourceProvider(const ResourceProvider&) = delete;
  ResourceProvider& operator=(const ResourceProvider&) = delete;

  // Gets the binder to be used by the AssociatedInterfaceRegistry to pass
  // browser-process mojo API endpoints to this class.
  virtual ReceiverBinder<mojom::RendererController>
  GetRendererControllerBinder() = 0;
  virtual ReceiverBinder<mojom::DemuxerConnector>
  GetDemuxerConnectorBinder() = 0;

  // Checks the |url| against a predefined constant, providing a
  // CastStreamingDemuxer instance in the case of a match.
  virtual std::unique_ptr<media::Demuxer> MaybeGetDemuxerOverride(
      const GURL& url,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner) = 0;

  // Gets the receiver for this instance. To be used by the renderer-process
  // PlaybackCommandForwardingRenderer to receive playback commands from the
  // browser.
  virtual mojo::PendingReceiver<media::mojom::Renderer>
  GetRendererCommandReceiver() = 0;

 protected:
  ResourceProvider();
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_RESOURCE_PROVIDER_H_
