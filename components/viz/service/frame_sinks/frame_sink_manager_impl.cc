// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/viz/service/display/shared_bitmap_manager.h"
#include "components/viz/service/display_embedder/output_surface_provider.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/video_capture/capturable_frame_sink.h"
#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_impl.h"

namespace viz {

FrameSinkManagerImpl::InitParams::InitParams() = default;
FrameSinkManagerImpl::InitParams::InitParams(
    SharedBitmapManager* shared_bitmap_manager,
    OutputSurfaceProvider* output_surface_provider)
    : shared_bitmap_manager(shared_bitmap_manager),
      output_surface_provider(output_surface_provider) {}
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
    : shared_bitmap_manager_(params.shared_bitmap_manager),
      output_surface_provider_(params.output_surface_provider),
      surface_manager_(this, params.activation_deadline_in_frames),
      hit_test_manager_(surface_manager()),
      restart_id_(params.restart_id),
      run_all_compositor_stages_before_draw_(
          params.run_all_compositor_stages_before_draw) {
  surface_manager_.AddObserver(&hit_test_manager_);
  surface_manager_.AddObserver(this);
}

FrameSinkManagerImpl::FrameSinkManagerImpl(
    SharedBitmapManager* shared_bitmap_manager,
    OutputSurfaceProvider* output_surface_provider)
    : FrameSinkManagerImpl(
          InitParams(shared_bitmap_manager, output_surface_provider)) {}

FrameSinkManagerImpl::~FrameSinkManagerImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  video_capturers_.clear();

  // All mojom::CompositorFrameSinks and BeginFrameSources should be deleted by
  // this point.
  DCHECK(sink_map_.empty());
  DCHECK(root_sink_map_.empty());
  DCHECK(cached_back_buffers_.empty());
  DCHECK(registered_sources_.empty());

  surface_manager_.RemoveObserver(this);
  surface_manager_.RemoveObserver(&hit_test_manager_);
}

void FrameSinkManagerImpl::BindAndSetClient(
    mojo::PendingReceiver<mojom::FrameSinkManager> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingRemote<mojom::FrameSinkManagerClient> client) {
  DCHECK(!client_);
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver), std::move(task_runner));
  client_remote_.Bind(std::move(client));
  client_ = client_remote_.get();
}

void FrameSinkManagerImpl::SetLocalClient(
    mojom::FrameSinkManagerClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  DCHECK(!client_remote_);
  DCHECK(!ui_task_runner_);
  client_ = client;
  ui_task_runner_ = ui_task_runner;
}

void FrameSinkManagerImpl::ForceShutdown() {
  if (receiver_.is_bound())
    receiver_.reset();

  for (auto& it : cached_back_buffers_)
    it.second.RunAndReset();
  cached_back_buffers_.clear();

  sink_map_.clear();
  root_sink_map_.clear();
}

void FrameSinkManagerImpl::RegisterFrameSinkId(const FrameSinkId& frame_sink_id,
                                               bool report_activation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!base::Contains(frame_sink_data_, frame_sink_id));

  frame_sink_data_.emplace(std::make_pair(frame_sink_id, report_activation));

  if (video_detector_)
    video_detector_->OnFrameSinkIdRegistered(frame_sink_id);

  for (auto& observer : observer_list_)
    observer.OnRegisteredFrameSinkId(frame_sink_id);
}

void FrameSinkManagerImpl::InvalidateFrameSinkId(
    const FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  surface_manager_.InvalidateFrameSinkId(frame_sink_id);
  if (video_detector_)
    video_detector_->OnFrameSinkIdInvalidated(frame_sink_id);

  // Destroy the [Root]CompositorFrameSinkImpl if there is one.
  sink_map_.erase(frame_sink_id);
  root_sink_map_.erase(frame_sink_id);

  frame_sink_data_.erase(frame_sink_id);

  for (auto& observer : observer_list_)
    observer.OnInvalidatedFrameSinkId(frame_sink_id);
}

void FrameSinkManagerImpl::EnableSynchronizationReporting(
    const FrameSinkId& frame_sink_id,
    const std::string& reporting_label) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = frame_sink_data_.find(frame_sink_id);
  if (it != frame_sink_data_.end())
    it->second.synchronization_label = reporting_label;
}

void FrameSinkManagerImpl::SetFrameSinkDebugLabel(
    const FrameSinkId& frame_sink_id,
    const std::string& debug_label) {
  auto it = frame_sink_data_.find(frame_sink_id);
  if (it != frame_sink_data_.end())
    it->second.debug_label = debug_label;
}

