// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_MOJO_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_MOJO_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/frame_sinks/frame_sink_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/viz/privileged/mojom/compositing/external_begin_frame_controller.mojom.h"

namespace viz {

class FrameSinkManagerImpl;

// Implementation of ExternalBeginFrameSource that's controlled by IPCs over
// the mojom::ExternalBeginFrameController interface. Replaces the Display's
// default BeginFrameSource. Observes the Display to be notified of BeginFrame
// completion.
class VIZ_SERVICE_EXPORT ExternalBeginFrameSourceMojo
    : public mojom::ExternalBeginFrameController,
      public DisplayObserver,
      public ExternalBeginFrameSource,
      public ExternalBeginFrameSourceClient,
      public FrameSinkObserver {
 public:
  ExternalBeginFrameSourceMojo(
      FrameSinkManagerImpl* frame_sink_manager,
      mojo::PendingAssociatedReceiver<mojom::ExternalBeginFrameController>
          controller_receiver,
      uint32_t restart_id);
  ~ExternalBeginFrameSourceMojo() override;

  // mojom::ExternalBeginFrameController implementation.
  void IssueExternalBeginFrame(
      const BeginFrameArgs& args,
      bool force,
      base::OnceCallback<void(const BeginFrameAck&)> callback) override;

  void SetDisplay(Display* display);

 private:
  // ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override {}

  // DisplayObserver overrides.
  void OnDisplayDidFinishFrame(const BeginFrameAck& ack) override;
  void OnDisplayDestroyed() override;

  // FrameSinkObserver overrides.
  void OnRegisteredFrameSinkId(const FrameSinkId& frame_sink_id) override {}
  void OnInvalidatedFrameSinkId(const FrameSinkId& frame_sink_id) override {}
  void OnCreatedCompositorFrameSink(const FrameSinkId& frame_sink_id,
                                    bool is_root) override {}
  void OnDestroyedCompositorFrameSink(
      const FrameSinkId& frame_sink_id) override;
  void OnRegisteredFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) override {}
  void OnUnregisteredFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) override {}
  void OnFrameSinkDidBeginFrame(const FrameSinkId& frame_sink_id,
                                const BeginFrameArgs& args) override;
  void OnFrameSinkDidFinishFrame(const FrameSinkId& frame_sink_id,
                                 const BeginFrameArgs& args) override;

  void MaybeProduceFrameCallback();

  FrameSinkManagerImpl* const frame_sink_manager_;

  mojo::AssociatedReceiver<mojom::ExternalBeginFrameController> receiver_;
  base::OnceCallback<void(const BeginFrameAck& ack)> pending_frame_callback_;
  // The frame source id as specified in BeginFrameArgs passed to
  // IssueExternalBeginFrame. Note this is likely to be different from our
  // source id, but this is what will be reported to FrameSinkObserver methods.
  uint64_t original_source_id_ = BeginFrameArgs::kStartingSourceId;

  base::flat_set<FrameSinkId> pending_frame_sinks_;
  Display* display_ = nullptr;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_MOJO_H_
