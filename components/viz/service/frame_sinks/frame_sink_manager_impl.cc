// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/containers/queue.h"
#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/input/utils.h"
#include "components/viz/common/features.h"
#include "components/viz/common/performance_hint_utils.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/common/surfaces/video_capture_target.h"
#include "components/viz/service/display/overdraw_tracker.h"
#include "components/viz/service/display_embedder/output_surface_provider.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_bundle_impl.h"
#include "components/viz/service/frame_sinks/shared_image_interface_provider.h"
#include "components/viz/service/frame_sinks/video_capture/capturable_frame_sink.h"
#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_impl.h"
#include "components/viz/service/input/input_manager.h"
#include "components/viz/service/performance_hint/hint_session.h"
#include "components/viz/service/surfaces/pending_copy_output_request.h"
#include "components/viz/service/surfaces/surface.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

#if BUILDFLAG(IS_MAC)
#include "components/viz/service/frame_sinks/external_begin_frame_source_mojo_mac.h"
#include "ui/display/display_features.h"
#include "ui/display/mac/vsync_provider_mac.h"
#endif

namespace viz {

FrameSinkManagerImpl::InitParams::InitParams(
    OutputSurfaceProvider* output_surface_provider,
    GmbVideoFramePoolContextProvider* gmb_context_provider)
    : output_surface_provider(output_surface_provider),
      gmb_context_provider(gmb_context_provider) {}
FrameSinkManagerImpl::InitParams::InitParams(InitParams&& other) = default;
FrameSinkManagerImpl::InitParams::~InitParams() = default;
FrameSinkManagerImpl::InitParams& FrameSinkManagerImpl::InitParams::operator=(
    InitParams&& other) = default;

FrameSinkManagerImpl::FrameSinkSourceMapping::FrameSinkSourceMapping() =
    default;

FrameSinkManagerImpl::FrameSinkSourceMapping::FrameSinkSourceMapping(
    FrameSinkSourceMapping&& other) = default;

FrameSinkManagerImpl::FrameSinkSourceMapping::~FrameSinkSourceMapping() =
    default;

FrameSinkManagerImpl::FrameSinkSourceMapping&
FrameSinkManagerImpl::FrameSinkSourceMapping::operator=(
    FrameSinkSourceMapping&& other) = default;

FrameSinkManagerImpl::FrameSinkData::FrameSinkData(bool report_activation)
    : report_activation(report_activation) {}

FrameSinkManagerImpl::FrameSinkData::FrameSinkData(FrameSinkData&& other) =
    default;
FrameSinkManagerImpl::FrameSinkData::~FrameSinkData() = default;
FrameSinkManagerImpl::FrameSinkData& FrameSinkManagerImpl::FrameSinkData::
operator=(FrameSinkData&& other) = default;

FrameSinkManagerImpl::FrameSinkManagerImpl(const InitParams& params)
    : output_surface_provider_(params.output_surface_provider),
      gpu_service_(params.gpu_service),
      gmb_context_provider_(params.gmb_context_provider),
      surface_manager_(this,
                       params.activation_deadline_in_frames,
                       params.max_uncommitted_frames),
      hit_test_manager_(surface_manager()),
      restart_id_(params.restart_id),
      run_all_compositor_stages_before_draw_(
          params.run_all_compositor_stages_before_draw),
      log_capture_pipeline_in_webrtc_(params.log_capture_pipeline_in_webrtc),
      debug_settings_(params.debug_renderer_settings),
      host_process_id_(params.host_process_id),
      hint_session_factory_(params.hint_session_factory),
      frame_sink_manager_receiver_(std::in_place_type<Receiver>, this) {
  if (mojo::IsDirectReceiverSupported() && mojo::IsAsyncIOSupported() &&
      features::IsVizDirectCompositorThreadIpcFrameSinkManagerEnabled()) {
    frame_sink_manager_receiver_.emplace<DirectReceiver>(
        mojo::DirectReceiverKey{}, this);
  }
  surface_manager_.AddObserver(&hit_test_manager_);
  surface_manager_.AddObserver(this);

  if (input::InputUtils::IsTransferInputToVizSupported()) {
    input_manager_ = std::make_unique<InputManager>(this);
  }

#if BUILDFLAG(IS_MAC)
  // The VSyncProviderMac must execute on the Viz thread.
  // As VSyncProviderMac::GetInstance() can be invoked from either the Viz
  // thread or the GPU thread, it is called here to ensure the Viz task runner
  // is saved for VSyncProviderMac.
  if (ui::DisplayLinkMac::SupportsDisplayLinkMacInBrowser()) {
    ui::VSyncProviderMac::GetInstance();
  }
#endif
}

FrameSinkManagerImpl::~FrameSinkManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  video_capturers_.clear();

  // All mojom::CompositorFrameSinks and BeginFrameSources should be deleted by
  // this point.
  DCHECK(sink_map_.empty());
  DCHECK(root_sink_map_.empty());
#if BUILDFLAG(IS_ANDROID)
  DCHECK(cached_back_buffers_.empty());
#endif
  DCHECK(registered_sources_.empty());

  surface_manager_.RemoveObserver(this);
  surface_manager_.RemoveObserver(&hit_test_manager_);
}

CompositorFrameSinkImpl* FrameSinkManagerImpl::GetFrameSinkImpl(
    const FrameSinkId& id) {
  return base::FindPtrOrNull(sink_map_, id);
}

FrameSinkBundleImpl* FrameSinkManagerImpl::GetFrameSinkBundle(
    const FrameSinkBundleId& id) {
  return base::FindPtrOrNull(bundle_map_, id);
}

void FrameSinkManagerImpl::BindAndSetClient(
    mojo::PendingReceiver<mojom::FrameSinkManager> interface_receiver,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingRemote<mojom::FrameSinkManagerClient> client,
    SharedImageInterfaceProvider* shared_image_interface_provider) {
  DCHECK(!client_);
  DCHECK(shared_image_interface_provider);
  shared_image_interface_provider_ = shared_image_interface_provider;

  std::visit(absl::Overload{[&](Receiver& receiver) {
                              receiver.Bind(std::move(interface_receiver),
                                            task_runner);
                            },
                            [&](DirectReceiver& receiver) {
                              receiver.Bind(std::move(interface_receiver));
                            }},
             frame_sink_manager_receiver_);
  client_remote_.Bind(std::move(client));
  client_ = client_remote_.get();

  if (client_ && input_manager_) {
    base::ReadOnlySharedMemoryRegion region =
        input_manager_->DuplicateVizTouchStateRegion();
    if (region.IsValid()) {
      // Send via the Mojo interface implemented by HostFrameSinkManager.
      client_->OnVizTouchStateAvailable(std::move(region));
    }
  }
}

void FrameSinkManagerImpl::SetLocalClient(
    mojom::FrameSinkManagerClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  DCHECK(!client_remote_);
  DCHECK(!ui_task_runner_);
  client_ = client;
  ui_task_runner_ = ui_task_runner;
}

void FrameSinkManagerImpl::SetInputManagerForTesting(
    std::unique_ptr<InputManager> input_manager) {
  if (!input::InputUtils::IsTransferInputToVizSupported()) {
    return;
  }

  input_manager_ = std::move(input_manager);
}

