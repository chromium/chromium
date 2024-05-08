// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/frame_sink_bundle_impl.h"

#include <map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_impl.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/mojom/compositing/frame_sink_bundle.mojom.h"

namespace viz {

// A SinkGroup is responsible for batching messages out to a group of
// bundled CompositorFrameSink clients who all share a common BeginFrameSource.
// FrameSinkBundleImpls may own any number of SinkGroups, and groups are created
// or destroyed as needed when a sink is added to or removed from the bundle.
//
// Note that the BeginFrameSource is only observed by this SinkGroup while there
// are active FrameSinks present who have explicitly indicated a need for
// BeginFrame notifications. This avoids generation and processing of unused
// frame events which might otherwise incur substantial overhead.
class FrameSinkBundleImpl::SinkGroup : public BeginFrameObserver {
 public:
  SinkGroup(FrameSinkManagerImpl& manager,
            FrameSinkBundleImpl& bundle,
            BeginFrameSource& source,
            mojom::FrameSinkBundleClient& client)
      : manager_(manager), bundle_(bundle), source_(source), client_(client) {}

  ~SinkGroup() override {
    if (is_observing_begin_frame_) {
      source_->RemoveObserver(this);
    }
  }

  bool IsEmpty() const { return frame_sinks_.empty(); }

  base::WeakPtr<SinkGroup> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void AddFrameSink(uint32_t sink_id) {
    frame_sinks_.insert(sink_id);

    FrameSinkId id(bundle_->id().client_id(), sink_id);
    if (auto* support = manager_->GetFrameSinkForId(id)) {
      if (support->needs_begin_frame()) {
        frame_sinks_needing_begin_frame_.insert(sink_id);
        UpdateBeginFrameObservation();
      }
    }
  }

  void RemoveFrameSink(uint32_t sink_id) {
    frame_sinks_.erase(sink_id);
    unacked_submissions_.erase(sink_id);
    FlushMessages();

    frame_sinks_needing_begin_frame_.erase(sink_id);
    UpdateBeginFrameObservation();
  }

  void SetNeedsBeginFrame(uint32_t sink_id, bool needs_begin_frame) {
    if (needs_begin_frame) {
      frame_sinks_needing_begin_frame_.insert(sink_id);
    } else {
      frame_sinks_needing_begin_frame_.erase(sink_id);
    }

    UpdateBeginFrameObservation();
  }

  void WillSubmitFrame(uint32_t sink_id) {
    unacked_submissions_.insert(sink_id);
  }

  void EnqueueDidReceiveCompositorFrameAck(
      uint32_t sink_id,
      std::vector<ReturnedResource> resources) {
    pending_received_frame_acks_.push_back(
        mojom::BundledReturnedResources::New(sink_id, std::move(resources)));

    unacked_submissions_.erase(sink_id);

    // We expect to be notified about the consumption of all previously
    // submitted frames at approximately the same time. This condition allows us
    // to batch most ack messages without any significant delays. Note that
    // sink IDs are added to `unacked_submissions_` in WillSubmitFrame() for
    // each sink that submits a frame.
    if (unacked_submissions_.empty()) {
      FlushMessages();
    }
  }

  void EnqueueOnBeginFrame(
      uint32_t sink_id,
      const BeginFrameArgs& args,
      const base::flat_map<uint32_t, FrameTimingDetails>& details,
      bool frame_ack,
      std::vector<ReturnedResource> resources) {
    pending_on_begin_frames_.push_back(mojom::BeginFrameInfo::New(
        sink_id, args, details, frame_ack, std::move(resources)));
    if (!defer_on_begin_frames_) {
      FlushMessages();
    }
  }

  void EnqueueReclaimResources(uint32_t sink_id,
                               std::vector<ReturnedResource> resources) {
    // We always defer ReclaimResources until the next flush, whether it's done
    // for frame acks or OnBeginFrames.
    pending_reclaimed_resources_.push_back(
        mojom::BundledReturnedResources::New(sink_id, std::move(resources)));
  }

  // BeginFrameObserver implementation:
  void OnBeginFrame(const BeginFrameArgs& args) override {
    last_used_begin_frame_args_ = args;

    // We expect the calls below to result in reentrant calls to our own
    // EnqueueOnBeginFrame(), via the sink's BundleClientProxy. In a sense this
    // means we act both as the sink's BeginFrameSource and its client. The
    // indirection is useful since the sink performs some non-trivial logic in
    // OnBeginFrame() and passes computed data to the client, which we want to
    // be able to forward in our batched OnBeginFrame notifications back to the
    // real remote client.
    defer_on_begin_frames_ = true;
    for (const auto sink_id : frame_sinks_) {
      FrameSinkId id(bundle_->id().client_id(), sink_id);
      if (BeginFrameObserver* observer = manager_->GetFrameSinkForId(id)) {
        observer->OnBeginFrame(args);
      }
    }
    defer_on_begin_frames_ = false;
    FlushMessages();
  }

