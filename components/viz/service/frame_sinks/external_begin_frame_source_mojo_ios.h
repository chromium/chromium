// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_MOJO_IOS_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_MOJO_IOS_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/viz/privileged/mojom/compositing/external_begin_frame_controller.mojom.h"

namespace viz {

// Implementation of ExternalBeginFrameSource that's controlled by IPCs over the
// mojom::ExternalBeginFrameController interface from UI/browser process on iOS.
class VIZ_SERVICE_EXPORT ExternalBeginFrameSourceMojoIOS
    : public mojom::ExternalBeginFrameController,
      public ExternalBeginFrameSource,
      public ExternalBeginFrameSourceClient {
 public:
  // `controller_receiver` must be a valid mojo receiver.
  // `controller_client_remote` is optional and can be an invalid remote.
  ExternalBeginFrameSourceMojoIOS(
      mojo::PendingAssociatedReceiver<mojom::ExternalBeginFrameController>
          controller_receiver,
      mojo::PendingAssociatedRemote<mojom::ExternalBeginFrameControllerClient>
          controller_client_remote,
      uint32_t restart_id);
  ~ExternalBeginFrameSourceMojoIOS() override;

  // mojom::ExternalBeginFrameController implementation.
  void IssueExternalBeginFrameNoAck(const BeginFrameArgs& args) override;

 private:
  // ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;
  void SetPreferredInterval(base::TimeDelta interval) override;

  mojo::AssociatedReceiver<mojom::ExternalBeginFrameController> receiver_;
  mojo::AssociatedRemote<mojom::ExternalBeginFrameControllerClient>
      remote_client_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_MOJO_IOS_H_