void FrameSinkManagerImpl::RegisterFrameSinkId(const FrameSinkId& frame_sink_id,
                                               bool report_activation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!frame_sink_data_.contains(frame_sink_id));

  frame_sink_data_.emplace(std::make_pair(frame_sink_id, report_activation));

  if (video_detector_)
    video_detector_->OnFrameSinkIdRegistered(frame_sink_id);
}

void FrameSinkManagerImpl::InvalidateFrameSinkId(
    const FrameSinkId& frame_sink_id,
    InvalidateFrameSinkIdCallback callback) {
  TRACE_EVENT("viz", "FrameSinkManagerImpl::InvalidateFrameSinkId",
              "frame_sink_id", frame_sink_id);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ScopedClosureRunner callback_runner(std::move(callback));

  surface_manager_.InvalidateFrameSinkId(frame_sink_id);
  if (video_detector_)
    video_detector_->OnFrameSinkIdInvalidated(frame_sink_id);

  MaybeEraseHitTestQuery(frame_sink_id);

  // Destroy the [Root]CompositorFrameSinkImpl if there is one.
  sink_map_.erase(frame_sink_id);
  root_sink_map_.erase(frame_sink_id);

  frame_sink_data_.erase(frame_sink_id);
}

void FrameSinkManagerImpl::SetFrameSinkDebugLabel(
    const FrameSinkId& frame_sink_id,
    const std::string& debug_label) {
  FrameSinkData* frame_sink_data =
      base::FindOrNull(frame_sink_data_, frame_sink_id);
  if (frame_sink_data) {
    frame_sink_data->debug_label = debug_label;
    if (frame_counter_) {
      frame_counter_->SetFrameSinkDebugLabel(frame_sink_id, debug_label);
    }
  }
}

void FrameSinkManagerImpl::CreateRootCompositorFrameSink(
    mojom::RootCompositorFrameSinkParamsPtr params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!root_sink_map_.contains(params->frame_sink_id));
  DCHECK(output_surface_provider_);

  // We are transferring ownership of |params| so remember FrameSinkId here.
  FrameSinkId frame_sink_id = params->frame_sink_id;

  if (root_sink_map_.empty()) {
    root_frame_sink_id_ = frame_sink_id;
  }

  bool create_input_receiver = false;
#if BUILDFLAG(IS_ANDROID)
  create_input_receiver = params->create_input_receiver;
#endif
  gpu::SurfaceHandle widget = params->widget;

  // Creating RootCompositorFrameSinkImpl can fail and return null.
  auto root_compositor_frame_sink = RootCompositorFrameSinkImpl::Create(
      std::move(params), this, output_surface_provider_, restart_id_,
      run_all_compositor_stages_before_draw_, &debug_settings_,
      hint_session_factory_);

  if (root_compositor_frame_sink) {
    root_sink_map_[frame_sink_id] = std::move(root_compositor_frame_sink);
    if (GetInputManager()) {
      GetInputManager()->OnCreateCompositorFrameSink(
          frame_sink_id,
          /*is_root=*/true,
          /*render_input_router_config=*/nullptr, create_input_receiver,
          widget);
    }
  }

  MaybeAddHitTestQuery(frame_sink_id);
}

#if BUILDFLAG(IS_MAC)
void FrameSinkManagerImpl::CreateCompositorDisplayLink(
    mojom::CompositorDisplayLinkParamsPtr params) {
  auto update_vsync_displays_cb = base::BindRepeating(
      &FrameSinkManagerImpl::UpdateVSyncDisplays, weak_factory_.GetWeakPtr());

  external_begin_frame_source_ =
      std::make_unique<ExternalBeginFrameSourceMojoMac>(
          std::move(params->external_begin_frame_controller),
          std::move(params->external_begin_frame_controller_client),
          update_vsync_displays_cb);
}

void FrameSinkManagerImpl::UpdateVSyncDisplays() {
  for (auto& root_frame_sink : root_sink_map_) {
    if (root_frame_sink.second->external_begin_frame_source()) {
      root_frame_sink.second->external_begin_frame_source()
          ->UpdateVSyncDisplay();
    }
  }
}

#endif

void FrameSinkManagerImpl::CreateFrameSinkBundle(
    const FrameSinkBundleId& bundle_id,
    mojo::PendingReceiver<mojom::FrameSinkBundle> receiver,
    mojo::PendingRemote<mojom::FrameSinkBundleClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (bundle_map_.contains(bundle_id)) {
    uint32_t client_id = bundle_id.client_id();
    uint32_t bundle_id_value = bundle_id.bundle_id();
    std::visit(
        [](auto& receiver) {
          receiver.ReportBadMessage("Duplicate FrameSinkBundle ID");
        },
        frame_sink_manager_receiver_);
    base::debug::Alias(&client_id);
    base::debug::Alias(&bundle_id_value);
    return;
  }

  bundle_map_[bundle_id] = std::make_unique<FrameSinkBundleImpl>(
      *this, bundle_id, std::move(receiver), std::move(client));
}

void FrameSinkManagerImpl::CreateCompositorFrameSink(
    const FrameSinkId& frame_sink_id,
    const std::optional<FrameSinkBundleId>& bundle_id,
    mojo::PendingReceiver<mojom::CompositorFrameSink> receiver,
    mojo::PendingRemote<mojom::CompositorFrameSinkClient> client,
    input::mojom::RenderInputRouterConfigPtr render_input_router_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT("viz", "FrameSinkManagerImpl::CreateCompositorFrameSink",
              "frame_sink_id", frame_sink_id);
  if (sink_map_.contains(frame_sink_id)) {
    std::visit(
        [](auto& receiver) {
          receiver.ReportBadMessage("Duplicate FrameSinkId");
        },
        frame_sink_manager_receiver_);
    return;
  }
  if (bundle_id && !GetFrameSinkBundle(*bundle_id)) {
    VLOG(1) << "Terminating sink established with non-existent bundle";
    return;
  }

  sink_map_[frame_sink_id] = std::make_unique<CompositorFrameSinkImpl>(
      this, frame_sink_id, bundle_id, std::move(receiver), std::move(client));

  if (GetInputManager()) {
    GetInputManager()->OnCreateCompositorFrameSink(
        frame_sink_id,
        /*is_root=*/false, std::move(render_input_router_config),
        /*create_input_receiver=*/false, gpu::SurfaceHandle());
    // Set BeginFrameSource here since RenderInputRouter associated with
    // |frame_sink_id| would've been created by now.
    FrameSinkSourceMapping* mapping =
        base::FindOrNull(frame_sink_source_map_, frame_sink_id);
    if (mapping && mapping->source) {
      GetInputManager()->SetBeginFrameSource(frame_sink_id, mapping->source);
    }
  }
}

void FrameSinkManagerImpl::DestroyCompositorFrameSink(
    const FrameSinkId& frame_sink_id,
    DestroyCompositorFrameSinkCallback callback) {
  sink_map_.erase(frame_sink_id);
  root_sink_map_.erase(frame_sink_id);
  std::move(callback).Run();
}