  const BeginFrameArgs& LastUsedBeginFrameArgs() const override {
    return last_used_begin_frame_args_;
  }

  void OnBeginFrameSourcePausedChanged(bool paused) override {}

  bool WantsAnimateOnlyBeginFrames() const override { return false; }

  void FlushMessages() {
    std::vector<mojom::BundledReturnedResourcesPtr> pending_received_frame_acks;
    std::swap(pending_received_frame_acks_, pending_received_frame_acks);

    std::vector<mojom::BeginFrameInfoPtr> pending_on_begin_frames;
    std::swap(pending_on_begin_frames_, pending_on_begin_frames);

    std::vector<mojom::BundledReturnedResourcesPtr> pending_reclaimed_resources;
    std::swap(pending_reclaimed_resources_, pending_reclaimed_resources);

    if (!pending_received_frame_acks.empty() ||
        !pending_on_begin_frames.empty() ||
        !pending_reclaimed_resources.empty()) {
      client_->FlushNotifications(std::move(pending_received_frame_acks),
                                  std::move(pending_on_begin_frames),
                                  std::move(pending_reclaimed_resources));
    }
  }

  void DidFinishFrame() { source_->DidFinishFrame(this); }

 private:
  void UpdateBeginFrameObservation() {
    bool should_observe_begin_frame = !frame_sinks_needing_begin_frame_.empty();
    if (should_observe_begin_frame && !is_observing_begin_frame_) {
      // NOTE: It's important to set this flag before adding the observer,
      // because AddObserver() can synchronously enter CFSS::OnBeginFrame(),
      // which can in turn re-enter this method.
      is_observing_begin_frame_ = true;
      source_->AddObserver(this);
      return;
    }

    if (is_observing_begin_frame_ && !should_observe_begin_frame) {
      source_->RemoveObserver(this);
      is_observing_begin_frame_ = false;
    }
  }

  const raw_ref<FrameSinkManagerImpl> manager_;
  const raw_ref<FrameSinkBundleImpl> bundle_;
  const raw_ref<BeginFrameSource> source_;
  const raw_ref<mojom::FrameSinkBundleClient> client_;

  bool defer_on_begin_frames_ = false;
  std::vector<mojom::BundledReturnedResourcesPtr> pending_received_frame_acks_;
  std::vector<mojom::BundledReturnedResourcesPtr> pending_reclaimed_resources_;
  std::vector<mojom::BeginFrameInfoPtr> pending_on_begin_frames_;
  std::set<uint32_t> frame_sinks_;
  std::set<uint32_t> frame_sinks_needing_begin_frame_;
  bool is_observing_begin_frame_ = false;

  // Tracks which sinks in the group are still expecting an ack for a previously
  // submitted frame.
  std::set<uint32_t> unacked_submissions_;

  BeginFrameArgs last_used_begin_frame_args_;

  base::WeakPtrFactory<SinkGroup> weak_ptr_factory_{this};
};

FrameSinkBundleImpl::FrameSinkBundleImpl(
    FrameSinkManagerImpl& manager,
    const FrameSinkBundleId& id,
    mojo::PendingReceiver<mojom::FrameSinkBundle> receiver,
    mojo::PendingRemote<mojom::FrameSinkBundleClient> client)
    : manager_(manager),
      id_(id),
      receiver_(this, std::move(receiver)),
      client_(std::move(client)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &FrameSinkBundleImpl::OnDisconnect, base::Unretained(this)));
}

FrameSinkBundleImpl::~FrameSinkBundleImpl() = default;

void FrameSinkBundleImpl::SetSinkNeedsBeginFrame(uint32_t sink_id,
                                                 bool needs_begin_frame) {
  if (auto* group = GetSinkGroup(sink_id)) {
    group->SetNeedsBeginFrame(sink_id, needs_begin_frame);
  }
}

