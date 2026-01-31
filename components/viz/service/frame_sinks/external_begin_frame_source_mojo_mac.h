// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_MOJO_MAC_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_MOJO_MAC_H_

#include "components/viz/service/frame_sinks/external_begin_frame_source_mac.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/direct_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/external_begin_frame_controller.mojom.h"

namespace viz {

// Implementation of VSync Source that's controlled by IPCs over the
// mojom::ExternalBeginFrameController interface from UI/browser process on Mac.
// This is only for using CADisplayLink created in the Browser process.
class VIZ_SERVICE_EXPORT ExternalBeginFrameSourceMojoMac
    : public mojom::ExternalBeginFrameController {
 public:
  ExternalBeginFrameSourceMojoMac(
      mojo::PendingReceiver<mojom::ExternalBeginFrameController>
          controller_receiver,
      mojo::PendingRemote<mojom::ExternalBeginFrameControllerClient>
          controller_remote_client,
      base::RepeatingClosure update_vsync_displays_cb);
  ~ExternalBeginFrameSourceMojoMac() override;

  // mojom::ExternalBeginFrameController implementation.
  // Originated from the browser process and propagated to DisplayLinkMac in the
  // Viz thread.
  void IssueExternalVSync(const CADisplayLinkParams& params) override;
  void SetSupportedDisplayLinkId(int64_t display_id,
                                 bool is_supported) override;

  // For headless only. This should not be called.
  void IssueExternalBeginFrame(
      const BeginFrameArgs& args,
      bool force,
      IssueExternalBeginFrameCallback callback) override;

  // This function forwards NeedsBeginFrame on/off from DisplayLinkMac in the
  // GPU process to DisplayLinkMacMojo in the browser process.
  void NeedsBeginFrameWithId(int64_t display_id, bool needs_begin_frames);

 private:
  using Receiver = mojo::Receiver<mojom::ExternalBeginFrameController>;
  using DirectReceiver =
      mojo::DirectReceiver<mojom::ExternalBeginFrameController>;
  std::variant<Receiver, DirectReceiver> receiver_;

  mojo::Remote<mojom::ExternalBeginFrameControllerClient> remote_client_;

  // This is a callback to FrameSinkManagerImpl::UpdateVSyncDisplays() which
  // updates DisplayLinkMac in all RootCompositorFrameSink if needed.
  base::RepeatingClosure update_vsync_displays_cb_;

  base::WeakPtrFactory<ExternalBeginFrameSourceMojoMac> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_MOJO_MAC_H_