void FrameSinkManagerImpl::CreateRootCompositorFrameSink(
    mojom::RootCompositorFrameSinkParamsPtr params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!base::Contains(root_sink_map_, params->frame_sink_id));
  DCHECK(output_surface_provider_);

  // We are transfering ownership of |params| so remember FrameSinkId here.
  FrameSinkId frame_sink_id = params->frame_sink_id;

  // Creating RootCompositorFrameSinkImpl can fail and return null.
  auto root_compositor_frame_sink = RootCompositorFrameSinkImpl::Create(
      std::move(params), this, output_surface_provider_, restart_id_,
      run_all_compositor_stages_before_draw_);
  if (root_compositor_frame_sink)
    root_sink_map_[frame_sink_id] = std::move(root_compositor_frame_sink);
}

void FrameSinkManagerImpl::CreateCompositorFrameSink(
    const FrameSinkId& frame_sink_id,
    mojo::PendingReceiver<mojom::CompositorFrameSink> receiver,
    mojo::PendingRemote<mojom::CompositorFrameSinkClient> client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!base::Contains(sink_map_, frame_sink_id));

  sink_map_[frame_sink_id] = std::make_unique<CompositorFrameSinkImpl>(
      this, frame_sink_id, std::move(receiver), std::move(client));
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
  DCHECK(!base::Contains(children, child_frame_sink_id));
  children.insert(child_frame_sink_id);

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
  // Deliberately do not check validity of either parent or child FrameSinkId
  // here. They were valid during the registration, so were valid at some point
  // in time. This makes it possible to invalidate parent and child FrameSinkIds
  // independently of each other and not have an ordering dependency of
  // unregistering the hierarchy first before either of them.

  for (auto& observer : observer_list_) {
    observer.OnUnregisteredFrameSinkHierarchy(parent_frame_sink_id,
                                              child_frame_sink_id);
  }

  auto iter = frame_sink_source_map_.find(parent_frame_sink_id);
  DCHECK(iter != frame_sink_source_map_.end());

  // Remove |child_frame_sink_id| from parents list of children.
  auto& mapping = iter->second;
  DCHECK(base::Contains(mapping.children, child_frame_sink_id));
  mapping.children.erase(child_frame_sink_id);

  // Delete the FrameSinkSourceMapping for |parent_frame_sink_id| if empty.
  if (mapping.children.empty() && !mapping.source) {
    frame_sink_source_map_.erase(iter);
    return;
  }

  // If the parent does not have a begin frame source, then disconnecting it
  // will not change any of its children.
  BeginFrameSource* parent_source = iter->second.source;
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
    mojo::PendingReceiver<mojom::FrameSinkVideoCapturer> receiver) {
  video_capturers_.emplace(std::make_unique<FrameSinkVideoCapturerImpl>(
      this, std::move(receiver),
      std::make_unique<media::VideoCaptureOracle>(
          true /* enable_auto_throttling */)));
}

void FrameSinkManagerImpl::EvictSurfaces(
    const std::vector<SurfaceId>& surface_ids) {
  for (const SurfaceId& surface_id : surface_ids) {
    auto it = support_map_.find(surface_id.frame_sink_id());
    if (it == support_map_.end())
      continue;
    it->second->EvictSurface(surface_id.local_surface_id());
  }
}

void FrameSinkManagerImpl::RequestCopyOfOutput(
    const SurfaceId& surface_id,
    std::unique_ptr<CopyOutputRequest> request) {
  auto it = support_map_.find(surface_id.frame_sink_id());
  if (it == support_map_.end()) {
    // |request| will send an empty result when it goes out of scope.
    return;
  }
  it->second->RequestCopyOfOutput(surface_id.local_surface_id(),
                                  std::move(request));
}

void FrameSinkManagerImpl::SetHitTestAsyncQueriedDebugRegions(
    const FrameSinkId& root_frame_sink_id,
    const std::vector<FrameSinkId>& hit_test_async_queried_debug_queue) {
  hit_test_manager_.SetHitTestAsyncQueriedDebugRegions(
      root_frame_sink_id, hit_test_async_queried_debug_queue);
  DCHECK(base::Contains(root_sink_map_, root_frame_sink_id));
  root_sink_map_[root_frame_sink_id]->ForceImmediateDrawAndSwapIfPossible();
}