void FrameSinkManagerImpl::RegisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  // If it's possible to reach the parent through the child's descendant chain,
  // then this will create an infinite loop.  Might as well just crash here.
  CHECK(!ChildContains(child_frame_sink_id, parent_frame_sink_id));

  auto& children = frame_sink_source_map_[parent_frame_sink_id].children;
  DCHECK(!children.contains(child_frame_sink_id));
  children.insert(child_frame_sink_id);

  // Add `parent_frame_sink_id` as parent to the list tracking parents of
  // `child_frame_sink_id`.
  FrameSinkSourceMapping& mapping = frame_sink_source_map_[child_frame_sink_id];
  mapping.parent.emplace_back(parent_frame_sink_id);

  // Now the hierarchy has been updated, update throttling.
  UpdateThrottling();

  for (auto& observer : observer_list_) {
    observer.OnRegisteredFrameSinkHierarchy(parent_frame_sink_id,
                                            child_frame_sink_id);
  }

  // If the parent has no source, then attaching it to this child will
  // not change any downstream sources.
  BeginFrameSource* parent_source =
      frame_sink_source_map_[parent_frame_sink_id].source;
  if (!parent_source)
    return;

  DCHECK_EQ(registered_sources_.count(parent_source), 1u);
  RecursivelyAttachBeginFrameSource(child_frame_sink_id, parent_source);
}

void FrameSinkManagerImpl::UnregisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  TRACE_EVENT("viz", "FrameSinkManagerImpl::UnregisterFrameSinkHierarchy",
              "parent_frame_sink_id", parent_frame_sink_id,
              "child_frame_sink_id", child_frame_sink_id);
  // Deliberately do not check validity of either parent or child FrameSinkId
  // here. They were valid during the registration, so were valid at some point
  // in time. This makes it possible to invalidate parent and child FrameSinkIds
  // independently of each other and not have an ordering dependency of
  // unregistering the hierarchy first before either of them.

  auto iter_child = frame_sink_source_map_.find(child_frame_sink_id);
  CHECK(iter_child != frame_sink_source_map_.end());

  auto& child_mapping = iter_child->second;
  DCHECK(std::ranges::contains(child_mapping.parent, parent_frame_sink_id));

  // Delete `parent_frame_sink_id` from parent list of `child_frame_sink_id` in
  // `frame_sink_source_map_`.
  auto iter_find_parent =
      std::find(child_mapping.parent.begin(), child_mapping.parent.end(),
                parent_frame_sink_id);
  child_mapping.parent.erase(iter_find_parent);

  // Delete `child_frame_sink_id` entry from `frame_sink_source_map_` if empty.
  if (child_mapping.children.empty() && child_mapping.parent.empty() &&
      !child_mapping.source) {
    frame_sink_source_map_.erase(iter_child);
  }

  auto iter_parent = frame_sink_source_map_.find(parent_frame_sink_id);
  CHECK(iter_parent != frame_sink_source_map_.end());

  // Remove |child_frame_sink_id| from parents list of children.
  auto& mapping = iter_parent->second;
  DCHECK(mapping.children.contains(child_frame_sink_id));
  mapping.children.erase(child_frame_sink_id);

  for (auto& observer : observer_list_) {
    observer.OnUnregisteredFrameSinkHierarchy(parent_frame_sink_id,
                                              child_frame_sink_id);
  }

  // Now the hierarchy has been updated, update throttling.
  UpdateThrottling();

  // Delete the FrameSinkSourceMapping for |parent_frame_sink_id| if empty.
  if (mapping.children.empty() && mapping.parent.empty() && !mapping.source) {
    frame_sink_source_map_.erase(iter_parent);
    return;
  }

  // If the parent does not have a begin frame source, then disconnecting it
  // will not change any of its children.
  BeginFrameSource* parent_source = iter_parent->second.source;
  if (!parent_source)
    return;

  // TODO(enne): these walks could be done in one step.
  RecursivelyDetachBeginFrameSource(child_frame_sink_id, parent_source);
  for (auto& source_iter : registered_sources_)
    RecursivelyAttachBeginFrameSource(source_iter.second, source_iter.first);
}

void FrameSinkManagerImpl::AddVideoDetectorObserver(
    mojo::PendingRemote<mojom::VideoDetectorObserver> observer) {
  if (!video_detector_) {
    video_detector_ = std::make_unique<VideoDetector>(
        GetRegisteredFrameSinkIds(), &surface_manager_);
  }
  video_detector_->AddObserver(std::move(observer));
}

void FrameSinkManagerImpl::CreateVideoCapturer(
    mojo::PendingReceiver<mojom::FrameSinkVideoCapturer> receiver,
    uint32_t capture_version_source) {
  video_capturers_.emplace(std::make_unique<FrameSinkVideoCapturerImpl>(
      *this, gmb_context_provider_, std::move(receiver),
      std::make_unique<media::VideoCaptureOracle>(
          true /* enable_auto_throttling */),
      log_capture_pipeline_in_webrtc_, capture_version_source));
}

void FrameSinkManagerImpl::EvictSurfaces(
    const std::vector<SurfaceId>& surface_ids) {
  for (const SurfaceId& surface_id : surface_ids) {
    CompositorFrameSinkSupport* support =
        base::FindPtrOrNull(support_map_, surface_id.frame_sink_id());
    if (!support) {
      continue;
    }

    // Even if we try to evict the root surface, it won't actually be freed up
    // since various parts of the graphics stack will keep references to its
    // resources. If we need to support evicting the root surface, we can revert
    // crrev.com/c/6312283.
    support->EvictSurface(surface_id.local_surface_id());

    if (!support->is_root()) {
      continue;
    }
    RootCompositorFrameSinkImpl* root =
        base::FindPtrOrNull(root_sink_map_, surface_id.frame_sink_id());
    if (root) {
      root->DidEvictSurface(surface_id);
    }
  }

  // Trigger garbage collection immediately, otherwise the surface may not be
  // evicted for a long time (e.g. not before a frame is produced).
  surface_manager_.GarbageCollectSurfaces();
}

void FrameSinkManagerImpl::RequestCopyOfOutput(
    const SurfaceId& surface_id,
    std::unique_ptr<CopyOutputRequest> request,
    bool capture_exact_surface_id,
    base::TimeDelta timeout) {
  TRACE_EVENT0("viz", "FrameSinkManagerImpl::RequestCopyOfOutput");
  auto pending_request = std::make_unique<PendingCopyOutputRequest>(
      surface_id.local_surface_id(), SubtreeCaptureId(), std::move(request),
      capture_exact_surface_id, timeout);
  // The exact request can be picked up by the targeted surface right away,
  // instead of being queued up in the `CompositorFrameSinkSupport`. In some
  // cases (e.g., a request issued against the old surface after the old
  // renderer tearing down the frame sink) when the request arrives the frame
  // sink is already unregistered, but the targeted surface is still kept alive.
  if (capture_exact_surface_id) {
    auto* exact_surface = surface_manager_.GetSurfaceForId(surface_id);
    if (exact_surface) {
      exact_surface->RequestCopyOfOutput(std::move(pending_request));

      BeginFrameAck ack;
      ack.has_damage = true;
      surface_manager_.SurfaceModified(
          surface_id, ack, SurfaceObserver::HandleInteraction::kNoChange);
      return;
    }
  }

  // For the exact request yet to have a surface, or the non-exact request,
  // queue them up in the matching `CompositorFrameSinkSupport`.
  CompositorFrameSinkSupport* support =
      base::FindPtrOrNull(support_map_, surface_id.frame_sink_id());
  if (!support) {
    if (capture_exact_surface_id) {
      // It is extremely rare for the browser to issue a copy request against
      // its embedded `SurfaceId` before the surface exists (submitting a
      // request before the GPU draws anything) or before the frame sink exists
      // (submitting a request before the renderer loads the document). We don't
      // want to crash the GPU in either cases. The ERROR log shows up in
      // "chrome://gpu".
      LOG(ERROR) << "The browser issued an exact CopyOutputRequest for "
                 << surface_id
                 << " but there is no such surface or a frame sink.";
    }
    // `pending_request` will send an empty result when it goes out of scope.
    return;
  }

  support->RequestCopyOfOutput(std::move(pending_request));
}

