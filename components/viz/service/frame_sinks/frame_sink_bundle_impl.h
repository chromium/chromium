// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_BUNDLE_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_BUNDLE_IMPL_H_

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/frame_sink_bundle_id.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/mojom/compositing/frame_sink_bundle.mojom.h"

namespace viz {

class CompositorFrameSinkImpl;
class CompositorFrameSinkSupport;
class FrameSinkManagerImpl;

// This object receives aggregate SubmitCompositorFrame and DidNotProduceFrame
// messages from remote CompositorFrameSink clients, with the `client_id`, who
// who were created as part of this bundle.
//
// Outgoing client messages from the corresponding CompositorFrameSinkImpls are
// also aggregated here and sent in batch back to the client.
//
// The bundle accepts any kind of sink from the same client, and messages are
// batched together for sinks which share a BeginFrameSource.
class FrameSinkBundleImpl : public mojom::FrameSinkBundle {
 public:
  // Constructs a new FrameSinkBundleImpl for a subset of sinks belonging to
  // `client_id`.
  FrameSinkBundleImpl(FrameSinkManagerImpl& manager,
                      const FrameSinkBundleId& id,
                      mojo::PendingReceiver<mojom::FrameSinkBundle> receiver,
                      mojo::PendingRemote<mojom::FrameSinkBundleClient> client);
  FrameSinkBundleImpl(const FrameSinkBundleImpl&) = delete;
  FrameSinkBundleImpl& operator=(const FrameSinkBundleImpl&) = delete;
  ~FrameSinkBundleImpl() override;

  const FrameSinkBundleId& id() const { return id_; }

  // Called by the identified sink itself to notify the bundle that the sink
  // needs (or no longer needs) BeginFrame notifications. This is distinct from
  // SetNeedsBeginFrame(), as the latter is only called by clients.
  void SetSinkNeedsBeginFrame(uint32_t sink_id, bool needs_begin_frame);

  void AddFrameSink(CompositorFrameSinkSupport* support);
  void UpdateFrameSink(CompositorFrameSinkSupport* support,
                       BeginFrameSource* old_source);
  void RemoveFrameSink(CompositorFrameSinkSupport* support);

  // mojom::FrameSinkBundle implementation:
  void InitializeCompositorFrameSinkType(
      uint32_t sink_id,
      mojom::CompositorFrameSinkType type) override;
  void SetNeedsBeginFrame(uint32_t sink_id, bool needs_begin_frame) override;
  void SetWantsBeginFrameAcks(uint32_t sink_id) override;
  void Submit(
      std::vector<mojom::BundledFrameSubmissionPtr> submissions) override;
  void DidAllocateSharedBitmap(uint32_t sink_id,
                               base::ReadOnlySharedMemoryRegion region,
                               const SharedBitmapId& id) override;
#if BUILDFLAG(IS_ANDROID)
  void SetThreadIds(uint32_t sink_id,
                    const std::vector<int32_t>& thread_ids) override;
#endif

  // Helpers used by each CompositorFrameSinkImpl to proxy their client messages
  // to this object for potentially batched communication.
  void EnqueueDidReceiveCompositorFrameAck(
      uint32_t sink_id,
      std::vector<ReturnedResource> resources);
  void EnqueueOnBeginFrame(
      uint32_t sink_id,
      const BeginFrameArgs& args,
      const base::flat_map<uint32_t, FrameTimingDetails>& details,
      bool frame_ack,
      std::vector<ReturnedResource> resources);
  void EnqueueReclaimResources(uint32_t sink_id,
                               std::vector<ReturnedResource> resources);
  void SendOnBeginFramePausedChanged(uint32_t sink_id, bool paused);
  void SendOnCompositorFrameTransitionDirectiveProcessed(uint32_t sink_id,
                                                         uint32_t sequence_id);

 private:
  class SinkGroup;

  void RemoveFrameSinkImpl(BeginFrameSource* source, uint32_t sink_id);

  CompositorFrameSinkImpl* GetFrameSink(uint32_t sink_id) const;
  CompositorFrameSinkSupport* GetFrameSinkSupport(uint32_t sink_id) const;
  SinkGroup* GetSinkGroup(uint32_t sink_id) const;

  void OnDisconnect();

  const raw_ref<FrameSinkManagerImpl> manager_;
  const FrameSinkBundleId id_;

  mojo::Receiver<mojom::FrameSinkBundle> receiver_;
  mojo::Remote<mojom::FrameSinkBundleClient> client_;

  // Set of sinks that are in the bundle but don't yet have a known
  // BeginFrameSource. These sinks will not receive OnBeginFrame notifications
  // until they get a BeginFrameSource.
  std::set<uint32_t> sourceless_sinks_;

  // Mapping from BeginFrameSource to the SinkGroup which manages batched
  // communication for all the sinks which share that source.
  base::flat_map<BeginFrameSource*, std::unique_ptr<SinkGroup>> sink_groups_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_BUNDLE_IMPL_H_
