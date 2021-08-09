// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_BUNDLE_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_BUNDLE_IMPL_H_

#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/frame_sink_bundle_id.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/mojom/compositing/frame_sink_bundle.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace viz {

class CompositorFrameSinkImpl;
class FrameSinkManagerImpl;

// This object receives aggregate SubmitCompositorFrame and DidNotProduceFrame
// messages from remote CompositorFrameSink clients, with the `client_id`, who
// who were created as part of this bundle.
//
// Outgoing client messages from the corresponding CompositorFrameSinkImpls are
// also aggregated here and sent in batch back to the client.
//
// Every sink added to a bundle must be of the same CompositorFrameSinkType and
// use the same BeginFrameSource.
class FrameSinkBundleImpl : public mojom::FrameSinkBundle,
                            public BeginFrameObserver {
 public:
  // Constructs a new FrameSinkBundleImpl for a subset of sinks belonging to
  // `client_id`. This object observes OnBeginFrame() events from
  // `begin_frame_source` and pushes them synchronously to each frame sink in
  // the bundle at the same time.
  FrameSinkBundleImpl(FrameSinkManagerImpl& manager,
                      const FrameSinkBundleId& id,
                      BeginFrameSource* begin_frame_source,
                      mojo::PendingReceiver<mojom::FrameSinkBundle> receiver,
                      mojo::PendingRemote<mojom::FrameSinkBundleClient> client);
  FrameSinkBundleImpl(const FrameSinkBundleImpl&) = delete;
  FrameSinkBundleImpl& operator=(const FrameSinkBundleImpl&) = delete;
  ~FrameSinkBundleImpl() override;

  BeginFrameSource* begin_frame_source() { return begin_frame_source_; }

  void AddFrameSink(const FrameSinkId& id);
  void RemoveFrameSink(const FrameSinkId& id);

  // mojom::FrameSinkBundle implementation:
  void InitializeCompositorFrameSinkType(
      uint32_t sink_id,
      mojom::CompositorFrameSinkType type) override;
  void SetNeedsBeginFrame(uint32_t sink_id, bool needs_begin_frame) override;
  void Submit(
      std::vector<mojom::BundledFrameSubmissionPtr> submissions) override;
  void DidAllocateSharedBitmap(uint32_t sink_id,
                               base::ReadOnlySharedMemoryRegion region,
                               const gpu::Mailbox& id) override;

  // Helpers used by each CompositorFrameSinkImpl to proxy their client messages
  // to this object for potentially batched communication.
  void EnqueueDidReceiveCompositorFrameAck(
      uint32_t sink_id,
      std::vector<ReturnedResource> resources);
  void EnqueueOnBeginFrame(
      uint32_t sink_id,
      const BeginFrameArgs& args,
      const base::flat_map<uint32_t, FrameTimingDetails>& details);
  void EnqueueReclaimResources(uint32_t sink_id,
                               std::vector<ReturnedResource> resources);
  void SendOnBeginFramePausedChanged(uint32_t sink_id, bool paused);
  void SendOnCompositorFrameTransitionDirectiveProcessed(uint32_t sink_id,
                                                         uint32_t sequence_id);

  void UnregisterBeginFrameSource(BeginFrameSource* source);

 private:
  CompositorFrameSinkImpl* GetFrameSink(uint32_t sink_id) const;

  void FlushMessages();
  void OnDisconnect();

  // BeginFrameObserver implementation:
  void OnBeginFrame(const BeginFrameArgs& args) override;
  const BeginFrameArgs& LastUsedBeginFrameArgs() const override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;
  bool WantsAnimateOnlyBeginFrames() const override;

  FrameSinkManagerImpl& manager_;
  const FrameSinkBundleId id_;
  BeginFrameSource* begin_frame_source_;
  mojo::Receiver<mojom::FrameSinkBundle> receiver_;
  mojo::Remote<mojom::FrameSinkBundleClient> client_;

  BeginFrameArgs last_used_begin_frame_args_;

  size_t num_unacked_submissions_ = 0;

  absl::optional<mojom::CompositorFrameSinkType> sink_type_;
  bool defer_on_begin_frames_ = false;
  std::vector<mojom::BundledReturnedResourcesPtr> pending_received_frame_acks_;
  std::vector<mojom::BundledReturnedResourcesPtr> pending_reclaimed_resources_;
  std::vector<mojom::BeginFrameInfoPtr> pending_on_begin_frames_;

  std::set<uint32_t> frame_sinks_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_BUNDLE_IMPL_H_
