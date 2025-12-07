// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/compositor_frame_sink_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/service/frame_sinks/frame_sink_bundle_impl.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "ui/gfx/overlay_transform.h"

namespace viz {

namespace {

// Helper class which implements the CompositorFrameSinkClient interface so it
// can route CompositorFrameSinkSupport client messages to a local
// FrameSinkBundleImpl for batching, rather than having them go directly to the
// remote client.
class BundleClientProxy : public mojom::CompositorFrameSinkClient {
 public:
  BundleClientProxy(FrameSinkManagerImpl& manager,
                    FrameSinkId frame_sink_id,
                    FrameSinkBundleId bundle_id)
      : manager_(manager),
        frame_sink_id_(frame_sink_id),
        bundle_id_(bundle_id) {}

  BundleClientProxy(const BundleClientProxy&) = delete;
  BundleClientProxy& operator=(const BundleClientProxy&) = delete;
  ~BundleClientProxy() override = default;

  // mojom::CompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck(
      std::vector<ReturnedResource> resources) override {
    if (auto* bundle = GetBundle()) {
      bundle->EnqueueDidReceiveCompositorFrameAck(frame_sink_id_.sink_id(),
                                                  std::move(resources));
    }
  }

  void OnBeginFrame(const BeginFrameArgs& args,
                    const FrameTimingDetailsMap& timing_details,
                    std::vector<ReturnedResource> resources) override {
    if (auto* bundle = GetBundle()) {
      bundle->EnqueueOnBeginFrame(frame_sink_id_.sink_id(), args,
                                  timing_details, std::move(resources));
    }
  }

  void ReclaimResources(std::vector<ReturnedResource> resources) override {
    if (auto* bundle = GetBundle()) {
      bundle->EnqueueReclaimResources(frame_sink_id_.sink_id(),
                                      std::move(resources));
    }
  }

  void OnBeginFramePausedChanged(bool paused) override {
    if (auto* bundle = GetBundle()) {
      bundle->SendOnBeginFramePausedChanged(frame_sink_id_.sink_id(), paused);
    }
  }

  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) override {
    if (auto* bundle = GetBundle()) {
      bundle->SendOnCompositorFrameTransitionDirectiveProcessed(
          frame_sink_id_.sink_id(), sequence_id);
    }
  }

  void OnSurfaceEvicted(const LocalSurfaceId& local_surface_id) override {}

 private:
  FrameSinkBundleImpl* GetBundle() {
    return manager_->GetFrameSinkBundle(bundle_id_);
  }

  const raw_ref<FrameSinkManagerImpl> manager_;
  const FrameSinkId frame_sink_id_;
  const FrameSinkBundleId bundle_id_;
};

}  // namespace

CompositorFrameSinkImpl::CompositorFrameSinkImpl(
    FrameSinkManagerImpl* frame_sink_manager,
    const FrameSinkId& frame_sink_id,
    std::optional<FrameSinkBundleId> bundle_id,
    mojo::PendingReceiver<mojom::CompositorFrameSink> interface_receiver,
    mojo::PendingRemote<mojom::CompositorFrameSinkClient> client)
    : compositor_frame_sink_client_(std::move(client)),
      proxying_client_(
          bundle_id.has_value()
              ? std::make_unique<BundleClientProxy>(*frame_sink_manager,
                                                    frame_sink_id,
                                                    *bundle_id)
              : nullptr),
      compositor_frame_sink_receiver_(std::in_place_type<Receiver>, this),
      support_(std::make_unique<CompositorFrameSinkSupport>(
          proxying_client_ ? proxying_client_.get()
                           : compositor_frame_sink_client_.get(),
          frame_sink_manager,
          frame_sink_id,
          false /* is_root */)) {
  if (mojo::IsDirectReceiverSupported() &&
      features::IsVizDirectCompositorThreadIpcNonRootEnabled()) {
    compositor_frame_sink_receiver_.emplace<DirectReceiver>(
        mojo::DirectReceiverKey{}, this);
  }
  std::visit(
      [&](auto& receiver) {
        receiver.Bind(std::move(interface_receiver));
        receiver.set_disconnect_handler(
            base::BindOnce(&CompositorFrameSinkImpl::OnClientConnectionLost,
                           base::Unretained(this)));
      },
      compositor_frame_sink_receiver_);
  if (bundle_id.has_value()) {
    support_->SetBundle(*bundle_id);
  }
}

CompositorFrameSinkImpl::~CompositorFrameSinkImpl() = default;

void CompositorFrameSinkImpl::SetNeedsBeginFrame(bool needs_begin_frame) {
  support_->SetNeedsBeginFrame(needs_begin_frame);
}

void CompositorFrameSinkImpl::SetParams(
    mojom::CompositorFrameSinkParamsPtr params) {
  DCHECK(!params_set_ && !support_->last_created_surface_id().is_valid());
  params_set_ = true;
  if (params->wants_animate_only_begin_frames) {
    support_->SetWantsAnimateOnlyBeginFrames();
  }
  if (params->auto_needs_begin_frame) {
    support_->SetAutoNeedsBeginFrame();
  }
  if (params->no_compositor_frame_acks) {
    support_->SetNoCompositorFrameAcks();
  }
}

void CompositorFrameSinkImpl::SubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame,
    std::optional<HitTestRegionList> hit_test_region_list,
    uint64_t submit_time) {
  // Non-root surface frames should not have display transform hint.
  DCHECK_EQ(gfx::OVERLAY_TRANSFORM_NONE, frame.metadata.display_transform_hint);

  const auto result = support_->MaybeSubmitCompositorFrame(
      local_surface_id, std::move(frame), std::move(hit_test_region_list),
      submit_time);
  if (result == SubmitResult::ACCEPTED)
    return;

  const char* reason =
      CompositorFrameSinkSupport::GetSubmitResultAsString(result);
  DLOG(ERROR) << "SubmitCompositorFrame failed for " << local_surface_id
              << " because " << reason;

  std::visit(
      [&](auto& receiver) {
        receiver.ResetWithReason(static_cast<uint32_t>(result), reason);
      },
      compositor_frame_sink_receiver_);
}

void CompositorFrameSinkImpl::DidNotProduceFrame(
    const BeginFrameAck& begin_frame_ack) {
  support_->DidNotProduceFrame(begin_frame_ack);
}

void CompositorFrameSinkImpl::NotifyNewLocalSurfaceIdExpectedWhilePaused() {
  support_->NotifyNewLocalSurfaceIdExpectedWhilePaused();
}

void CompositorFrameSinkImpl::BindLayerContext(
    mojom::PendingLayerContextPtr context,
    mojom::LayerContextSettingsPtr settings) {
  support_->BindLayerContext(*context, std::move(settings));
}

#if BUILDFLAG(IS_ANDROID)
void CompositorFrameSinkImpl::SetThreads(const std::vector<Thread>& threads) {
  support_->SetThreads(/*from_untrusted_client=*/true, threads);
}
#endif

void CompositorFrameSinkImpl::OnClientConnectionLost() {
  // The client that owns this CompositorFrameSink is either shutting down or
  // has done something invalid and the connection to the client was terminated.
  // Destroy |this| to free up resources as it's no longer useful.
  FrameSinkId frame_sink_id = support_->frame_sink_id();
  support_->frame_sink_manager()->DestroyCompositorFrameSink(frame_sink_id,
                                                             base::DoNothing());
}

}  // namespace viz
