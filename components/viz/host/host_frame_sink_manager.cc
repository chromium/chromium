// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/host_frame_sink_manager.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/host/renderer_settings_creation.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "services/viz/privileged/mojom/compositing/renderer_settings.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace viz {

HostFrameSinkManager::HostFrameSinkManager()
    : debug_renderer_settings_(CreateDefaultDebugRendererSettings()) {}

HostFrameSinkManager::~HostFrameSinkManager() = default;

void HostFrameSinkManager::SetLocalManager(
    mojom::FrameSinkManager* frame_sink_manager) {
  DCHECK(!frame_sink_manager_remote_);
  frame_sink_manager_ = frame_sink_manager;
}

void HostFrameSinkManager::BindAndSetManager(
    mojo::PendingReceiver<mojom::FrameSinkManagerClient> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingRemote<mojom::FrameSinkManager> remote) {
  DCHECK(!receiver_.is_bound());

  receiver_.Bind(std::move(receiver), std::move(task_runner));
  frame_sink_manager_remote_.Bind(std::move(remote));
  frame_sink_manager_ = frame_sink_manager_remote_.get();

  frame_sink_manager_remote_.set_disconnect_handler(base::BindOnce(
      &HostFrameSinkManager::OnConnectionLost, base::Unretained(this)));

  if (connection_was_lost_) {
    RegisterAfterConnectionLoss();
    connection_was_lost_ = false;
  }
}

void HostFrameSinkManager::SetConnectionLostCallback(
    base::RepeatingClosure callback) {
  connection_lost_callback_ = std::move(callback);
}

void HostFrameSinkManager::RegisterFrameSinkId(
    const FrameSinkId& frame_sink_id,
    HostFrameSinkClient* client,
    ReportFirstSurfaceActivation report_activation) {
  DCHECK(frame_sink_id.is_valid());
  DCHECK(client);

  FrameSinkData& data = frame_sink_data_map_[frame_sink_id];
  CHECK(!data.IsFrameSinkRegistered());
  DCHECK(!data.has_created_compositor_frame_sink);
  data.client = client;
  data.report_activation = report_activation;
  frame_sink_manager_->RegisterFrameSinkId(
      frame_sink_id, report_activation == ReportFirstSurfaceActivation::kYes);
}

bool HostFrameSinkManager::IsFrameSinkIdRegistered(
    const FrameSinkId& frame_sink_id) const {
  auto iter = frame_sink_data_map_.find(frame_sink_id);
  return iter != frame_sink_data_map_.end() && iter->second.client != nullptr;
}

void HostFrameSinkManager::InvalidateFrameSinkId(
    const FrameSinkId& frame_sink_id) {
  DCHECK(frame_sink_id.is_valid());

  FrameSinkData& data = frame_sink_data_map_[frame_sink_id];
  CHECK(data.IsFrameSinkRegistered());

  const bool destroy_synchronously =
      data.has_created_compositor_frame_sink && data.wait_on_destruction;

  data.has_created_compositor_frame_sink = false;
  data.client = nullptr;

  // There may be frame sink hierarchy information left in FrameSinkData.
  if (data.IsEmpty())
    frame_sink_data_map_.erase(frame_sink_id);

  display_hit_test_query_.erase(frame_sink_id);

  if (destroy_synchronously) {
    // This synchronous call ensures that the GL context/surface that draw to
    // the platform window (eg. XWindow or HWND) get destroyed before the
    // platform window is destroyed.
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
    frame_sink_manager_->DestroyCompositorFrameSink(frame_sink_id);

    // Other synchronous IPCs continue to get processed while
    // DestroyCompositorFrameSink() is happening, so it's possible
    // HostFrameSinkManager has been mutated. |data| might not be a valid
    // reference at this point.
  }

  frame_sink_manager_->InvalidateFrameSinkId(frame_sink_id);
}

void HostFrameSinkManager::SetFrameSinkDebugLabel(
    const FrameSinkId& frame_sink_id,
    const std::string& debug_label) {
  DCHECK(frame_sink_id.is_valid());

  FrameSinkData& data = frame_sink_data_map_[frame_sink_id];
  DCHECK(data.IsFrameSinkRegistered());

  data.debug_label = debug_label;
  frame_sink_manager_->SetFrameSinkDebugLabel(frame_sink_id, debug_label);
}

