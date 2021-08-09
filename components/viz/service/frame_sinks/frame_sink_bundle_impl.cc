// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/frame_sink_bundle_impl.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_impl.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/mojom/compositing/frame_sink_bundle.mojom.h"

namespace viz {

FrameSinkBundleImpl::FrameSinkBundleImpl(
    FrameSinkManagerImpl& manager,
    const FrameSinkBundleId& id,
    BeginFrameSource* begin_frame_source,
    mojo::PendingReceiver<mojom::FrameSinkBundle> receiver,
    mojo::PendingRemote<mojom::FrameSinkBundleClient> client)
    : manager_(manager),
      id_(id),
      begin_frame_source_(begin_frame_source),
      receiver_(this, std::move(receiver)),
      client_(std::move(client)) {
  DCHECK(begin_frame_source_);
  begin_frame_source_->AddObserver(this);

  receiver_.set_disconnect_handler(base::BindOnce(
      &FrameSinkBundleImpl::OnDisconnect, base::Unretained(this)));
}

FrameSinkBundleImpl::~FrameSinkBundleImpl() {
  if (begin_frame_source_) {
    begin_frame_source_->RemoveObserver(this);
  }
}

void FrameSinkBundleImpl::AddFrameSink(const FrameSinkId& id) {
  frame_sinks_.insert(id.sink_id());
}

void FrameSinkBundleImpl::RemoveFrameSink(const FrameSinkId& id) {
  frame_sinks_.erase(id.sink_id());
}

void FrameSinkBundleImpl::InitializeCompositorFrameSinkType(
    uint32_t sink_id,
    mojom::CompositorFrameSinkType type) {
  if (!sink_type_.has_value()) {
    sink_type_ = type;
  } else if (type != *sink_type_) {
    receiver_.ReportBadMessage("Bundled frame sinks must be of the same type");
    return;
  }

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

void FrameSinkBundleImpl::Submit(
    std::vector<mojom::BundledFrameSubmissionPtr> submissions) {
  // Count the frame submissions before processing anything. This ensures that
  // any frames submitted here will be acked together in a batch, and not acked
  // individually in case they happen to ack synchronously within
  // SubmitCompositorFrame below.
  for (auto& submission : submissions) {
    if (GetFrameSink(submission->sink_id) && submission->data->is_frame()) {
      ++num_unacked_submissions_;
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
  FlushMessages();
}

void FrameSinkBundleImpl::DidAllocateSharedBitmap(
    uint32_t sink_id,
    base::ReadOnlySharedMemoryRegion region,
    const gpu::Mailbox& id) {
  if (auto* sink = GetFrameSink(sink_id)) {
    sink->DidAllocateSharedBitmap(std::move(region), id);
  }
}

void FrameSinkBundleImpl::EnqueueDidReceiveCompositorFrameAck(
    uint32_t sink_id,
    std::vector<ReturnedResource> resources) {
  pending_received_frame_acks_.push_back(
      mojom::BundledReturnedResources::New(sink_id, std::move(resources)));

  // We expect to be notified about the consumption of all previously submitted
  // frames at approximately the same time. This condition allows us to batch
  // most ack messages without any significant delays. Note that
  // `num_unacked_submissions_` is incremented in Submit() exactly once for
  // every actual frame submitted by clients within the bundle, and is
  // decremented only when acks are flushed by FlushMessages().
  if (pending_received_frame_acks_.size() >= num_unacked_submissions_) {
    FlushMessages();
  }
}

void FrameSinkBundleImpl::EnqueueOnBeginFrame(
    uint32_t sink_id,
    const BeginFrameArgs& args,
    const base::flat_map<uint32_t, FrameTimingDetails>& details) {
  pending_on_begin_frames_.push_back(
      mojom::BeginFrameInfo::New(sink_id, args, details));
  if (!defer_on_begin_frames_) {
    FlushMessages();
  }
}

void FrameSinkBundleImpl::EnqueueReclaimResources(
    uint32_t sink_id,
    std::vector<ReturnedResource> resources) {
  // We always defer ReclaimResources until the next flush, whether it's done
  // for frame acks or OnBeginFrames.
  pending_reclaimed_resources_.push_back(
      mojom::BundledReturnedResources::New(sink_id, std::move(resources)));
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

void FrameSinkBundleImpl::UnregisterBeginFrameSource(BeginFrameSource* source) {
  if (begin_frame_source_ == source) {
    begin_frame_source_->RemoveObserver(this);
    begin_frame_source_ = nullptr;
  }
}

CompositorFrameSinkImpl* FrameSinkBundleImpl::GetFrameSink(
    uint32_t sink_id) const {
  return manager_.GetFrameSinkImpl(FrameSinkId(id_.client_id(), sink_id));
}

void FrameSinkBundleImpl::FlushMessages() {
  std::vector<mojom::BundledReturnedResourcesPtr> pending_received_frame_acks;
  std::swap(pending_received_frame_acks_, pending_received_frame_acks);

  DCHECK_GE(num_unacked_submissions_, pending_received_frame_acks.size());
  num_unacked_submissions_ -= pending_received_frame_acks.size();

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

void FrameSinkBundleImpl::OnDisconnect() {
  manager_.DestroyFrameSinkBundle(id_);
}

void FrameSinkBundleImpl::OnBeginFrame(const BeginFrameArgs& args) {
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
    FrameSinkId id(id_.client_id(), sink_id);
    if (BeginFrameObserver* observer = manager_.GetFrameSinkForId(id)) {
      observer->OnBeginFrame(args);
    }
  }
  defer_on_begin_frames_ = false;
  FlushMessages();
}

const BeginFrameArgs& FrameSinkBundleImpl::LastUsedBeginFrameArgs() const {
  return last_used_begin_frame_args_;
}

void FrameSinkBundleImpl::OnBeginFrameSourcePausedChanged(bool paused) {}

bool FrameSinkBundleImpl::WantsAnimateOnlyBeginFrames() const {
  return false;
}

}  // namespace viz