void FrameSinkManagerImpl::DestroyFrameSinkBundle(const FrameSinkBundleId& id) {
  bundle_map_.erase(id);
}

void FrameSinkManagerImpl::OnFirstSurfaceActivation(
    const SurfaceInfo& surface_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(surface_info.device_scale_factor(), 0.0f);

  const auto* frame_sink_data =
      base::FindOrNull(frame_sink_data_, surface_info.id().frame_sink_id());

  if (frame_sink_data && client_ && frame_sink_data->report_activation) {
    client_->OnFirstSurfaceActivation(surface_info);
  }
}

void FrameSinkManagerImpl::UpdateHitTestRegionData(
    const FrameSinkId& frame_sink_id,
    const std::vector<AggregatedHitTestRegion>& hit_test_data) {
  if (!display_hit_test_query_.contains(frame_sink_id)) {
    // The corresponding HitTestQuery has already been deleted, so drop the
    // in-flight hit-test data.
    return;
  }

  // Notify observers of the updated hit test data.
  for (HitTestRegionObserver& observer : hit_test_region_observers_) {
    observer.OnAggregatedHitTestRegionListUpdated(frame_sink_id, hit_test_data);
  }
}

void FrameSinkManagerImpl::OnAggregatedHitTestRegionListUpdated(
    const FrameSinkId& frame_sink_id,
    const std::vector<AggregatedHitTestRegion>& hit_test_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UpdateHitTestRegionData(frame_sink_id, hit_test_data);

  if (client_) {
    client_->OnAggregatedHitTestRegionListUpdated(frame_sink_id, hit_test_data);
  }
}

std::string_view FrameSinkManagerImpl::GetFrameSinkDebugLabel(
    const FrameSinkId& frame_sink_id) const {
  auto* data = base::FindOrNull(frame_sink_data_, frame_sink_id);
  return data ? data->debug_label : std::string_view();
}

void FrameSinkManagerImpl::AggregatedFrameSinksChanged() {
  hit_test_manager_.SetNeedsSubmit();
}

void FrameSinkManagerImpl::RegisterSameDocViewTransitionToken(
    const blink::ViewTransitionToken& token) {
  same_doc_tokens_pending_.insert(token);
}

void FrameSinkManagerImpl::MarkSameDocViewTransitionTokenReady(
    const blink::ViewTransitionToken& token) {
  if (!same_doc_tokens_pending_.erase(token)) {
    return;
  }

  same_doc_tokens_ready_.insert(token);

  // This is what Surface waits on to resolve view_transition_dependencies_.
  for (auto& observer : observer_list_) {
    observer.OnViewTransitionSaved(token);
  }
}

void FrameSinkManagerImpl::ClearSameDocViewTransitionToken(
    const blink::ViewTransitionToken& token) {
  same_doc_tokens_pending_.erase(token);
  same_doc_tokens_ready_.erase(token);
}

bool FrameSinkManagerImpl::HasViewTransitionToken(
    const blink::ViewTransitionToken& transition_token) {
  return transition_token_to_animation_manager_.contains(transition_token) ||
         same_doc_tokens_ready_.contains(transition_token);
}

void FrameSinkManagerImpl::AddHitTestRegionObserver(
    HitTestRegionObserver* observer) {
  hit_test_region_observers_.AddObserver(observer);
}

void FrameSinkManagerImpl::RemoveHitTestRegionObserver(
    HitTestRegionObserver* observer) {
  hit_test_region_observers_.RemoveObserver(observer);
}

const DisplayHitTestQueryMap& FrameSinkManagerImpl::GetDisplayHitTestQuery()
    const {
  return display_hit_test_query_;
}

void FrameSinkManagerImpl::RegisterCompositorFrameSinkSupport(
    const FrameSinkId& frame_sink_id,
    CompositorFrameSinkSupport* support) {
  DCHECK(support);
  DCHECK(!support_map_.contains(frame_sink_id));

  support_map_[frame_sink_id] = support;

  for (auto& capturer : video_capturers_) {
    if (capturer->target() &&
        capturer->target()->frame_sink_id == frame_sink_id)
      capturer->SetResolvedTarget(support);
  }

  FrameSinkSourceMapping* mapping =
      base::FindOrNull(frame_sink_source_map_, frame_sink_id);
  if (mapping && mapping->source) {
    support->SetBeginFrameSource(mapping->source);
  }

  UpdateThrottlingRecursively(frame_sink_id);

  if (frame_counter_) {
    frame_counter_->AddFrameSink(frame_sink_id, support->is_root(),
                                 GetFrameSinkDebugLabel(frame_sink_id));
  }
}

void FrameSinkManagerImpl::UnregisterCompositorFrameSinkSupport(
    const FrameSinkId& frame_sink_id) {
  TRACE_EVENT("viz",
              "FrameSinkManagerImpl::UnregisterCompositorFrameSinkSupport",
              "frame_sink_id", frame_sink_id);
  DCHECK(support_map_.contains(frame_sink_id));

  for (auto& observer : observer_list_)
    observer.OnDestroyedCompositorFrameSink(frame_sink_id);

  for (auto& capturer : video_capturers_) {
    if (capturer->target() &&
        capturer->target()->frame_sink_id == frame_sink_id)
      capturer->OnTargetWillGoAway();
  }

  captured_frame_sink_ids_.erase(frame_sink_id);
  const bool was_interactive = interactive_frame_sink_ids_.erase(frame_sink_id);

  support_map_.erase(frame_sink_id);

  // If we delete the last interactive frame sink we must do a global update
  // since it was causing all other frame sinks to throttle.
  if (was_interactive && interactive_frame_sink_ids_.empty()) {
    UpdateThrottling();
  } else {
    // The standard path only updates the sub tree to avoid N^2 time complexity
    // when unregistering all frame sinks
    UpdateThrottlingRecursively(frame_sink_id);
  }
}

void FrameSinkManagerImpl::RegisterBeginFrameSource(
    BeginFrameSource* source,
    const FrameSinkId& frame_sink_id) {
  DCHECK(source);
  DCHECK_EQ(registered_sources_.count(source), 0u);

  registered_sources_[source] = frame_sink_id;
  RecursivelyAttachBeginFrameSource(frame_sink_id, source);

  if (frame_sink_id == root_frame_sink_id_) {
    root_begin_frame_source_ = source;
  }
}

void FrameSinkManagerImpl::UnregisterBeginFrameSource(
    BeginFrameSource* source) {
  DCHECK(source);
  DCHECK_EQ(registered_sources_.count(source), 1u);

  FrameSinkId frame_sink_id = registered_sources_[source];
  registered_sources_.erase(source);

  if (frame_sink_id == root_frame_sink_id_) {
    root_begin_frame_source_ = nullptr;
  }

  if (frame_sink_source_map_.count(frame_sink_id) == 0u)
    return;

  // TODO(enne): these walks could be done in one step.
  // Remove this begin frame source from its subtree.
  RecursivelyDetachBeginFrameSource(frame_sink_id, source);
  // Then flush every remaining registered source to fix any sources that
  // became null because of the previous step but that have an alternative.
  for (auto source_iter : registered_sources_)
    RecursivelyAttachBeginFrameSource(source_iter.second, source_iter.first);
}

