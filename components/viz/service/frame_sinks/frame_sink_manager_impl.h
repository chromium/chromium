// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/viz/common/constants.h"
#include "components/viz/common/surfaces/frame_sink_bundle_id.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_impl.h"
#include "components/viz/service/frame_sinks/frame_counter.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace viz {

class CapturableFrameSink;
class CompositorFrameSinkSupport;
class FrameSinkBundleImpl;
class GmbVideoFramePoolContextProvider;
class HintSessionFactory;
class OutputSurfaceProvider;
class SharedBitmapManager;
struct VideoCaptureTarget;

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
    explicit InitParams(
        SharedBitmapManager* shared_bitmap_manager,
        OutputSurfaceProvider* output_surface_provider = nullptr,
        GmbVideoFramePoolContextProvider* gmb_context_provider = nullptr);
    InitParams(InitParams&& other);
    ~InitParams();
    InitParams& operator=(InitParams&& other);

    raw_ptr<SharedBitmapManager> shared_bitmap_manager = nullptr;
    absl::optional<uint32_t> activation_deadline_in_frames =
        kDefaultActivationDeadlineInFrames;
    raw_ptr<OutputSurfaceProvider> output_surface_provider = nullptr;
    raw_ptr<GmbVideoFramePoolContextProvider> gmb_context_provider = nullptr;
    uint32_t restart_id = BeginFrameSource::kNotRestartableId;
    bool run_all_compositor_stages_before_draw = false;
    bool log_capture_pipeline_in_webrtc = false;
    DebugRendererSettings debug_renderer_settings;
    base::ProcessId host_process_id = base::kNullProcessId;
    raw_ptr<HintSessionFactory> hint_session_factory = nullptr;
    size_t max_uncommitted_frames = 0;
  };
  explicit FrameSinkManagerImpl(const InitParams& params);

  FrameSinkManagerImpl(const FrameSinkManagerImpl&) = delete;
  FrameSinkManagerImpl& operator=(const FrameSinkManagerImpl&) = delete;

  ~FrameSinkManagerImpl() override;

  CompositorFrameSinkImpl* GetFrameSinkImpl(const FrameSinkId& id);
  FrameSinkBundleImpl* GetFrameSinkBundle(const FrameSinkBundleId& id);

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
  void SetFrameSinkDebugLabel(const FrameSinkId& frame_sink_id,
                              const std::string& debug_label) override;
  void CreateRootCompositorFrameSink(
      mojom::RootCompositorFrameSinkParamsPtr params) override;
  void CreateFrameSinkBundle(
      const FrameSinkBundleId& bundle_id,
      mojo::PendingReceiver<mojom::FrameSinkBundle> receiver,
      mojo::PendingRemote<mojom::FrameSinkBundleClient> client) override;
  void CreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      const absl::optional<FrameSinkBundleId>& bundle_id,
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
  void CacheBackBuffer(uint32_t cache_id,
                       const FrameSinkId& root_frame_sink_id) override;
  void EvictBackBuffer(uint32_t cache_id,
                       EvictBackBufferCallback callback) override;
  void UpdateDebugRendererSettings(
      const DebugRendererSettings& debug_settings) override;
  void Throttle(const std::vector<FrameSinkId>& ids,
                base::TimeDelta interval) override;
  void StartThrottlingAllFrameSinks(base::TimeDelta interval) override;
  void StopThrottlingAllFrameSinks() override;
  void StartFrameCountingForTest(base::TimeTicks start_time,
                                 base::TimeDelta bucket_size) override;
  void StopFrameCountingForTest(
      StopFrameCountingForTestCallback callback) override;

  void DestroyFrameSinkBundle(const FrameSinkBundleId& id);

  // SurfaceObserver implementation.
  void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) override;

  // HitTestAggregatorDelegate implementation:
  void OnAggregatedHitTestRegionListUpdated(
      const FrameSinkId& frame_sink_id,
      const std::vector<AggregatedHitTestRegion>& hit_test_data) override;

  // SurfaceManagerDelegate implementation:
  base::StringPiece GetFrameSinkDebugLabel(
      const FrameSinkId& frame_sink_id) const override;
  void AggregatedFrameSinksChanged() override;

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
      absl::optional<HitTestRegionList> hit_test_region_list);

  // Instantiates |video_detector_| for tests where we simulate the passage of
  // time.
  VideoDetector* CreateVideoDetectorForTesting(
      const base::TickClock* tick_clock,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  void OnFrameTokenChangedDirect(const FrameSinkId& frame_sink_id,
                                 uint32_t frame_token,
                                 base::TimeTicks activation_time);

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
  CompositorFrameSinkSupport* GetFrameSinkForId(
      const FrameSinkId& frame_sink_id) const;

  base::TimeDelta GetPreferredFrameIntervalForFrameSinkId(
      const FrameSinkId& id,
      mojom::CompositorFrameSinkType* type) const;

  // This cancels pending output requests owned by the frame sinks associated
  // with the specified BeginFrameSource.
  // The requets callback will be fired as part of request destruction.
  // This may be used in case we know a frame can't be produced any time soon,
  // so there's no point for caller to wait for the copy of output.
  void DiscardPendingCopyOfOutputRequests(const BeginFrameSource* source);

  // Called when video capture starts on the target frame sink with |id|.
  void OnCaptureStarted(const FrameSinkId& id);
  // Called when video capture stops on the target frame sink with |id|.
  void OnCaptureStopped(const FrameSinkId& id);

  // Returns true if thread IDs do not belong to this process or the host (ie
  // browser) process. Note this also returns false on any unexpected errors.
  // Only implemented on Android.
  bool VerifySandboxedThreadIds(
      base::flat_set<base::PlatformThreadId> thread_ids);

  // Manages transferring ownership of SurfaceAnimationManager for
  // cross-document navigations where a transition could be initiated on one
  // CompositorFrameSink but animations are executed on a different
  // CompositorFrameSink.
  void CacheSurfaceAnimationManager(
      NavigationID navigation_id,
      std::unique_ptr<SurfaceAnimationManager> manager);
  std::unique_ptr<SurfaceAnimationManager> TakeSurfaceAnimationManager(
      NavigationID navigation_id);
  void ClearSurfaceAnimationManager(NavigationID navigation_id);

  FrameCounter* frame_counter() {
    return frame_counter_ ? &frame_counter_.value() : nullptr;
  }

 private:
  friend class FrameSinkManagerTest;
  friend class CompositorFrameSinkSupportTest;

  // Metadata for a CompositorFrameSink.
  struct FrameSinkData {
    explicit FrameSinkData(bool report_activation);

    FrameSinkData(const FrameSinkData&) = delete;
    FrameSinkData& operator=(const FrameSinkData&) = delete;

    FrameSinkData(FrameSinkData&& other);
    FrameSinkData& operator=(FrameSinkData&& other);

    ~FrameSinkData();

    // A label to identify frame sink.
    std::string debug_label;

    // Indicates whether the client wishes to receive FirstSurfaceActivation
    // notification.
    bool report_activation;
  };

  // BeginFrameSource routing information for a FrameSinkId.
  struct FrameSinkSourceMapping {
    FrameSinkSourceMapping();

    FrameSinkSourceMapping(const FrameSinkSourceMapping&) = delete;
    FrameSinkSourceMapping& operator=(const FrameSinkSourceMapping&) = delete;

    FrameSinkSourceMapping(FrameSinkSourceMapping&& other);
    FrameSinkSourceMapping& operator=(FrameSinkSourceMapping&& other);

    ~FrameSinkSourceMapping();

    // The currently assigned begin frame source for this client.
    raw_ptr<BeginFrameSource> source = nullptr;
    // This represents a dag of parent -> children mapping.
    base::flat_set<FrameSinkId> children;
  };

  void RecursivelyAttachBeginFrameSource(const FrameSinkId& frame_sink_id,
                                         BeginFrameSource* source);
  void RecursivelyDetachBeginFrameSource(const FrameSinkId& frame_sink_id,
                                         BeginFrameSource* source);

  // FrameSinkVideoCapturerManager implementation:
  CapturableFrameSink* FindCapturableFrameSink(
      const VideoCaptureTarget& target) override;
  void OnCapturerConnectionLost(FrameSinkVideoCapturerImpl* capturer) override;

  // Returns true if |child framesink| is or has |search_frame_sink_id| as a
  // child.
  bool ChildContains(const FrameSinkId& child_frame_sink_id,
                     const FrameSinkId& search_frame_sink_id) const;

  // Updates throttling recursively on a frame sink specified by its |id|
  // and all its descendants to send BeginFrames at |interval|.
  void UpdateThrottlingRecursively(const FrameSinkId& id,
                                   base::TimeDelta interval);

  // Called when throttling needs to be updated. Some examples can trigger such
  // an update include: starting of video capturing requires throttling on the
  // frame sink being captured to be stopped; a frame sink hierarchical change
  // requires throttling on affected frame sinks to be started or stopped.
  void UpdateThrottling();

  // Clears throttling operation on the frame sink with |id| and all its
  // descendants.
  void ClearThrottling(const FrameSinkId& id);

  // SharedBitmapManager for the viz display service for receiving software
  // resources in CompositorFrameSinks.
  const raw_ptr<SharedBitmapManager> shared_bitmap_manager_;

  // Provides an output surface for CreateRootCompositorFrameSink().
  const raw_ptr<OutputSurfaceProvider> output_surface_provider_;

  const raw_ptr<GmbVideoFramePoolContextProvider> gmb_context_provider_;

  SurfaceManager surface_manager_;

  // Must be created after and destroyed before |surface_manager_|.
  HitTestManager hit_test_manager_;

  // Restart id to generate unique begin frames across process restarts.  Used
  // for creating a BeginFrameSource for RootCompositorFrameSink.
  const uint32_t restart_id_;

  // Whether display scheduler should wait for all pipeline stages before draw.
  const bool run_all_compositor_stages_before_draw_;

  // Whether capture pipeline should emit log messages to webrtc log.
  const bool log_capture_pipeline_in_webrtc_;

  // This is viz-global instance of DebugRendererSettings.
  DebugRendererSettings debug_settings_;

  base::ProcessId host_process_id_;

  // Performance hint session factory of this viz instance.
  const raw_ptr<HintSessionFactory> hint_session_factory_;

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

  base::flat_map<FrameSinkBundleId, std::unique_ptr<FrameSinkBundleImpl>>
      bundle_map_;

  base::flat_set<std::unique_ptr<FrameSinkVideoCapturerImpl>,
                 base::UniquePtrComparator>
      video_capturers_;

  base::flat_map<NavigationID, std::unique_ptr<SurfaceAnimationManager>>
      navigation_to_animation_manager_;

  // The ids of the frame sinks that are currently being captured.
  // These frame sinks should not be throttled.
  base::flat_set<FrameSinkId> captured_frame_sink_ids_;

  // Ids of the frame sinks that have been requested to throttle.
  std::vector<FrameSinkId> frame_sink_ids_to_throttle_;

  // The throttling interval which defines how often BeginFrames are sent for
  // frame sinks in `frame_sink_ids_to_throttle_`, if
  // `global_throttle_interval_` is unset or if this interval is longer than
  // `global_throttle_interval_`.
  base::TimeDelta throttle_interval_ = BeginFrameArgs::DefaultInterval();

  // If present, the throttling interval which defines the upper bound of how
  // often BeginFrames are sent for all current and future frame sinks.
  absl::optional<base::TimeDelta> global_throttle_interval_ = absl::nullopt;

  base::flat_map<uint32_t, base::ScopedClosureRunner> cached_back_buffers_;

  THREAD_CHECKER(thread_checker_);

  // |video_detector_| is instantiated lazily in order to avoid overhead on
  // platforms that don't need video detection.
  std::unique_ptr<VideoDetector> video_detector_;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  mojo::Remote<mojom::FrameSinkManagerClient> client_remote_;
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
  raw_ptr<mojom::FrameSinkManagerClient> client_ = nullptr;
  mojo::Receiver<mojom::FrameSinkManager> receiver_{this};

  base::ObserverList<FrameSinkObserver>::Unchecked observer_list_;

  // Counts frames for test.
  absl::optional<FrameCounter> frame_counter_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_IMPL_H_
