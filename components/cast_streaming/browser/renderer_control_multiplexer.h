// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_RENDERER_CONTROL_MULTIPLEXER_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_RENDERER_CONTROL_MULTIPLEXER_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace cast_streaming {

// This class is responsible for multiplexing a number of media::mojo::Renderer
// callers by forwarding all such calls across a single mojo pipe.
class RendererControlMultiplexer : public media::mojom::Renderer {
 public:
  RendererControlMultiplexer(
      mojo::Remote<media::mojom::Renderer> renderer_remote,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~RendererControlMultiplexer() override;

  // Adds a new mojo pipe for which calls should be forwarded to
  // |renderer_remote_|.
  void RegisterController(
      mojo::PendingReceiver<media::mojom::Renderer> controls);

  // media::mojo::Renderer overrides.
  //
  // These calls only function to forward calls to |renderer_remote_|. Note that
  // mojo::FusePipes() cannot be used instead because we must retain the
  // |renderer_remote_| instance to make calls from here.
  void StartPlayingFrom(::base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  void SetCdm(const absl::optional<::base::UnguessableToken>& cdm_id,
              SetCdmCallback callback) override;
  void Initialize(
      mojo::PendingAssociatedRemote<media::mojom::RendererClient> client,
      absl::optional<
          std::vector<::mojo::PendingRemote<::media::mojom::DemuxerStream>>>
          streams,
      media::mojom::MediaUrlParamsPtr media_url_params,
      InitializeCallback callback) override;
  void Flush(FlushCallback callback) override;

 private:
  void OnMojoDisconnect();

  mojo::Remote<media::mojom::Renderer> renderer_remote_;
  std::vector<std::unique_ptr<mojo::Receiver<media::mojom::Renderer>>>
      receiver_list_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<RendererControlMultiplexer> weak_factory_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_RENDERER_CONTROL_MULTIPLEXER_H_