void HostFrameSinkManager::CreateRootCompositorFrameSink(
    mojom::RootCompositorFrameSinkParamsPtr params,
    bool maybe_wait_on_destruction /*=true*/) {
  // Should only be used with an out-of-process display compositor.
  DCHECK(frame_sink_manager_remote_);

  FrameSinkId frame_sink_id = params->frame_sink_id;
  FrameSinkData& data = frame_sink_data_map_[frame_sink_id];
  DCHECK(data.IsFrameSinkRegistered());

  // If GL context is lost a new CompositorFrameSink will be created. Destroy
  // the old CompositorFrameSink first.
  if (data.has_created_compositor_frame_sink) {
    frame_sink_manager_->DestroyCompositorFrameSink(frame_sink_id,
                                                    base::DoNothing());
  }

  data.is_root = true;
  data.has_created_compositor_frame_sink = true;

  // Only wait on destruction if using GPU compositing for the window.
  data.wait_on_destruction =
      maybe_wait_on_destruction && params->gpu_compositing;

  frame_sink_manager_->CreateRootCompositorFrameSink(std::move(params));
  display_hit_test_query_[frame_sink_id] = std::make_unique<HitTestQuery>();
}

void HostFrameSinkManager::CreateCompositorFrameSink(
    const FrameSinkId& frame_sink_id,
    mojo::PendingReceiver<mojom::CompositorFrameSink> receiver,
    mojo::PendingRemote<mojom::CompositorFrameSinkClient> client) {
  CreateFrameSink(frame_sink_id, /*bundle_id=*/absl::nullopt,
                  std::move(receiver), std::move(client));
}

void HostFrameSinkManager::CreateFrameSinkBundle(
    const FrameSinkBundleId& bundle_id,
    mojo::PendingReceiver<mojom::FrameSinkBundle> receiver,
    mojo::PendingRemote<mojom::FrameSinkBundleClient> client) {
  frame_sink_manager_->CreateFrameSinkBundle(bundle_id, std::move(receiver),
                                             std::move(client));
}

void HostFrameSinkManager::CreateBundledCompositorFrameSink(
    const FrameSinkId& frame_sink_id,
    const FrameSinkBundleId& bundle_id,
    mojo::PendingReceiver<mojom::CompositorFrameSink> receiver,
    mojo::PendingRemote<mojom::CompositorFrameSinkClient> client) {
  CreateFrameSink(frame_sink_id, bundle_id, std::move(receiver),
                  std::move(client));
}

void HostFrameSinkManager::CreateFrameSink(
    const FrameSinkId& frame_sink_id,
    absl::optional<FrameSinkBundleId> bundle_id,
    mojo::PendingReceiver<mojom::CompositorFrameSink> receiver,
    mojo::PendingRemote<mojom::CompositorFrameSinkClient> client) {
  FrameSinkData& data = frame_sink_data_map_[frame_sink_id];
  DCHECK(data.IsFrameSinkRegistered());

  // If GL context is lost a new CompositorFrameSink will be created. Destroy
  // the old CompositorFrameSink first.
  if (data.has_created_compositor_frame_sink) {
    frame_sink_manager_->DestroyCompositorFrameSink(frame_sink_id,
                                                    base::DoNothing());
  }

  data.is_root = false;
  data.has_created_compositor_frame_sink = true;

  frame_sink_manager_->CreateCompositorFrameSink(
      frame_sink_id, bundle_id, std::move(receiver), std::move(client));
}

void HostFrameSinkManager::OnFrameTokenChanged(
    const FrameSinkId& frame_sink_id,
    uint32_t frame_token,
    base::TimeTicks activation_time) {
  DCHECK(frame_sink_id.is_valid());
  auto iter = frame_sink_data_map_.find(frame_sink_id);
  if (iter == frame_sink_data_map_.end())
    return;

  const FrameSinkData& data = iter->second;
  if (data.client)
    data.client->OnFrameTokenChanged(frame_token, activation_time);
}

bool HostFrameSinkManager::RegisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  auto iter = frame_sink_data_map_.find(parent_frame_sink_id);
  // |parent_frame_sink_id| isn't registered so it can't embed anything.
  if (iter == frame_sink_data_map_.end() ||
      !iter->second.IsFrameSinkRegistered()) {
    return false;
  }

  FrameSinkData& parent_data = iter->second;
  CHECK(!base::Contains(parent_data.children, child_frame_sink_id));
  parent_data.children.push_back(child_frame_sink_id);

  // Register and store the parent.
  frame_sink_manager_->RegisterFrameSinkHierarchy(parent_frame_sink_id,
                                                  child_frame_sink_id);

  return true;
}