void FrameSinkManagerImpl::OnFirstSurfaceActivation(
    const SurfaceInfo& surface_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(surface_info.device_scale_factor(), 0.0f);

  auto it = frame_sink_data_.find(surface_info.id().frame_sink_id());
  if (it == frame_sink_data_.end())
    return;

  const FrameSinkData& frame_sink_data = it->second;

  if (client_ && frame_sink_data.report_activation)
    client_->OnFirstSurfaceActivation(surface_info);
}

void FrameSinkManagerImpl::OnSurfaceActivated(
    const SurfaceId& surface_id,
    base::Optional<base::TimeDelta> duration) {
  if (!duration || !client_)
    return;

  // If |duration| is populated then there was a synchronization event prior
  // to this activation.
  auto it = frame_sink_data_.find(surface_id.frame_sink_id());
  if (it == frame_sink_data_.end())
    return;

  std::string& synchronization_label = it->second.synchronization_label;
  if (!synchronization_label.empty()) {
    TRACE_EVENT_INSTANT2("viz", "SurfaceSynchronizationEvent",
                         TRACE_EVENT_SCOPE_THREAD, "duration_ms",
                         duration->InMilliseconds(), "client_label",
                         synchronization_label);
    base::UmaHistogramCustomCounts(synchronization_label,
                                   duration->InMilliseconds(), 1, 10000, 50);
  }
}

bool FrameSinkManagerImpl::OnSurfaceDamaged(const SurfaceId& surface_id,
                                            const BeginFrameAck& ack) {
  return false;
}

void FrameSinkManagerImpl::OnSurfaceDestroyed(const SurfaceId& surface_id) {}

void FrameSinkManagerImpl::OnSurfaceMarkedForDestruction(
    const SurfaceId& surface_id) {}

void FrameSinkManagerImpl::OnSurfaceDamageExpected(const SurfaceId& surface_id,
                                                   const BeginFrameArgs& args) {
}

void FrameSinkManagerImpl::OnAggregatedHitTestRegionListUpdated(
    const FrameSinkId& frame_sink_id,
    const std::vector<AggregatedHitTestRegion>& hit_test_data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client_) {
    client_->OnAggregatedHitTestRegionListUpdated(frame_sink_id, hit_test_data);
  }
}

base::StringPiece FrameSinkManagerImpl::GetFrameSinkDebugLabel(
    const FrameSinkId& frame_sink_id) const {
  auto it = frame_sink_data_.find(frame_sink_id);
  if (it != frame_sink_data_.end())
    return it->second.debug_label;
  return base::StringPiece();
}

void FrameSinkManagerImpl::RegisterCompositorFrameSinkSupport(
    const FrameSinkId& frame_sink_id,
    CompositorFrameSinkSupport* support) {
  DCHECK(support);
  DCHECK(!base::Contains(support_map_, frame_sink_id));

  support_map_[frame_sink_id] = support;

  for (auto& capturer : video_capturers_) {
    if (capturer->requested_target() == frame_sink_id)
      capturer->SetResolvedTarget(support);
  }

  auto it = frame_sink_source_map_.find(frame_sink_id);
  if (it != frame_sink_source_map_.end() && it->second.source)
    support->SetBeginFrameSource(it->second.source);

  for (auto& observer : observer_list_)
    observer.OnCreatedCompositorFrameSink(frame_sink_id, support->is_root());
}

void FrameSinkManagerImpl::UnregisterCompositorFrameSinkSupport(
    const FrameSinkId& frame_sink_id) {
  DCHECK(base::Contains(support_map_, frame_sink_id));

  for (auto& observer : observer_list_)
    observer.OnDestroyedCompositorFrameSink(frame_sink_id);

  for (auto& capturer : video_capturers_) {
    if (capturer->requested_target() == frame_sink_id)
      capturer->OnTargetWillGoAway();
  }

  support_map_.erase(frame_sink_id);
}

void FrameSinkManagerImpl::RegisterBeginFrameSource(
    BeginFrameSource* source,
    const FrameSinkId& frame_sink_id) {
  DCHECK(source);
  DCHECK_EQ(registered_sources_.count(source), 0u);

  registered_sources_[source] = frame_sink_id;
  RecursivelyAttachBeginFrameSource(frame_sink_id, source);
}