void FrameSinkBundleImpl::AddFrameSink(CompositorFrameSinkSupport* support) {
  uint32_t sink_id = support->frame_sink_id().sink_id();
  auto* source = support->begin_frame_source();
  if (!source) {
    sourceless_sinks_.insert(sink_id);
    return;
  }

  auto& group = sink_groups_[source];
  if (!group) {
    group =
        std::make_unique<SinkGroup>(*manager_, *this, *source, *client_.get());
  }
  group->AddFrameSink(sink_id);
}

void FrameSinkBundleImpl::UpdateFrameSink(CompositorFrameSinkSupport* support,
                                          BeginFrameSource* old_source) {
  uint32_t sink_id = support->frame_sink_id().sink_id();
  RemoveFrameSinkImpl(old_source, sink_id);
  AddFrameSink(support);
}

void FrameSinkBundleImpl::RemoveFrameSink(CompositorFrameSinkSupport* support) {
  auto sink_id = support->frame_sink_id().sink_id();
  auto* source = support->begin_frame_source();
  RemoveFrameSinkImpl(source, sink_id);
}

void FrameSinkBundleImpl::InitializeCompositorFrameSinkType(
    uint32_t sink_id,
    mojom::CompositorFrameSinkType type) {
  if (auto* sink = GetFrameSink(sink_id)) {
    sink->InitializeCompositorFrameSinkType(type);
  }
}

void FrameSinkBundleImpl::SetNeedsBeginFrame(uint32_t sink_id,
                                             bool needs_begin_frame) {
  if (auto* sink = GetFrameSink(sink_id)) {
    sink->SetNeedsBeginFrame(needs_begin_frame);
  }
}

void FrameSinkBundleImpl::SetWantsBeginFrameAcks(uint32_t sink_id) {
  if (auto* sink = GetFrameSink(sink_id)) {
    sink->SetWantsBeginFrameAcks();
  }
}

void FrameSinkBundleImpl::Submit(
    std::vector<mojom::BundledFrameSubmissionPtr> submissions) {
  std::map<raw_ptr<SinkGroup>, base::WeakPtr<SinkGroup>> groups;
  std::map<raw_ptr<SinkGroup>, base::WeakPtr<SinkGroup>> affected_groups;

  // Count the frame submissions before processing anything. This ensures that
  // any frames submitted here will be acked together in a batch, and not acked
  // individually in case they happen to ack synchronously within
  // SubmitCompositorFrame below.
  //
  // For sinks which can't currently be associated with a SinkGroup (because
  // they have no BeginFrameSource), we count nothing and their acks will pass
  // through to the client without batching.
  for (auto& submission : submissions) {
    if (auto* group = GetSinkGroup(submission->sink_id)) {
      groups.emplace(group, group->GetWeakPtr());
      if (submission->data->is_frame()) {
        group->WillSubmitFrame(submission->sink_id);
        affected_groups.emplace(group, group->GetWeakPtr());
      }
    }
  }

  for (auto& submission : submissions) {
    if (auto* sink = GetFrameSink(submission->sink_id)) {
      switch (submission->data->which()) {
        case mojom::BundledFrameSubmissionData::Tag::kFrame: {
          mojom::BundledCompositorFramePtr& frame =
              submission->data->get_frame();
          sink->SubmitCompositorFrame(
              frame->local_surface_id, std::move(frame->frame),
              std::move(frame->hit_test_region_list), frame->submit_time);
          break;
        }

        case mojom::BundledFrameSubmissionData::Tag::kDidNotProduceFrame:
          sink->DidNotProduceFrame(
              submission->data->get_did_not_produce_frame());
          break;

        case mojom::BundledFrameSubmissionData::Tag::kDidDeleteSharedBitmap:
          sink->DidDeleteSharedBitmap(
              submission->data->get_did_delete_shared_bitmap());
          break;
      }
    }
  }

  for (const auto& [unsafe_group, weak_group] : groups) {
    if (weak_group) {
      weak_group->DidFinishFrame();
    }
  }

  for (const auto& [unsafe_group, weak_group] : affected_groups) {
    if (weak_group) {
      weak_group->FlushMessages();
    }
  }
}

void FrameSinkBundleImpl::DidAllocateSharedBitmap(
    uint32_t sink_id,
    base::ReadOnlySharedMemoryRegion region,
    const SharedBitmapId& id) {
  if (auto* sink = GetFrameSink(sink_id)) {
    sink->DidAllocateSharedBitmap(std::move(region), id);
  }
}

#if BUILDFLAG(IS_ANDROID)
void FrameSinkBundleImpl::SetThreadIds(uint32_t sink_id,
                                       const std::vector<int32_t>& thread_ids) {
  if (auto* sink = GetFrameSink(sink_id)) {
    sink->SetThreadIds(thread_ids);
  }
}
#endif