void FrameSinkManagerImpl::RecursivelyAttachBeginFrameSource(
    const FrameSinkId& frame_sink_id,
    BeginFrameSource* source) {
  FrameSinkSourceMapping& mapping = frame_sink_source_map_[frame_sink_id];
  if (!mapping.source) {
    mapping.source = source;
    CompositorFrameSinkSupport* support =
        base::FindPtrOrNull(support_map_, frame_sink_id);
    if (support) {
      // Updates the InputManager(or FlingScheduler) of BeginFrameSource changes
      // before CompositorFrameSinkSupport since it is 1:1 with
      // RenderInputRouter (for layer tree frame sinks associated CFSS) and
      // updating it earlier may cause UAF bugs.
      if (GetInputManager()) {
        GetInputManager()->SetBeginFrameSource(frame_sink_id, source);
      }
      support->SetBeginFrameSource(source);
    }
  }

  // Copy the list of children because RecursivelyAttachBeginFrameSource() can
  // modify |frame_sink_source_map_| and invalidate iterators.
  base::flat_set<FrameSinkId> children = mapping.children;
  for (const FrameSinkId& child : children)
    RecursivelyAttachBeginFrameSource(child, source);
}

void FrameSinkManagerImpl::RecursivelyDetachBeginFrameSource(
    const FrameSinkId& frame_sink_id,
    BeginFrameSource* source) {
  auto iter = frame_sink_source_map_.find(frame_sink_id);
  if (iter == frame_sink_source_map_.end())
    return;

  auto& mapping = iter->second;
  if (mapping.source == source) {
    mapping.source = nullptr;
    CompositorFrameSinkSupport* support =
        base::FindPtrOrNull(support_map_, frame_sink_id);
    if (support) {
      // Updates the InputManager(or FlingScheduler) of BeginFrameSource changes
      // before CompositorFrameSinkSupport since it is 1:1 with
      // RenderInputRouter (for layer tree frame sinks associated CFSS) and
      // updating it earlier may cause UAF bugs.
      if (GetInputManager()) {
        GetInputManager()->SetBeginFrameSource(frame_sink_id, nullptr);
      }
      support->SetBeginFrameSource(nullptr);
    }
  }

  // Delete the FrameSinkSourceMapping for `frame_sink_id` if both parent and
  // children lists are empty.
  if (mapping.children.empty() && mapping.parent.empty()) {
    frame_sink_source_map_.erase(iter);
    return;
  }

  // Copy the list of children because RecursivelyDetachBeginFrameSource() can
  // modify |frame_sink_source_map_| and invalidate iterators.
  base::flat_set<FrameSinkId> children = mapping.children;
  for (const FrameSinkId& child : children)
    RecursivelyDetachBeginFrameSource(child, source);
}

CapturableFrameSink* FrameSinkManagerImpl::FindCapturableFrameSink(
    const VideoCaptureTarget& target) {
  // Search the known CompositorFrameSinkSupport objects for region capture
  // bounds matching the crop ID specified by |target| (if one was set), and
  // return the corresponding frame sink.
  if (IsRegionCapture(target.sub_target)) {
    const auto crop_id = std::get<RegionCaptureCropId>(target.sub_target);
    for (const auto& id_and_sink : support_map_) {
      const RegionCaptureBounds& bounds =
          id_and_sink.second->current_capture_bounds();
      if (bounds.bounds().contains(crop_id)) {
        return id_and_sink.second;
      }
    }
    return nullptr;
  }

  FrameSinkId frame_sink_id = target.frame_sink_id;
  if (!frame_sink_id.is_valid())
    return nullptr;

  return base::FindPtrOrNull(support_map_, frame_sink_id);
}

void FrameSinkManagerImpl::OnCapturerConnectionLost(
    FrameSinkVideoCapturerImpl* capturer) {
  video_capturers_.erase(capturer);
}

bool FrameSinkManagerImpl::ChildContains(
    const FrameSinkId& child_frame_sink_id,
    const FrameSinkId& search_frame_sink_id) const {
  const FrameSinkSourceMapping* mapping =
      base::FindOrNull(frame_sink_source_map_, child_frame_sink_id);
  if (!mapping) {
    return false;
  }

  for (const FrameSinkId& child : mapping->children) {
    if (child == search_frame_sink_id)
      return true;
    if (ChildContains(child, search_frame_sink_id))
      return true;
  }
  return false;
}

InputManager* FrameSinkManagerImpl::GetInputManager() {
  return input_manager_.get();
}

void FrameSinkManagerImpl::SubmitHitTestRegionList(
    const SurfaceId& surface_id,
    uint64_t frame_index,
    std::optional<HitTestRegionList> hit_test_region_list) {
  hit_test_manager_.SubmitHitTestRegionList(surface_id, frame_index,
                                            std::move(hit_test_region_list));
}

void FrameSinkManagerImpl::OnFrameTokenChangedDirect(
    const FrameSinkId& frame_sink_id,
    uint32_t frame_token,
    base::TimeTicks activation_time) {
  if (client_)
    client_->OnFrameTokenChanged(frame_sink_id, frame_token, activation_time);
}

void FrameSinkManagerImpl::OnFrameTokenChanged(const FrameSinkId& frame_sink_id,
                                               uint32_t frame_token) {
  if (client_remote_ || !ui_task_runner_) {
    // This is a Mojo client or a locally-connected client *without* a task
    // runner. In this case, call directly.
    OnFrameTokenChangedDirect(frame_sink_id, frame_token,
                              /* activation_time =*/base::TimeTicks::Now());
  } else {
    // This is a locally-connected client *with* a task runner - post task.
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FrameSinkManagerImpl::OnFrameTokenChangedDirect,
                       base::Unretained(this), frame_sink_id, frame_token,
                       /* activation_time =*/base::TimeTicks::Now()));
  }
}

VideoDetector* FrameSinkManagerImpl::CreateVideoDetectorForTesting(
    const base::TickClock* tick_clock,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK(!video_detector_);
  video_detector_ = std::make_unique<VideoDetector>(
      GetRegisteredFrameSinkIds(), surface_manager(), tick_clock, task_runner);
  return video_detector_.get();
}

void FrameSinkManagerImpl::DidBeginFrame(const FrameSinkId& frame_sink_id,
                                         const BeginFrameArgs& args) {
  for (auto& observer : observer_list_)
    observer.OnFrameSinkDidBeginFrame(frame_sink_id, args);
}

void FrameSinkManagerImpl::DidFinishFrame(const FrameSinkId& frame_sink_id,
                                          const BeginFrameArgs& args) {
  for (auto& observer : observer_list_)
    observer.OnFrameSinkDidFinishFrame(frame_sink_id, args);
}

void FrameSinkManagerImpl::OnFrameSinkDeviceScaleFactorChanged(
    const FrameSinkId& frame_sink_id,
    float device_scale_factor) {
  for (auto& observer : observer_list_) {
    observer.OnFrameSinkDeviceScaleFactorChanged(frame_sink_id,
                                                 device_scale_factor);
  }
}