void HostFrameSinkManager::UnregisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  // Unregister and clear the stored parent.
  FrameSinkData& parent_data = frame_sink_data_map_[parent_frame_sink_id];
  size_t num_erased = base::Erase(parent_data.children, child_frame_sink_id);
  CHECK_EQ(num_erased, 1u);

  if (parent_data.IsEmpty())
    frame_sink_data_map_.erase(parent_frame_sink_id);

  frame_sink_manager_->UnregisterFrameSinkHierarchy(parent_frame_sink_id,
                                                    child_frame_sink_id);
}

void HostFrameSinkManager::AddVideoDetectorObserver(
    mojo::PendingRemote<mojom::VideoDetectorObserver> observer) {
  frame_sink_manager_->AddVideoDetectorObserver(std::move(observer));
}

void HostFrameSinkManager::CreateVideoCapturer(
    mojo::PendingReceiver<mojom::FrameSinkVideoCapturer> receiver) {
  frame_sink_manager_->CreateVideoCapturer(std::move(receiver));
}

std::unique_ptr<ClientFrameSinkVideoCapturer>
HostFrameSinkManager::CreateVideoCapturer() {
  return std::make_unique<ClientFrameSinkVideoCapturer>(base::BindRepeating(
      [](base::WeakPtr<HostFrameSinkManager> self,
         mojo::PendingReceiver<mojom::FrameSinkVideoCapturer> receiver) {
        self->CreateVideoCapturer(std::move(receiver));
      },
      weak_ptr_factory_.GetWeakPtr()));
}

void HostFrameSinkManager::EvictSurfaces(
    const std::vector<SurfaceId>& surface_ids) {
  frame_sink_manager_->EvictSurfaces(surface_ids);
}

void HostFrameSinkManager::RequestCopyOfOutput(
    const SurfaceId& surface_id,
    std::unique_ptr<CopyOutputRequest> request,
    bool capture_exact_surface_id) {
  frame_sink_manager_->RequestCopyOfOutput(surface_id, std::move(request),
                                           capture_exact_surface_id);
}

void HostFrameSinkManager::Throttle(const std::vector<FrameSinkId>& ids,
                                    base::TimeDelta interval) {
  frame_sink_manager_->Throttle(ids, interval);
}

void HostFrameSinkManager::StartThrottlingAllFrameSinks(
    base::TimeDelta interval) {
  frame_sink_manager_->StartThrottlingAllFrameSinks(interval);
}

void HostFrameSinkManager::StopThrottlingAllFrameSinks() {
  frame_sink_manager_->StopThrottlingAllFrameSinks();
}

void HostFrameSinkManager::AddHitTestRegionObserver(
    HitTestRegionObserver* observer) {
  observers_.AddObserver(observer);
}

void HostFrameSinkManager::RemoveHitTestRegionObserver(
    HitTestRegionObserver* observer) {
  observers_.RemoveObserver(observer);
}

void HostFrameSinkManager::OnConnectionLost() {
  connection_was_lost_ = true;

  receiver_.reset();
  // frame_sink_manager_ points to |frame_sink_manager_remote_| if using mojo.
  // Set frame_sink_manager_ to nullptr before
  // frame_sink_manager_remote_.reset() to avoid dangling ptr.
  frame_sink_manager_ = nullptr;
  frame_sink_manager_remote_.reset();

  // Any cached back buffers are invalid once the connection to the
  // FrameSinkManager is lost.
  min_valid_cache_back_buffer_id_ = next_cache_back_buffer_id_;

  // CompositorFrameSinks are lost along with the connection to
  // mojom::FrameSinkManager.
  for (auto& map_entry : frame_sink_data_map_) {
    map_entry.second.has_created_compositor_frame_sink = false;
    map_entry.second.wait_on_destruction = false;
  }

  if (!connection_lost_callback_.is_null())
    connection_lost_callback_.Run();
}

void HostFrameSinkManager::RegisterAfterConnectionLoss() {
  // Register FrameSinkIds first.
  for (auto& map_entry : frame_sink_data_map_) {
    const FrameSinkId& frame_sink_id = map_entry.first;
    FrameSinkData& data = map_entry.second;
    if (data.client) {
      frame_sink_manager_->RegisterFrameSinkId(
          frame_sink_id,
          data.report_activation == ReportFirstSurfaceActivation::kYes);
    }
    if (!data.debug_label.empty()) {
      frame_sink_manager_->SetFrameSinkDebugLabel(frame_sink_id,
                                                  data.debug_label);
    }
  }

  // Register FrameSink hierarchy second.
  for (auto& map_entry : frame_sink_data_map_) {
    const FrameSinkId& frame_sink_id = map_entry.first;
    FrameSinkData& data = map_entry.second;
    for (auto& child_frame_sink_id : data.children) {
      frame_sink_manager_->RegisterFrameSinkHierarchy(frame_sink_id,
                                                      child_frame_sink_id);
    }
  }
}

