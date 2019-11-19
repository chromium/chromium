// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_HOST_FRAME_SINK_MANAGER_H_
#define COMPONENTS_VIZ_HOST_HOST_FRAME_SINK_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "components/viz/host/hit_test/hit_test_query.h"
#include "components/viz/host/hit_test/hit_test_region_observer.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "components/viz/host/viz_host_export.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace viz {

class CompositorFrameSinkSupport;
class FrameSinkManagerImpl;
class SurfaceInfo;

enum class ReportFirstSurfaceActivation { kYes, kNo };

// Browser side wrapper of mojom::FrameSinkManager, to be used from the
// UI thread. Manages frame sinks and is intended to replace all usage of
// FrameSinkManagerImpl.
class VIZ_HOST_EXPORT HostFrameSinkManager
    : public mojom::FrameSinkManagerClient,
      public CompositorFrameSinkSupportManager {
 public:
  using DisplayHitTestQueryMap =
      base::flat_map<FrameSinkId, std::unique_ptr<HitTestQuery>>;

  HostFrameSinkManager();
  ~HostFrameSinkManager() override;

  const DisplayHitTestQueryMap& display_hit_test_query() const {
    return display_hit_test_query_;
  }

  // Sets a local FrameSinkManagerImpl instance and connects directly to it.
  void SetLocalManager(FrameSinkManagerImpl* frame_sink_manager_impl);

  // Binds |this| as a FrameSinkManagerClient for |receiver| on |task_runner|.
  // On Mac |task_runner| will be the resize helper task runner. May only be
  // called once. If |task_runner| is null, it uses the default mojo task runner
  // for the thread this call is made on.
  void BindAndSetManager(
      mojo::PendingReceiver<mojom::FrameSinkManagerClient> receiver,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      mojo::PendingRemote<mojom::FrameSinkManager> remote);

  // Sets a callback to be notified when the connection to the FrameSinkManager
  // on |frame_sink_manager_remote_| is lost.
  void SetConnectionLostCallback(base::RepeatingClosure callback);

  // Sets a callback to be notified after Viz sent bad message to Viz host.
  void SetBadMessageReceivedFromGpuCallback(base::RepeatingClosure callback);

  // Registers |frame_sink_id| so that a client can submit CompositorFrames
  // using it. This must be called before creating a CompositorFrameSink or
  // registering FrameSinkId hierarchy.
  //
  // When the client is done submitting CompositorFrames to |frame_sink_id| then
  // InvalidateFrameSink() should be called.
  void RegisterFrameSinkId(const FrameSinkId& frame_sink_id,
                           HostFrameSinkClient* client,
                           ReportFirstSurfaceActivation report_activation);

  // Returns true if RegisterFrameSinkId() was called with |frame_sink_id| and
  // InvalidateFrameSinkId() has not been called.
  bool IsFrameSinkIdRegistered(const FrameSinkId& frame_sink_id) const;

  // Invalidates |frame_sink_id| when the client is done submitting
  // CompositorFrames. If there is a CompositorFrameSink for |frame_sink_id|
  // then it will be destroyed and the message pipe to the client will be
  // closed.
  //
  // It's expected, but not enforced, that RegisterFrameSinkId() will never be
  // called for |frame_sink_id| again. This is to avoid problems with re-entrant
  // code. If the same client wants to submit CompositorFrames later a new
  // FrameSinkId should be allocated.
  void InvalidateFrameSinkId(const FrameSinkId& frame_sink_id);

  // Tells FrameSinkManger to report when a synchronization event completes via
  // tracing and UMA and the duration of that event. A synchronization event
  // occurs when a CompositorFrame submitted to the CompositorFrameSink
  // specified by |frame_sink_id| activates after having been blocked by
  // unresolved dependencies.
  void EnableSynchronizationReporting(const FrameSinkId& frame_sink_id,
                                      const std::string& reporting_label);

  // |debug_label| is used when printing out the surface hierarchy so we know
  // which clients are contributing which surfaces.
  void SetFrameSinkDebugLabel(const FrameSinkId& frame_sink_id,
                              const std::string& debug_label);

  // Creates a connection from a display root to viz. Provides the same
  // interfaces as CreateCompositorFramesink() plus the priviledged
  // DisplayPrivate and (if requested) ExternalBeginFrameController interfaces.
  // When no longer needed, call InvalidateFrameSinkId().
  //
  // If there is already a CompositorFrameSink for |frame_sink_id| then calling
  // this will destroy the existing CompositorFrameSink and create a new one.
  void CreateRootCompositorFrameSink(
      mojom::RootCompositorFrameSinkParamsPtr params);

  // Creates a connection from a client to viz, using |request| and |client|,
  // that allows the client to submit CompositorFrames. When no longer needed,
  // call InvalidateFrameSinkId().
  //
  // If there is already a CompositorFrameSink for |frame_sink_id| then calling
  // this will destroy the existing CompositorFrameSink and create a new one.
  void CreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      mojo::PendingReceiver<mojom::CompositorFrameSink> receiver,
      mojo::PendingRemote<mojom::CompositorFrameSinkClient> client);

  // Registers FrameSink hierarchy. It's expected that the parent will embed
  // the child. If |parent_frame_sink_id| is registered then it will be added as
  // a parent of |child_frame_sink_id| and the function will return true. If
  // |parent_frame_sink_id| is not registered then the function will return
  // false. A frame sink can have multiple parents.
  bool RegisterFrameSinkHierarchy(const FrameSinkId& parent_frame_sink_id,
                                  const FrameSinkId& child_frame_sink_id);

  // Unregisters FrameSink hierarchy. Client must have registered frame sink
  // hierarchy before unregistering.
  void UnregisterFrameSinkHierarchy(const FrameSinkId& parent_frame_sink_id,
                                    const FrameSinkId& child_frame_sink_id);

  // Returns true if RegisterFrameSinkHierarchy() was called with the supplied
  // arguments.
  bool IsFrameSinkHierarchyRegistered(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) const;

  // Returns the first ancestor of |start| (including |start|) that is a root.
  base::Optional<FrameSinkId> FindRootFrameSinkId(
      const FrameSinkId& start) const;

  // Asks viz to send updates regarding video activity to |observer|.
  void AddVideoDetectorObserver(
      mojo::PendingRemote<mojom::VideoDetectorObserver> observer);

  // Creates a FrameSinkVideoCapturer instance in viz.
  void CreateVideoCapturer(
      mojo::PendingReceiver<mojom::FrameSinkVideoCapturer> receiver);

  // Creates a FrameSinkVideoCapturer instance in viz and returns a
  // ClientFrameSinkVideoCapturer that's connected to it. Clients should prefer
  // this version because ClientFrameSinkVideoCapturer is resilient to viz
  // crashes.
  std::unique_ptr<ClientFrameSinkVideoCapturer> CreateVideoCapturer();

  // Marks the given SurfaceIds for destruction.
  void EvictSurfaces(const std::vector<SurfaceId>& surface_ids);

  // Takes snapshot of a |surface_id| or a newer surface with the same
  // FrameSinkId. The FrameSinkId is used to identify which frame we're
  // interested in. The snapshot will only be taken if the LocalSurfaceId is at
  // least the given LocalSurfaceId (|surface_id.local_frame_id()|). If the
  // LocalSurfaceId is lower than the given id, then the request is queued up to
  // be executed later.
  void RequestCopyOfOutput(const SurfaceId& surface_id,
                           std::unique_ptr<CopyOutputRequest> request);

  // Add/Remove an observer to receive notifications of when the host receives
  // new hit test data.
  void AddHitTestRegionObserver(HitTestRegionObserver* observer);
  void RemoveHitTestRegionObserver(HitTestRegionObserver* observer);

  // CompositorFrameSinkSupportManager:
  std::unique_ptr<CompositorFrameSinkSupport> CreateCompositorFrameSinkSupport(
      mojom::CompositorFrameSinkClient* client,
      const FrameSinkId& frame_sink_id,
      bool is_root,
      bool needs_sync_points) override;

  void OnFrameTokenChanged(const FrameSinkId& frame_sink_id,
                           uint32_t frame_token) override;

  void SetHitTestAsyncQueriedDebugRegions(
      const FrameSinkId& root_frame_sink_id,
      const std::vector<FrameSinkId>& hit_test_async_queried_debug_queue);

  // Preserves the back buffer associated with the |root_sink_id|, even after
  // the associated Display has been torn down, and returns an id for this cache
  // entry.
  uint32_t CacheBackBufferForRootSink(const FrameSinkId& root_sink_id);
  void EvictCachedBackBuffer(uint32_t cache_id);

 private:
  friend class HostFrameSinkManagerTestApi;
  friend class HostFrameSinkManagerTestBase;

  struct FrameSinkData {
    FrameSinkData();
    FrameSinkData(FrameSinkData&& other);
    ~FrameSinkData();
    FrameSinkData& operator=(FrameSinkData&& other);

    bool IsFrameSinkRegistered() const { return client != nullptr; }

    // Returns true if there is nothing in FrameSinkData and it can be deleted.
    bool IsEmpty() const {
      return !IsFrameSinkRegistered() && !has_created_compositor_frame_sink &&
             parents.empty() && children.empty();
    }

    // The client to be notified of changes to this FrameSink.
    HostFrameSinkClient* client = nullptr;

    // Indicates whether or not this client cares to receive
    // FirstSurfaceActivation notifications.
    ReportFirstSurfaceActivation report_activation =
        ReportFirstSurfaceActivation::kYes;

    // The label to use whether this client would like reporting for
    // synchronization events.
    std::string synchronization_reporting_label;

    // The name of the HostFrameSinkClient used for debug purposes.
    std::string debug_label;

    // If the frame sink is a root that corresponds to a Display.
    bool is_root = false;

    // If a mojom::CompositorFrameSink was created for this FrameSinkId. This
    // will always be false if not using Mojo.
    bool has_created_compositor_frame_sink = false;

    // Track frame sink hierarchy in both directions.
    std::vector<FrameSinkId> parents;
    std::vector<FrameSinkId> children;

   private:
    DISALLOW_COPY_AND_ASSIGN(FrameSinkData);
  };

  // Handles connection loss to |frame_sink_manager_remote_|. This should only
  // happen when the GPU process crashes.
  void OnConnectionLost();

  // Registers FrameSinkId and FrameSink hierarchy again after connection loss.
  void RegisterAfterConnectionLoss();

  // mojom::FrameSinkManagerClient:
  void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) override;
  void OnAggregatedHitTestRegionListUpdated(
      const FrameSinkId& frame_sink_id,
      const std::vector<AggregatedHitTestRegion>& hit_test_data) override;

  // This will point to |frame_sink_manager_remote_| if using Mojo or
  // |frame_sink_manager_impl_| if directly connected. Use this to make function
  // calls.
  mojom::FrameSinkManager* frame_sink_manager_ = nullptr;

  // Mojo connection to the FrameSinkManager. If this is bound then
  // |frame_sink_manager_impl_| must be null.
  mojo::Remote<mojom::FrameSinkManager> frame_sink_manager_remote_;

  // Mojo connection back from the FrameSinkManager.
  mojo::Receiver<mojom::FrameSinkManagerClient> receiver_{this};

  // A direct connection to FrameSinkManagerImpl. If this is set then
  // |frame_sink_manager_remote_| must be unbound. For use in browser process
  // only, viz process should not set this.
  FrameSinkManagerImpl* frame_sink_manager_impl_ = nullptr;

  // Per CompositorFrameSink data.
  std::unordered_map<FrameSinkId, FrameSinkData, FrameSinkIdHash>
      frame_sink_data_map_;

  // If |frame_sink_manager_remote_| connection was lost.
  bool connection_was_lost_ = false;

  base::RepeatingClosure connection_lost_callback_;

  DisplayHitTestQueryMap display_hit_test_query_;

  // TODO(jonross): Separate out all hit testing work into its own separate
  // class.
  base::ObserverList<HitTestRegionObserver>::Unchecked observers_;

  uint32_t next_cache_back_buffer_id_ = 1;
  uint32_t min_valid_cache_back_buffer_id_ = 1;

  base::WeakPtrFactory<HostFrameSinkManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HostFrameSinkManager);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_HOST_FRAME_SINK_MANAGER_H_