void FrameSinkBundleImpl::EnqueueDidReceiveCompositorFrameAck(
    uint32_t sink_id,
    std::vector<ReturnedResource> resources) {
  if (auto* group = GetSinkGroup(sink_id)) {
    group->EnqueueDidReceiveCompositorFrameAck(sink_id, std::move(resources));
  } else {
    // The sink has no BeginFrameSource at the moment and therefore does not
    // belong to a SinkGroup. Forward directly without batching.
    std::vector<mojom::BundledReturnedResourcesPtr> acks;
    acks.push_back(
        mojom::BundledReturnedResources::New(sink_id, std::move(resources)));
    client_->FlushNotifications(std::move(acks), {}, {});
  }
}

void FrameSinkBundleImpl::EnqueueOnBeginFrame(
    uint32_t sink_id,
    const BeginFrameArgs& args,
    const base::flat_map<uint32_t, FrameTimingDetails>& details,
    bool frame_ack,
    std::vector<ReturnedResource> resources) {
  if (auto* group = GetSinkGroup(sink_id)) {
    group->EnqueueOnBeginFrame(sink_id, args, details, frame_ack,
                               std::move(resources));
  } else {
    // The sink has no BeginFrameSource at the moment and therefore does not
    // belong to a SinkGroup. Forward directly without batching.
    std::vector<mojom::BeginFrameInfoPtr> begin_frames;
    begin_frames.push_back(mojom::BeginFrameInfo::New(
        sink_id, args, details, frame_ack, std::move(resources)));
    client_->FlushNotifications({}, std::move(begin_frames), {});
  }
}

void FrameSinkBundleImpl::EnqueueReclaimResources(
    uint32_t sink_id,
    std::vector<ReturnedResource> resources) {
  if (auto* group = GetSinkGroup(sink_id)) {
    group->EnqueueReclaimResources(sink_id, std::move(resources));
  } else {
    // The sink has no BeginFrameSource at the moment and therefore does not
    // belong to a SinkGroup. Forward directly without batching.
    std::vector<mojom::BundledReturnedResourcesPtr> reclaims;
    reclaims.push_back(
        mojom::BundledReturnedResources::New(sink_id, std::move(resources)));
    client_->FlushNotifications({}, {}, std::move(reclaims));
  }
}

void FrameSinkBundleImpl::SendOnBeginFramePausedChanged(uint32_t sink_id,
                                                        bool paused) {
  client_->OnBeginFramePausedChanged(sink_id, paused);
}

void FrameSinkBundleImpl::SendOnCompositorFrameTransitionDirectiveProcessed(
    uint32_t sink_id,
    uint32_t sequence_id) {
  client_->OnCompositorFrameTransitionDirectiveProcessed(sink_id, sequence_id);
}

void FrameSinkBundleImpl::RemoveFrameSinkImpl(BeginFrameSource* source,
                                              uint32_t sink_id) {
  if (!source) {
    sourceless_sinks_.erase(sink_id);
    return;
  }

  auto it = sink_groups_.find(source);
  if (it == sink_groups_.end()) {
    DVLOG(1) << "Unexpected missing SinkGroup entry for sink " << sink_id;
    return;
  }

  it->second->RemoveFrameSink(sink_id);
  if (it->second->IsEmpty()) {
    sink_groups_.erase(it);
  }
}

CompositorFrameSinkImpl* FrameSinkBundleImpl::GetFrameSink(
    uint32_t sink_id) const {
  return manager_->GetFrameSinkImpl(FrameSinkId(id_.client_id(), sink_id));
}

CompositorFrameSinkSupport* FrameSinkBundleImpl::GetFrameSinkSupport(
    uint32_t sink_id) const {
  return manager_->GetFrameSinkForId(FrameSinkId(id_.client_id(), sink_id));
}

FrameSinkBundleImpl::SinkGroup* FrameSinkBundleImpl::GetSinkGroup(
    uint32_t sink_id) const {
  auto* support = GetFrameSinkSupport(sink_id);
  if (!support) {
    return nullptr;
  }

  auto* source = support->begin_frame_source();
  auto it = sink_groups_.find(source);
  if (it == sink_groups_.end()) {
    return nullptr;
  }
  return it->second.get();
}

void FrameSinkBundleImpl::OnDisconnect() {
  manager_->DestroyFrameSinkBundle(id_);
}

}  // namespace viz