void HostFrameSinkManager::OnFirstSurfaceActivation(
    const SurfaceInfo& surface_info) {

  auto it = frame_sink_data_map_.find(surface_info.id().frame_sink_id());

  // If we've received a bogus or stale SurfaceId from Viz then just ignore it.
  if (it == frame_sink_data_map_.end())
    return;

  FrameSinkData& frame_sink_data = it->second;
  if (frame_sink_data.client)
    frame_sink_data.client->OnFirstSurfaceActivation(surface_info);
}

void HostFrameSinkManager::OnAggregatedHitTestRegionListUpdated(
    const FrameSinkId& frame_sink_id,
    const std::vector<AggregatedHitTestRegion>& hit_test_data) {
  auto iter = display_hit_test_query_.find(frame_sink_id);
  // The corresponding HitTestQuery has already been deleted, so drop the
  // in-flight hit-test data.
  if (iter == display_hit_test_query_.end())
    return;

  iter->second->OnAggregatedHitTestRegionListUpdated(hit_test_data);

  // Ensure that HitTestQuery are updated so that observers are not working with
  // stale data.
  for (HitTestRegionObserver& observer : observers_)
    observer.OnAggregatedHitTestRegionListUpdated(frame_sink_id, hit_test_data);
}

uint32_t HostFrameSinkManager::CacheBackBufferForRootSink(
    const FrameSinkId& root_sink_id) {
  auto it = frame_sink_data_map_.find(root_sink_id);
  DCHECK(it != frame_sink_data_map_.end());
  DCHECK(it->second.is_root);
  DCHECK(it->second.IsFrameSinkRegistered());
  DCHECK(frame_sink_manager_remote_);

  uint32_t cache_id = next_cache_back_buffer_id_++;
  frame_sink_manager_remote_->CacheBackBuffer(cache_id, root_sink_id);
  return cache_id;
}

void HostFrameSinkManager::EvictCachedBackBuffer(uint32_t cache_id) {
  DCHECK(frame_sink_manager_remote_);

  if (cache_id < min_valid_cache_back_buffer_id_)
    return;

  // This synchronous call ensures that the GL context/surface that draw to
  // the platform window (eg. XWindow or HWND) get destroyed before the
  // platform window is destroyed.
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  frame_sink_manager_remote_->EvictBackBuffer(cache_id);
}

void HostFrameSinkManager::CreateHitTestQueryForSynchronousCompositor(
    const FrameSinkId& frame_sink_id) {
  display_hit_test_query_[frame_sink_id] = std::make_unique<HitTestQuery>();
}
void HostFrameSinkManager::EraseHitTestQueryForSynchronousCompositor(
    const FrameSinkId& frame_sink_id) {
  display_hit_test_query_.erase(frame_sink_id);
}

void HostFrameSinkManager::UpdateDebugRendererSettings(
    const DebugRendererSettings& debug_settings) {
  debug_renderer_settings_ = debug_settings;
  frame_sink_manager_->UpdateDebugRendererSettings(debug_settings);
}

void HostFrameSinkManager::StartFrameCountingForTest(
    base::TimeTicks start_time,
    base::TimeDelta bucket_size) {
  frame_sink_manager_->StartFrameCountingForTest(start_time,  // IN-TEST
                                                 bucket_size);
}

void HostFrameSinkManager::StopFrameCountingForTest(
    mojom::FrameSinkManager::StopFrameCountingForTestCallback callback) {
  frame_sink_manager_->StopFrameCountingForTest(  // IN-TEST
      std::move(callback));
}

HostFrameSinkManager::FrameSinkData::FrameSinkData() = default;

HostFrameSinkManager::FrameSinkData::FrameSinkData(FrameSinkData&& other) =
    default;

HostFrameSinkManager::FrameSinkData::~FrameSinkData() = default;

HostFrameSinkManager::FrameSinkData& HostFrameSinkManager::FrameSinkData::
operator=(FrameSinkData&& other) = default;

}  // namespace viz
