// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/constants.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_impl.h"
#include "components/viz/service/frame_sinks/frame_sink_observer.h"
#include "components/viz/service/frame_sinks/root_compositor_frame_sink_impl.h"
#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_manager.h"
#include "components/viz/service/frame_sinks/video_detector.h"
#include "components/viz/service/hit_test/hit_test_aggregator_delegate.h"
#include "components/viz/service/hit_test/hit_test_manager.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/service/surfaces/surface_manager_delegate.h"
#include "components/viz/service/surfaces/surface_observer.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "services/viz/public/mojom/compositing/video_detector_observer.mojom.h"

namespace viz {

class CapturableFrameSink;
class CompositorFrameSinkSupport;
class OutputSurfaceProvider;
class SharedBitmapManager;

// FrameSinkManagerImpl manages BeginFrame hierarchy. This is the implementation
// detail for FrameSinkManagerImpl.
class VIZ_SERVICE_EXPORT FrameSinkManagerImpl
    : public SurfaceObserver,
      public FrameSinkVideoCapturerManager,
      public mojom::FrameSinkManager,
      public HitTestAggregatorDelegate,
      public SurfaceManagerDelegate {
 public:
  struct VIZ_SERVICE_EXPORT InitParams {
    InitParams();
    InitParams(SharedBitmapManager* shared_bitmap_manager,
               OutputSurfaceProvider* output_surface_provider);
    InitParams(InitParams&& other);
    ~InitParams();
    InitParams& operator=(InitParams&& other);

    SharedBitmapManager* shared_bitmap_manager = nullptr;
    base::Optional<uint32_t> activation_deadline_in_frames =
        kDefaultActivationDeadlineInFrames;
    OutputSurfaceProvider* output_surface_provider = nullptr;
    uint32_t restart_id = BeginFrameSource::kNotRestartableId;
    bool run_all_compositor_stages_before_draw = false;
  };
  explicit FrameSinkManagerImpl(const InitParams& params);
  // TODO(kylechar): Cleanup tests and remove this constructor.
  FrameSinkManagerImpl(
      SharedBitmapManager* shared_bitmap_manager,
      OutputSurfaceProvider* output_surface_provider = nullptr);
  ~FrameSinkManagerImpl() override;

  // Performs cleanup needed to force shutdown from the GPU process. Stops all
  // incoming IPCs and destroys all [Root]CompositorFrameSinkImpls.
  void ForceShutdown();

  // Binds |this| as a FrameSinkManagerImpl for |receiver| on |task_runner|. On
  // Mac |task_runner| will be the resize helper task runner. May only be called
  // once.
  void BindAndSetClient(
      mojo::PendingReceiver<mojom::FrameSinkManager> receiver,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      mojo::PendingRemote<mojom::FrameSinkManagerClient> client);

  // Sets up a direction connection to |client| without using Mojo.
  void SetLocalClient(
      mojom::FrameSinkManagerClient* client,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner = nullptr);

  // mojom::FrameSinkManager implementation:
  void RegisterFrameSinkId(const FrameSinkId& frame_sink_id,
                           bool report_activation) override;
  void InvalidateFrameSinkId(const FrameSinkId& frame_sink_id) override;
  void EnableSynchronizationReporting(
      const FrameSinkId& frame_sink_id,
      const std::string& reporting_label) override;
  void SetFrameSinkDebugLabel(const FrameSinkId& frame_sink_id,
                              const std::string& debug_label) override;
  void CreateRootCompositorFrameSink(
      mojom::RootCompositorFrameSinkParamsPtr params) override;
  void CreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      mojo::PendingReceiver<mojom::CompositorFrameSink> receiver,
      mojo::PendingRemote<mojom::CompositorFrameSinkClient> client) override;
  void DestroyCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      DestroyCompositorFrameSinkCallback callback) override;
  void RegisterFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) override;
  void UnregisterFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) override;
  void AddVideoDetectorObserver(
      mojo::PendingRemote<mojom::VideoDetectorObserver> observer) override;
  void CreateVideoCapturer(
      mojo::PendingReceiver<mojom::FrameSinkVideoCapturer> receiver) override;
  void EvictSurfaces(const std::vector<SurfaceId>& surface_ids) override;
  void RequestCopyOfOutput(const SurfaceId& surface_id,
                           std::unique_ptr<CopyOutputRequest> request) override;
  void SetHitTestAsyncQueriedDebugRegions(
      const FrameSinkId& root_frame_sink_id,
      const std::vector<FrameSinkId>& hit_test_async_queried_debug_queue)
      override;
  void CacheBackBuffer(uint32_t cache_id,
                       const FrameSinkId& root_frame_sink_id) override;
  void EvictBackBuffer(uint32_t cache_id,
                       EvictBackBufferCallback callback) override;

  // SurfaceObserver implementation.
  void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) override;
  void OnSurfaceActivated(const SurfaceId& surface_id,
                          base::Optional<base::TimeDelta> duration) override;
  bool OnSurfaceDamaged(const SurfaceId& surface_id,
                        const BeginFrameAck& ack) override;
  void OnSurfaceDestroyed(const SurfaceId& surface_id) override;
  void OnSurfaceMarkedForDestruction(const SurfaceId& surface_id) override;
  void OnSurfaceDamageExpected(const SurfaceId& surface_id,
                               const BeginFrameArgs& args) override;

  // HitTestAggregatorDelegate implementation:
  void OnAggregatedHitTestRegionListUpdated(
      const FrameSinkId& frame_sink_id,
      const std::vector<AggregatedHitTestRegion>& hit_test_data) override;

  // SurfaceManagerDelegate implementation:
  base::StringPiece GetFrameSinkDebugLabel(
      const FrameSinkId& frame_sink_id) const override;

  // CompositorFrameSinkSupport, hierarchy, and BeginFrameSource can be
  // registered and unregistered in any order with respect to each other.
  //
  // This happens in practice, e.g. the relationship to between ui::Compositor /
  // DelegatedFrameHost is known before ui::Compositor has a surface/client).
  // However, DelegatedFrameHost can register itself as a client before its
  // relationship with the ui::Compositor is known.

  // Registers a CompositorFrameSinkSupport for |frame_sink_id|. |frame_sink_id|
  // must be unregistered when |support| is destroyed.
  void RegisterCompositorFrameSinkSupport(const FrameSinkId& frame_sink_id,
                                          CompositorFrameSinkSupport* support);
  void UnregisterCompositorFrameSinkSupport(const FrameSinkId& frame_sink_id);

  // Associates a |source| with a particular framesink.  That framesink and
  // any children of that framesink with valid clients can potentially use
  // that |source|.
  void RegisterBeginFrameSource(BeginFrameSource* source,
                                const FrameSinkId& frame_sink_id);
  void UnregisterBeginFrameSource(BeginFrameSource* source);

  SurfaceManager* surface_manager() { return &surface_manager_; }
  const HitTestManager* hit_test_manager() { return &hit_test_manager_; }
  SharedBitmapManager* shared_bitmap_manager() {
    return shared_bitmap_manager_;
  }

  void SubmitHitTestRegionList(
      const SurfaceId& surface_id,
      uint64_t frame_index,
      base::Optional<HitTestRegionList> hit_test_region_list);

  // Instantiates |video_detector_| for tests where we simulate the passage of
  // time.
  VideoDetector* CreateVideoDetectorForTesting(
      const base::TickClock* tick_clock,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  void OnFrameTokenChangedDirect(const FrameSinkId& frame_sink_id,
                                 uint32_t frame_token);

  // Called when |frame_token| is changed on a submitted CompositorFrame.
  void OnFrameTokenChanged(const FrameSinkId& frame_sink_id,
                           uint32_t frame_token);

  void DidBeginFrame(const FrameSinkId& frame_sink_id,
                     const BeginFrameArgs& args);
  void DidFinishFrame(const FrameSinkId& frame_sink_id,
                      const BeginFrameArgs& args);

  void AddObserver(FrameSinkObserver* obs);
  void RemoveObserver(FrameSinkObserver* obs);

  // Returns ids of all FrameSinks that were registered.
  std::vector<FrameSinkId> GetRegisteredFrameSinkIds() const;

  // Returns children of a FrameSink that has |parent_frame_sink_id|.
  // Returns an empty set if a parent doesn't have any children.
  base::flat_set<FrameSinkId> GetChildrenByParent(
      const FrameSinkId& parent_frame_sink_id) const;
  const CompositorFrameSinkSupport* GetFrameSinkForId(
      const FrameSinkId& frame_sink_id) const;

  void SetPreferredFrameIntervalForFrameSinkId(const FrameSinkId& id,
                                               base::TimeDelta interval);
  base::TimeDelta GetPreferredFrameIntervalForFrameSinkId(
      const FrameSinkId& id) const;

 private:
  friend class FrameSinkManagerTest;

  // Metadata for a CompositorFrameSink.
  struct FrameSinkData {
    explicit FrameSinkData(bool report_activation);
    FrameSinkData(FrameSinkData&& other);
    ~FrameSinkData();
    FrameSinkData& operator=(FrameSinkData&& other);

    // A label to identify frame sink.
    std::string debug_label;

    // Record synchronization events for this FrameSinkId if not empty.
    std::string synchronization_label;

    // Indicates whether the client wishes to receive FirstSurfaceActivation
    // notification.
    bool report_activation;

    base::TimeDelta preferred_frame_interval = BeginFrameArgs::MinInterval();

   private:
    DISALLOW_COPY_AND_ASSIGN(FrameSinkData);
  };

  // BeginFrameSource routing information for a FrameSinkId.
  struct FrameSinkSourceMapping {
    FrameSinkSourceMapping();
    FrameSinkSourceMapping(FrameSinkSourceMapping&& other);
    ~FrameSinkSourceMapping();
    FrameSinkSourceMapping& operator=(FrameSinkSourceMapping&& other);

    // The currently assigned begin frame source for this client.
    BeginFrameSource* source = nullptr;
    // This represents a dag of parent -> children mapping.
    base::flat_set<FrameSinkId> children;

   private:
    DISALLOW_COPY_AND_ASSIGN(FrameSinkSourceMapping);
  };

  void RecursivelyAttachBeginFrameSource(const FrameSinkId& frame_sink_id,
                                         BeginFrameSource* source);
  void RecursivelyDetachBeginFrameSource(const FrameSinkId& frame_sink_id,
                                         BeginFrameSource* source);

  // FrameSinkVideoCapturerManager implementation:
  CapturableFrameSink* FindCapturableFrameSink(
      const FrameSinkId& frame_sink_id) override;
  void OnCapturerConnectionLost(FrameSinkVideoCapturerImpl* capturer) override;

  // Returns true if |child framesink| is or has |search_frame_sink_id| as a
  // child.
  bool ChildContains(const FrameSinkId& child_frame_sink_id,
                     const FrameSinkId& search_frame_sink_id) const;

  // SharedBitmapManager for the viz display service for receiving software
  // resources in CompositorFrameSinks.
  SharedBitmapManager* const shared_bitmap_manager_;

  // Provides an output surface for CreateRootCompositorFrameSink().
  OutputSurfaceProvider* const output_surface_provider_;

  SurfaceManager surface_manager_;

  // Must be created after and destroyed before |surface_manager_|.
  HitTestManager hit_test_manager_;

  // Restart id to generate unique begin frames across process restarts.  Used
  // for creating a BeginFrameSource for RootCompositorFrameSink.
  const uint32_t restart_id_;

  // Whether display scheduler should wait for all pipeline stages before draw.
  const bool run_all_compositor_stages_before_draw_;

  // Contains registered frame sink ids, debug labels and synchronization
  // labels. Map entries will be created when frame sink is registered and
  // destroyed when frame sink is invalidated.
  base::flat_map<FrameSinkId, FrameSinkData> frame_sink_data_;

  // Set of BeginFrameSource along with associated FrameSinkIds. Any child
  // that is implicitly using this frame sink must be reachable by the
  // parent in the dag.
  base::flat_map<BeginFrameSource*, FrameSinkId> registered_sources_;

  // Contains FrameSinkId hierarchy and BeginFrameSource mapping.
  base::flat_map<FrameSinkId, FrameSinkSourceMapping> frame_sink_source_map_;

  // CompositorFrameSinkSupports get added to this map on creation and removed
  // on destruction.
  base::flat_map<FrameSinkId, CompositorFrameSinkSupport*> support_map_;

  // [Root]CompositorFrameSinkImpls are owned in these maps.
  base::flat_map<FrameSinkId, std::unique_ptr<RootCompositorFrameSinkImpl>>
      root_sink_map_;
  base::flat_map<FrameSinkId, std::unique_ptr<CompositorFrameSinkImpl>>
      sink_map_;

  base::flat_set<std::unique_ptr<FrameSinkVideoCapturerImpl>,
                 base::UniquePtrComparator>
      video_capturers_;

  base::flat_map<uint32_t, base::ScopedClosureRunner> cached_back_buffers_;

  THREAD_CHECKER(thread_checker_);

  // |video_detector_| is instantiated lazily in order to avoid overhead on
  // platforms that don't need video detection.
  std::unique_ptr<VideoDetector> video_detector_;

  // There are three states this can be in:
  //  1. Mojo client: |client_| will point to |client_remote_|, the Mojo client,
  //     and |ui_task_runner_| will not be used. Calls to OnFrameTokenChanged()
  //     will go through Mojo. This is the normal state.
  //  2. Local (directly connected) client, *with* task runner: |client_| will
  //     point to the client, |client_remote_| will not be bound to any remote
  //     client, and calls to OnFrameTokenChanged() will be PostTasked using
  //     |ui_task_runner_|. Used mostly for layout tests.
  //  3. Local (directly connected) client, *without* task runner: |client_|
  //     will point to the client, |client_remote_| will not be bound to any
  //     remote client and |ui_task_runner_| will be nullptr, and calls to
  //     OnFrameTokenChanged() will be directly called (without PostTask) on
  //     |client_|. Used for some unit tests.
  mojom::FrameSinkManagerClient* client_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_ = nullptr;
  mojo::Remote<mojom::FrameSinkManagerClient> client_remote_;
  mojo::Receiver<mojom::FrameSinkManager> receiver_{this};

  base::ObserverList<FrameSinkObserver>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(FrameSinkManagerImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_IMPL_H_