void FrameSinkManagerImpl::OnFrameSinkMobileOptimizedChanged(
    const FrameSinkId& frame_sink_id,
    bool is_mobile_optimized) {
  for (auto& observer : observer_list_) {
    observer.OnFrameSinkMobileOptimizedChanged(frame_sink_id,
                                               is_mobile_optimized);
  }
}

void FrameSinkManagerImpl::AddObserver(FrameSinkObserver* obs) {
  observer_list_.AddObserver(obs);
}

void FrameSinkManagerImpl::RemoveObserver(FrameSinkObserver* obs) {
  observer_list_.RemoveObserver(obs);
}

std::vector<FrameSinkId> FrameSinkManagerImpl::GetRegisteredFrameSinkIds()
    const {
  std::vector<FrameSinkId> frame_sink_ids;
  for (auto& map_entry : frame_sink_data_)
    frame_sink_ids.push_back(map_entry.first);
  return frame_sink_ids;
}

FrameSinkId FrameSinkManagerImpl::GetOldestParentByChildFrameId(
    const FrameSinkId& child_frame_sink_id) const {
  CHECK(!root_sink_map_.contains(child_frame_sink_id));

  const FrameSinkSourceMapping* mapping =
      base::FindOrNull(frame_sink_source_map_, child_frame_sink_id);
  if (!mapping || mapping->parent.empty()) {
    return FrameSinkId();
  }
  return mapping->parent.front();
}

int FrameSinkManagerImpl::GetNumParents(
    const FrameSinkId& frame_sink_id) const {
  auto* mapping = base::FindOrNull(frame_sink_source_map_, frame_sink_id);
  return mapping ? mapping->parent.size() : 0;
}

FrameSinkId FrameSinkManagerImpl::GetOldestRootCompositorFrameSinkId(
    const FrameSinkId& child_frame_sink_id) const {
  auto parent_id = GetOldestParentByChildFrameId(child_frame_sink_id);

  while (parent_id.is_valid() && !root_sink_map_.contains(parent_id)) {
    parent_id = GetOldestParentByChildFrameId(parent_id);
  }
  return parent_id;
}

base::flat_set<FrameSinkId> FrameSinkManagerImpl::GetChildrenByParent(
    const FrameSinkId& parent_frame_sink_id) const {
  auto* mapping =
      base::FindOrNull(frame_sink_source_map_, parent_frame_sink_id);
  return mapping ? mapping->children : base::flat_set<FrameSinkId>();
}

void FrameSinkManagerImpl::RecurseChildren(
    const FrameSinkId& frame_sink_id,
    base::FunctionRef<void(const FrameSinkId&)> callback) {
  callback(frame_sink_id);

  auto* mapping = base::FindOrNull(frame_sink_source_map_, frame_sink_id);
  if (mapping) {
    for (const FrameSinkId& child_id : mapping->children) {
      RecurseChildren(child_id, callback);
    }
  }
}

void FrameSinkManagerImpl::RecurseParents(
    const FrameSinkId& frame_sink_id,
    base::FunctionRef<void(const FrameSinkId&)> callback) {
  callback(frame_sink_id);

  auto* mapping = base::FindOrNull(frame_sink_source_map_, frame_sink_id);
  if (mapping) {
    for (const FrameSinkId& parent_id : mapping->parent) {
      RecurseParents(parent_id, callback);
    }
  }
}

CompositorFrameSinkSupport* FrameSinkManagerImpl::GetFrameSinkForId(
    const FrameSinkId& frame_sink_id) const {
  return base::FindPtrOrNull(support_map_, frame_sink_id);
}

void FrameSinkManagerImpl::DiscardPendingCopyOfOutputRequests(
    const BeginFrameSource* source) {
  const auto& root_sink = registered_sources_.at(source);
  base::queue<FrameSinkId> queue;
  for (queue.push(root_sink); !queue.empty(); queue.pop()) {
    auto& frame_sink_id = queue.front();
    CompositorFrameSinkSupport* support =
        base::FindPtrOrNull(support_map_, frame_sink_id);
    if (support) {
      support->ClearAllPendingCopyOutputRequests();
    }
    for (auto child : GetChildrenByParent(frame_sink_id))
      queue.push(child);
  }
}

#if BUILDFLAG(IS_ANDROID)
void FrameSinkManagerImpl::SetPreferEfficientScheduling(
    bool prefer_efficient_scheduling) const {
  if (hint_session_factory_) {
    hint_session_factory_->SetPreferPowerEfficientScheduling(
        prefer_efficient_scheduling);
  }
}
#endif

void FrameSinkManagerImpl::OnCaptureStarted(const FrameSinkId& id) {
  if (captured_frame_sink_ids_.insert(id).second) {
    UpdateThrottling();
  }
  for (auto& observer : observer_list_)
    observer.OnCaptureStarted(id);
}

void FrameSinkManagerImpl::OnCaptureStopped(const FrameSinkId& id) {
  captured_frame_sink_ids_.erase(id);
  UpdateThrottling();
}

void FrameSinkManagerImpl::VerifySandboxedThreadIds(
    const base::flat_set<base::PlatformThreadId>& thread_ids,
    base::OnceCallback<void(bool)> verification_callback) {
#if BUILDFLAG(IS_ANDROID)
  if (!CheckThreadIdsDoNotBelongToCurrentProcess(thread_ids)) {
    // At least one thread belongs to the GPU process, verification failed.
    std::move(verification_callback).Run(false);
    return;
  }
  // GPU check passed, now do an async check for the Browser process.
  static_assert(
      std::is_same_v<int32_t, base::PlatformThreadId::UnderlyingType>);
  std::vector<int32_t> tids;
  tids.reserve(thread_ids.size());
  std::transform(thread_ids.begin(), thread_ids.end(), std::back_inserter(tids),
                 [](const base::PlatformThreadId& tid) { return tid.raw(); });
  client_->VerifyThreadIdsDoNotBelongToHost(tids,
                                            std::move(verification_callback));
#else
  std::move(verification_callback).Run(false);
#endif
}

#if BUILDFLAG(IS_ANDROID)
void FrameSinkManagerImpl::CacheBackBuffer(
    uint32_t cache_id,
    const FrameSinkId& root_frame_sink_id) {
  RootCompositorFrameSinkImpl* root_frame_sink =
      base::FindPtrOrNull(root_sink_map_, root_frame_sink_id);

  // If creating RootCompositorFrameSinkImpl failed there might not be an entry
  // in |root_sink_map_|.
  if (!root_frame_sink) {
    return;
  }

  DCHECK(!cached_back_buffers_.contains(cache_id));
  cached_back_buffers_[cache_id] = root_frame_sink->GetCacheBackBufferCb();
}

void FrameSinkManagerImpl::EvictBackBuffer(uint32_t cache_id,
                                           EvictBackBufferCallback callback) {
  cached_back_buffers_.erase(cache_id);
  std::move(callback).Run();
}
#endif

void FrameSinkManagerImpl::UpdateDebugRendererSettings(
    const DebugRendererSettings& debug_settings) {
  debug_settings_ = debug_settings;
}

void FrameSinkManagerImpl::OnFrameSinkInteractionChanged(
    const FrameSinkId& frame_sink_id,
    bool is_handling_interaction) {
  if (is_handling_interaction) {
    interactive_frame_sink_ids_.insert(frame_sink_id);
  } else {
    interactive_frame_sink_ids_.erase(frame_sink_id);
  }

  // Interaction throttling changes global state, so do a full throttling
  // update.
  UpdateThrottling();
}