void FrameSinkManagerImpl::UnregisterBeginFrameSource(
    BeginFrameSource* source) {
  DCHECK(source);
  DCHECK_EQ(registered_sources_.count(source), 1u);

  FrameSinkId frame_sink_id = registered_sources_[source];
  registered_sources_.erase(source);

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
    auto iter = support_map_.find(frame_sink_id);
    if (iter != support_map_.end())
      iter->second->SetBeginFrameSource(source);
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
    auto client_iter = support_map_.find(frame_sink_id);
    if (client_iter != support_map_.end())
      client_iter->second->SetBeginFrameSource(nullptr);
  }

  // Delete the FrameSinkSourceMapping for |frame_sink_id| if empty.
  if (mapping.children.empty()) {
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
    const FrameSinkId& frame_sink_id) {
  const auto it = support_map_.find(frame_sink_id);
  if (it == support_map_.end())
    return nullptr;
  return it->second;
}

void FrameSinkManagerImpl::OnCapturerConnectionLost(
    FrameSinkVideoCapturerImpl* capturer) {
  video_capturers_.erase(capturer);
}

bool FrameSinkManagerImpl::ChildContains(
    const FrameSinkId& child_frame_sink_id,
    const FrameSinkId& search_frame_sink_id) const {
  auto iter = frame_sink_source_map_.find(child_frame_sink_id);
  if (iter == frame_sink_source_map_.end())
    return false;

  for (const FrameSinkId& child : iter->second.children) {
    if (child == search_frame_sink_id)
      return true;
    if (ChildContains(child, search_frame_sink_id))
      return true;
  }
  return false;
}

void FrameSinkManagerImpl::SubmitHitTestRegionList(
    const SurfaceId& surface_id,
    uint64_t frame_index,
    base::Optional<HitTestRegionList> hit_test_region_list) {
  hit_test_manager_.SubmitHitTestRegionList(surface_id, frame_index,
                                            std::move(hit_test_region_list));
}

void FrameSinkManagerImpl::OnFrameTokenChangedDirect(
    const FrameSinkId& frame_sink_id,
    uint32_t frame_token) {
  if (client_)
    client_->OnFrameTokenChanged(frame_sink_id, frame_token);
}

void FrameSinkManagerImpl::OnFrameTokenChanged(const FrameSinkId& frame_sink_id,
                                               uint32_t frame_token) {
  if (client_remote_ || !ui_task_runner_) {
    // This is a Mojo client or a locally-connected client *without* a task
    // runner. In this case, call directly.
    OnFrameTokenChangedDirect(frame_sink_id, frame_token);
  } else {
    // This is a locally-connected client *with* a task runner - post task.
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FrameSinkManagerImpl::OnFrameTokenChangedDirect,
                       base::Unretained(this), frame_sink_id, frame_token));
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

base::flat_set<FrameSinkId> FrameSinkManagerImpl::GetChildrenByParent(
    const FrameSinkId& parent_frame_sink_id) const {
  auto it = frame_sink_source_map_.find(parent_frame_sink_id);
  if (it != frame_sink_source_map_.end())
    return it->second.children;
  return {};
}

const CompositorFrameSinkSupport* FrameSinkManagerImpl::GetFrameSinkForId(
    const FrameSinkId& frame_sink_id) const {
  auto it = support_map_.find(frame_sink_id);
  if (it != support_map_.end())
    return it->second;
  return nullptr;
}

void FrameSinkManagerImpl::SetPreferredFrameIntervalForFrameSinkId(
    const FrameSinkId& id,
    base::TimeDelta interval) {
  auto it = frame_sink_data_.find(id);
  DCHECK(it != frame_sink_data_.end());
  it->second.preferred_frame_interval = interval;
}

base::TimeDelta FrameSinkManagerImpl::GetPreferredFrameIntervalForFrameSinkId(
    const FrameSinkId& id) const {
  auto it = frame_sink_data_.find(id);
  if (it == frame_sink_data_.end())
    return BeginFrameArgs::MinInterval();

  return it->second.preferred_frame_interval;
}

void FrameSinkManagerImpl::CacheBackBuffer(
    uint32_t cache_id,
    const FrameSinkId& root_frame_sink_id) {
  auto it = root_sink_map_.find(root_frame_sink_id);

  DCHECK(it != root_sink_map_.end());
  DCHECK(cached_back_buffers_.find(cache_id) == cached_back_buffers_.end());

  cached_back_buffers_[cache_id] = it->second->GetCacheBackBufferCb();
}

void FrameSinkManagerImpl::EvictBackBuffer(uint32_t cache_id,
                                           EvictBackBufferCallback callback) {
  auto it = cached_back_buffers_.find(cache_id);
  DCHECK(it != cached_back_buffers_.end());

  it->second.RunAndReset();
  cached_back_buffers_.erase(it);
  std::move(callback).Run();
}

}  // namespace viz