void FrameSinkManagerImpl::UpdateThrottlingRecursively(
    FrameSinkId frame_sink_id) {
  const base::TimeDelta default_throttle =
      global_throttle_interval_.value_or(base::TimeDelta());

  // Clear throttling on all children
  RecurseChildren(frame_sink_id,
                  [this, &default_throttle](const FrameSinkId& child_id) {
                    if (auto* support = GetFrameSinkForId(child_id)) {
                      support->SetThrottleInterval(default_throttle);
                      support->SetAllowThrottling(true);
                      if (features::ShouldThrottleWhenInteractiveFrameSinks()) {
                        support->SetThrottledDueToInteraction(
                            !interactive_frame_sink_ids_.empty());
                      }
                    }
                  });

  bool check_throttles = ThrottleIntervalHasEffect();
  bool check_captures = !captured_frame_sink_ids_.empty();
  bool check_interactions = base::FeatureList::IsEnabled(
                                features::kThrottleFrameSinksOnInteraction) &&
                            !interactive_frame_sink_ids_.empty();

  if (!check_throttles && !check_captures && !check_interactions) {
    return;
  }

  base::flat_set<FrameSinkId> throttles;
  base::flat_set<FrameSinkId> captures;
  base::flat_set<FrameSinkId> interactions;

  // Identify anything which would affect throttling from ancestors or
  // descendants.
  auto collect_roots = [&](const FrameSinkId& id, bool is_ancestor) {
    // If an ancestor is throttled/captured, the current `frame_sink_id` acts as
    // the root of that effect for this subtree update.
    //
    // This ensures we limit updates to only the current subtree, while also
    // inheriting capture or throttle effects from parents.
    const FrameSinkId& root = is_ancestor ? frame_sink_id : id;

    if (check_throttles && frame_sink_ids_to_throttle_.contains(id)) {
      throttles.insert(root);
    }
    if (check_captures && captured_frame_sink_ids_.contains(id)) {
      captures.insert(root);
    }

    // Throttle all clients which are not interactive.
    if (check_interactions && interactive_frame_sink_ids_.contains(id)) {
      interactions.insert(id);
    }
  };

  RecurseParents(frame_sink_id, [&](const FrameSinkId& parent) {
    if (parent != frame_sink_id) {
      collect_roots(parent, /*is_ancestor=*/true);
    }
  });

  RecurseChildren(frame_sink_id, [&](const FrameSinkId& child) {
    collect_roots(child, /*is_ancestor=*/false);
  });

  // Apply the identified rules.
  ApplyThrottlingRules(throttles, captures, interactions);
}

void FrameSinkManagerImpl::Throttle(const std::vector<FrameSinkId>& ids,
                                    base::TimeDelta interval) {
  frame_sink_ids_to_throttle_ = base::flat_set<FrameSinkId>(ids);
  throttle_interval_ = interval;
  UpdateThrottling();
}

void FrameSinkManagerImpl::StartThrottlingAllFrameSinks(
    base::TimeDelta interval) {
  global_throttle_interval_ = interval;
  UpdateThrottling();
}

void FrameSinkManagerImpl::StopThrottlingAllFrameSinks() {
  global_throttle_interval_ = std::nullopt;
  UpdateThrottling();
}

void FrameSinkManagerImpl::ApplyThrottlingRules(
    const base::flat_set<FrameSinkId>& throttled_roots,
    const base::flat_set<FrameSinkId>& captured_roots,
    const base::flat_set<FrameSinkId>& interacting_roots) {
  // Apply throttling
  if (ThrottleIntervalHasEffect()) {
    for (const auto& id : throttled_roots) {
      RecurseChildren(id, [this](const FrameSinkId& frame_sink_id) {
        if (auto* support = GetFrameSinkForId(frame_sink_id)) {
          support->SetThrottleInterval(throttle_interval_);
        }
      });
    }
  }

  // Do not allow throttling on frame sinks currently being captured.
  for (const auto& id : captured_roots) {
    RecurseChildren(id, [this](const FrameSinkId& frame_sink_id) {
      if (auto* support = GetFrameSinkForId(frame_sink_id)) {
        support->SetAllowThrottling(false);
      }
    });
  }

  // Interacting clients should not be throttled.
  for (const FrameSinkId& id : interacting_roots) {
    if (auto* support = GetFrameSinkForId(id)) {
      support->SetThrottledDueToInteraction(false);
    }
  }
}

void FrameSinkManagerImpl::UpdateThrottling() {
  // Update throttling on all frame sinks to an initial state, which is
  // either the global state or the unthrottled state.
  const base::TimeDelta gloabl_throttle =
      global_throttle_interval_.value_or(base::TimeDelta());

  for (auto& [id, support] : support_map_) {
    support->SetThrottleInterval(gloabl_throttle);
    support->SetAllowThrottling(true);
    if (features::ShouldThrottleWhenInteractiveFrameSinks()) {
      support->SetThrottledDueToInteraction(
          !interactive_frame_sink_ids_.empty());
    }
  }

  ApplyThrottlingRules(frame_sink_ids_to_throttle_, captured_frame_sink_ids_,
                       interactive_frame_sink_ids_);
}

bool FrameSinkManagerImpl::ThrottleIntervalHasEffect() const {
  return !global_throttle_interval_ ||
         throttle_interval_ > global_throttle_interval_;
}

void FrameSinkManagerImpl::MaybeEraseHitTestQuery(
    const FrameSinkId& frame_sink_id) {
  if (!input::InputUtils::IsTransferInputToVizSupported()) {
    return;
  }
  display_hit_test_query_.erase(frame_sink_id);
}

void FrameSinkManagerImpl::MaybeAddHitTestQuery(
    const FrameSinkId& frame_sink_id) {
  if (!input::InputUtils::IsTransferInputToVizSupported()) {
    return;
  }
  CompositorFrameSinkSupport* support =
      base::FindPtrOrNull(support_map_, frame_sink_id);
  // Only create a HitTestQuery for `frame_sink_id` if `InputOnViz` flag is
  // enabled and a corresponding CompositorFrameSinkSupport* has been created
  // for this RootCompositorFrameSink.
  if (support) {
    display_hit_test_query_[frame_sink_id] = std::make_unique<HitTestQuery>(
        support->GetHitTestAggregator()->GetDataProviderSafeRef());
  }
}

void FrameSinkManagerImpl::CacheSurfaceAnimationManager(
    const blink::ViewTransitionToken& transition_token,
    std::unique_ptr<SurfaceAnimationManager> manager) {
  if (transition_token_to_animation_manager_.contains(transition_token)) {
    LOG(ERROR)
        << "SurfaceAnimationManager already exists for |transition_token| : "
        << transition_token;
    return;
  }

  transition_token_to_animation_manager_[transition_token] = std::move(manager);
  for (auto& observer : observer_list_) {
    observer.OnViewTransitionSaved(transition_token);
  }
}

std::unique_ptr<SurfaceAnimationManager>
FrameSinkManagerImpl::TakeSurfaceAnimationManager(
    const blink::ViewTransitionToken& transition_token) {
  auto it = transition_token_to_animation_manager_.find(transition_token);
  if (it == transition_token_to_animation_manager_.end()) {
    LOG(ERROR) << "SurfaceAnimationManager missing for |transition_token| : "
               << transition_token;
    return nullptr;
  }

  auto manager = std::move(it->second);
  transition_token_to_animation_manager_.erase(it);
  return manager;
}

bool FrameSinkManagerImpl::ClearSurfaceAnimationManager(
    const blink::ViewTransitionToken& transition_token) {
  return transition_token_to_animation_manager_.erase(transition_token);
}

void FrameSinkManagerImpl::OnScreenshotCaptured(
    const blink::SameDocNavigationScreenshotDestinationToken& destination_token,
    std::unique_ptr<CopyOutputResult> copy_output_result) {
  client_->OnScreenshotCaptured(destination_token,
                                std::move(copy_output_result));
}

void FrameSinkManagerImpl::OnViewTransitionResourcesCaptured(
    const blink::ViewTransitionToken& transition_token) {
  if (client_) {
    client_->OnViewTransitionResourcesCaptured(transition_token);
  }
  MarkSameDocViewTransitionTokenReady(transition_token);
}

bool FrameSinkManagerImpl::IsFrameSinkIdInRootSinkMap(
    const FrameSinkId& frame_sink_id) {
  return root_sink_map_.contains(frame_sink_id);
}

gpu::SharedImageInterface* FrameSinkManagerImpl::GetSharedImageInterface() {
  DCHECK(shared_image_interface_provider_);
  return shared_image_interface_provider_->GetSharedImageInterface();
}

base::OnceCallback<void(const blink::ViewTransitionToken&)>
FrameSinkManagerImpl::GetViewTransitionResourcesCapturedCallback() {
  return base::BindOnce(
      &FrameSinkManagerImpl::OnViewTransitionResourcesCaptured,
      weak_factory_.GetWeakPtr());
}

void FrameSinkManagerImpl::StartFrameCounting(base::TimeTicks start_time,
                                              base::TimeDelta bucket_size) {
  DCHECK(!frame_counter_.has_value());
  frame_counter_.emplace(start_time, bucket_size);

  for (auto& [sink_id, support] : support_map_) {
    DCHECK_EQ(sink_id, support->frame_sink_id());
    frame_counter_->AddFrameSink(sink_id, support->is_root(),
                                 GetFrameSinkDebugLabel(sink_id));
  }
}

void FrameSinkManagerImpl::StopFrameCounting(
    StopFrameCountingCallback callback) {
  // Returns empty data if `frame_counter_` has no value. This could happen
  // when gpu-process is restarted in middle of test and test scripts still
  // calls this at the end.
  if (!frame_counter_.has_value()) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(frame_counter_->TakeData());
  frame_counter_.reset();
}

void FrameSinkManagerImpl::StartOverdrawTracking(
    const FrameSinkId& root_frame_sink_id,
    base::TimeDelta bucket_size) {
  RootCompositorFrameSinkImpl* root_frame_sink =
      base::FindPtrOrNull(root_sink_map_, root_frame_sink_id);
  if (!root_frame_sink) {
    LOG(ERROR) << "No RootCompositorFrameSink for root_frame_sink_id:"
               << root_frame_sink_id;
    return;
  }

  root_frame_sink->StartOverdrawTracking(bucket_size.InSeconds());
}

void FrameSinkManagerImpl::StopOverdrawTracking(
    const FrameSinkId& root_frame_sink_id,
    StopOverdrawTrackingCallback callback) {
  RootCompositorFrameSinkImpl* root_frame_sink =
      base::FindPtrOrNull(root_sink_map_, root_frame_sink_id);
  if (!root_frame_sink) {
    LOG(ERROR) << "No RootCompositorFrameSink for root_frame_sink_id:"
               << root_frame_sink_id;
    std::move(callback).Run(std::move(nullptr));
    return;
  }

  mojom::OverdrawDataPtr data = mojom::OverdrawData::New();
  data->average_overdraws = root_frame_sink->StopOverdrawTracking();
  std::move(callback).Run(std::move(data));
}

void FrameSinkManagerImpl::HasUnclaimedViewTransitionResources(
    HasUnclaimedViewTransitionResourcesCallback callback) {
  std::move(callback).Run(!transition_token_to_animation_manager_.empty());
}

void FrameSinkManagerImpl::SetSameDocNavigationScreenshotSize(
    const gfx::Size& result_size,
    SetSameDocNavigationScreenshotSizeCallback callback) {
  copy_output_request_result_size_for_testing_ = result_size;
  std::move(callback).Run();
}

void FrameSinkManagerImpl::GetForceEnableZoomState(
    const FrameSinkId& frame_sink_id,
    GetForceEnableZoomStateCallback callback) {
  CHECK(GetInputManager());
  CHECK(GetInputManager()->GetRenderInputRouterFromFrameSinkId(frame_sink_id));
  bool enabled = GetInputManager()
                     ->GetRenderInputRouterFromFrameSinkId(frame_sink_id)
                     ->GetForceEnableZoom();
  std::move(callback).Run(enabled);
}

void FrameSinkManagerImpl::WaitForSurfaceAnimationManager(
    const FrameSinkId& frame_sink_id,
    WaitForSurfaceAnimationManagerCallback callback) {
  auto* support = FrameSinkManagerImpl::GetFrameSinkForId(frame_sink_id);
  CHECK(support);

  support->RegisterSurfaceAnimationManagerNotification(std::move(callback));
}

void FrameSinkManagerImpl::ClearUnclaimedViewTransitionResources(
    const blink::ViewTransitionToken& transition_token) {
  transition_token_to_animation_manager_.erase(transition_token);
}

void FrameSinkManagerImpl::CreateMetricsRecorderForTest(
    mojo::PendingReceiver<mojom::FrameSinksMetricsRecorder> receiver) {
  CHECK(!metrics_receiver_.is_bound());
  metrics_receiver_.Bind(std::move(receiver));
}

void FrameSinkManagerImpl::EnableFrameSinkManagerTestApi(
    mojo::PendingReceiver<mojom::FrameSinkManagerTestApi> receiver) {
  CHECK(!test_api_receiver_.is_bound());
  test_api_receiver_.Bind(std::move(receiver));
}

void FrameSinkManagerImpl::SetupRendererInputRouterDelegateRegistry(
    mojo::PendingReceiver<mojom::RendererInputRouterDelegateRegistry>
        receiver) {
  input_manager_->SetupRendererInputRouterDelegateRegistry(std::move(receiver));
}

void FrameSinkManagerImpl::NotifyRendererBlockStateChanged(
    bool blocked,
    const std::vector<FrameSinkId>& render_input_routers) {
  input_manager_->NotifyRendererBlockStateChanged(blocked,
                                                  render_input_routers);
}

void FrameSinkManagerImpl::RequestInputBack() {
  bool success = input_manager_->ReturnInputBackToBrowser();
  TRACE_EVENT_INSTANT("viz", "FrameSinkManagerImpl::RequestInputBack",
                      "success", success);
}

void FrameSinkManagerImpl::RequestBeginFrameForGpuService(bool toggle) {
  if (root_begin_frame_source_ && gpu_service_) {
    if (toggle) {
      root_begin_frame_source_->AddObserver(gpu_service_);
    } else {
      root_begin_frame_source_->RemoveObserver(gpu_service_);
    }
  }
}

GpuServiceImpl* FrameSinkManagerImpl::GetGpuService() {
  return gpu_service_;
}

}  // namespace viz
