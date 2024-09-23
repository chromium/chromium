// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_HOST_FRAME_SINK_MANAGER_H_
#define COMPONENTS_VIZ_HOST_HOST_FRAME_SINK_MANAGER_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/input/render_input_router.mojom.h"
#include "components/viz/common/hit_test/hit_test_data_provider.h"
#include "components/viz/common/hit_test/hit_test_query.h"
#include "components/viz/common/hit_test/hit_test_region_observer.h"
#include "components/viz/common/surfaces/frame_sink_bundle_id.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "components/viz/host/viz_host_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager_test_api.mojom.h"
#include "services/viz/privileged/mojom/compositing/frame_sinks_metrics_recorder.mojom.h"
#include "services/viz/public/mojom/compositing/frame_sink_bundle.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace viz {

class SurfaceInfo;

enum class ReportFirstSurfaceActivation { kYes, kNo };

// Browser side wrapper of mojom::FrameSinkManager, to be used from the
// UI thread. Manages frame sinks and is intended to replace all usage of
// FrameSinkManagerImpl.
class VIZ_HOST_EXPORT HostFrameSinkManager
    : public mojom::FrameSinkManagerClient,
      public HitTestDataProvider {
 public:
  HostFrameSinkManager();

  HostFrameSinkManager(const HostFrameSinkManager&) = delete;
  HostFrameSinkManager& operator=(const HostFrameSinkManager&) = delete;

  ~HostFrameSinkManager() override;

  // Sets a local FrameSinkManagerImpl instance and connects directly to it.
  void SetLocalManager(mojom::FrameSinkManager* frame_sink_manager);

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

  // Registers `frame_sink_id` so that a client can submit CompositorFrames
  // using it. This must be called before creating a CompositorFrameSink or
  // registering FrameSinkId hierarchy.
  //
  // A subsequent call with a new `client` for the same `frame_sink_id` will
  // bind the id to the new `client` immediately. InvalidateFrameSinkId must
  // only be called for the new bound `client`. All callbacks, including
  // existing pending ones, are dispatched to the new `client`.
  //
  // When the `client` is done submitting CompositorFrames to `frame_sink_id`
  // InvalidateFrameSink() should be called. It is invalid to register this
  // `frame_sink_id` with another client after invalidation.
  void RegisterFrameSinkId(const FrameSinkId& frame_sink_id,
                           HostFrameSinkClient* client,
                           ReportFirstSurfaceActivation report_activation);

  // Returns true if RegisterFrameSinkId() was called with |frame_sink_id| and
  // InvalidateFrameSinkId() has not been called.
  bool IsFrameSinkIdRegistered(const FrameSinkId& frame_sink_id) const;

  // Invalidates `frame_sink_id` when the client is done submitting
  // CompositorFrames. If there is a CompositorFrameSink for `frame_sink_id`
  // then it will be destroyed and the message pipe to the client will be
  // closed.
  void InvalidateFrameSinkId(const FrameSinkId& frame_sink_id,
                             HostFrameSinkClient* client);

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
  //
  // |maybe_wait_on_destruction| is for the caller to request that the browser
  // should wait on frame sync destruction before destroying the platform
  // window.
  void CreateRootCompositorFrameSink(
      mojom::RootCompositorFrameSinkParamsPtr params,
      bool maybe_wait_on_destruction = true);

  // Creates a connection from a client to viz, using |request| and |client|,
  // that allows the client to submit CompositorFrames. When no longer needed,
  // call InvalidateFrameSinkId().
  //
  // If there is already a CompositorFrameSink for |frame_sink_id| then calling
  // this will destroy the existing CompositorFrameSink and create a new one.
  void CreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      mojo::PendingReceiver<mojom::CompositorFrameSink> receiver,
      mojo::PendingRemote<mojom::CompositorFrameSinkClient> client,
      input::mojom::RenderInputRouterConfigPtr render_input_router_config =
          nullptr);

  // Creates a connection to control a set of related frame sinks through
  // batched IPCs on the FrameSinkBundle and FrameSinkBundleClient interfaces.
  // Frame sinks are added to the bundle at frame sink creation time by using
  // CreateBundledCompositorFrameSink with the same `bundle_id` value, rather
  // than using CreateCompositorFrameSink.
  void CreateFrameSinkBundle(
      const FrameSinkBundleId& bundle_id,
      mojo::PendingReceiver<mojom::FrameSinkBundle> receiver,
      mojo::PendingRemote<mojom::FrameSinkBundleClient> client);

  // Similar to CreateCompositorFrameSink, but the new viz-side
  // CompositorFrameSink object will be associated with the identified
  // FrameSinkBundle. This means that it will receive OnBeginFrames() and a few
  // other client notifications in batch with other frame sinks in the bundle
  // via the corresponding FrameSinkBundleClient, rather than through `client`
  // (though `client` is still used to send some notifications), and that its
  // CompositorFrames (or DidNotSubmitFrame calls) MAY be submitted in batch
  // through the corresponding FrameSinkBundle, rather than being sent directly
  // to `receiver`.
  void CreateBundledCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      const FrameSinkBundleId& bundle_id,
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
  //
  // When `capture_exact_surface_id` is true, the snapshot will only be
  // taken for the surface whose ID is an exact match to `surface_id`. This is
  // useful to take a snapshot of an old `Surface` post-navigation.
  void RequestCopyOfOutput(const SurfaceId& surface_id,
                           std::unique_ptr<CopyOutputRequest> request,
                           bool capture_exact_surface_id = false);

  using ScreenshotDestinationReadyCallback =
      base::OnceCallback<void(const SkBitmap& copy_output)>;
  // Sets the callback which is invoked when a `CopyOutputResult` associated
  // with `destination_token` is received by the host/browser process from the
  // Viz process. Must be called once per `destination_token`.
  // This is used to save screenshots for same-document navigations committed in
  // the renderer process.
  void SetOnCopyOutputReadyCallback(
      const blink::SameDocNavigationScreenshotDestinationToken&
          destination_token,
      ScreenshotDestinationReadyCallback callback);

  // Invalidates the `ScreenshotDestinationReadyCallback` for
  // `destination_token`. Used when the destination is no longer eligible for
  // storing the screenshot (e.g., a later-arrival screenshot after the
  // destination is destroyed).
  void InvalidateCopyOutputReadyCallback(
      const blink::SameDocNavigationScreenshotDestinationToken&
          destination_token);

  void Throttle(const std::vector<FrameSinkId>& ids, base::TimeDelta interval);
  void StartThrottlingAllFrameSinks(base::TimeDelta interval);
  void StopThrottlingAllFrameSinks();

  // HitTestDataProvider implementation.
  void AddHitTestRegionObserver(HitTestRegionObserver* observer) override;
  void RemoveHitTestRegionObserver(HitTestRegionObserver* observer) override;
  const DisplayHitTestQueryMap& GetDisplayHitTestQuery() const override;

  void SetHitTestAsyncQueriedDebugRegions(
      const FrameSinkId& root_frame_sink_id,
      const std::vector<FrameSinkId>& hit_test_async_queried_debug_queue);

#if BUILDFLAG(IS_ANDROID)
  // Preserves the back buffer associated with the |root_sink_id|, even after
  // the associated Display has been torn down, and returns an id for this cache
  // entry.
  uint32_t CacheBackBufferForRootSink(const FrameSinkId& root_sink_id);
  void EvictCachedBackBuffer(uint32_t cache_id);
#endif

  void CreateHitTestQueryForSynchronousCompositor(
      const FrameSinkId& frame_sink_id);
  void EraseHitTestQueryForSynchronousCompositor(
      const FrameSinkId& frame_sink_id);

  void UpdateDebugRendererSettings(const DebugRendererSettings& debug_settings);

  mojom::FrameSinksMetricsRecorder& GetFrameSinksMetricsRecorderForTest();
  mojom::FrameSinkManagerTestApi& GetFrameSinkManagerTestApi();

  void ClearUnclaimedViewTransitionResources(
      const blink::ViewTransitionToken& transition_token);

  const DebugRendererSettings& debug_renderer_settings() const {
    return debug_renderer_settings_;
  }

 private:
  friend class HostFrameSinkManagerTest;
  friend class HostFrameSinkManagerTestApi;

  struct FrameSinkData {
    FrameSinkData();

    FrameSinkData(const FrameSinkData&) = delete;
    FrameSinkData& operator=(const FrameSinkData&) = delete;

    FrameSinkData(FrameSinkData&& other);
    FrameSinkData& operator=(FrameSinkData&& other);

    ~FrameSinkData();

    bool IsFrameSinkRegistered() const { return client != nullptr; }

    // Returns true if there is nothing in FrameSinkData and it can be deleted.
    bool IsEmpty() const {
      return !IsFrameSinkRegistered() && !has_created_compositor_frame_sink &&
             children.empty();
    }

    // The client to be notified of changes to this FrameSink.
    raw_ptr<HostFrameSinkClient> client = nullptr;

    // Indicates whether or not this client cares to receive
    // FirstSurfaceActivation notifications.
    ReportFirstSurfaceActivation report_activation =
        ReportFirstSurfaceActivation::kYes;

    // The name of the HostFrameSinkClient used for debug purposes.
    std::string debug_label;

    // If the frame sink is a root that corresponds to a Display.
    bool is_root = false;

    // If we should wait on synchronous destruction.
    bool wait_on_destruction = false;

    // If a mojom::CompositorFrameSink was created for this FrameSinkId. This
    // will always be false if not using Mojo.
    bool has_created_compositor_frame_sink = false;

    // Track frame sink hierarchy.
    std::vector<FrameSinkId> children;
  };

  void CreateFrameSink(
      const FrameSinkId& frame_sink_id,
      std::optional<FrameSinkBundleId> bundle_id,
      mojo::PendingReceiver<mojom::CompositorFrameSink> receiver,
      mojo::PendingRemote<mojom::CompositorFrameSinkClient> client,
      input::mojom::RenderInputRouterConfigPtr render_input_router_config);

  // Handles connection loss to |frame_sink_manager_remote_|. This should only
  // happen when the GPU process crashes.
  void OnConnectionLost();

  // Registers FrameSinkId and FrameSink hierarchy again after connection loss.
  void RegisterAfterConnectionLoss();

  // mojom::FrameSinkManagerClient:
  void OnFrameTokenChanged(const FrameSinkId& frame_sink_id,
                           uint32_t frame_token,
                           base::TimeTicks activation_time) override;
  void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) override;
  void OnAggregatedHitTestRegionListUpdated(
      const FrameSinkId& frame_sink_id,
      const std::vector<AggregatedHitTestRegion>& hit_test_data) override;
#if BUILDFLAG(IS_ANDROID)
  void VerifyThreadIdsDoNotBelongToHost(
      const std::vector<int32_t>& thread_ids,
      VerifyThreadIdsDoNotBelongToHostCallback callback) override;
#endif
  void OnScreenshotCaptured(
      const blink::SameDocNavigationScreenshotDestinationToken&
          destination_token,
      std::unique_ptr<CopyOutputResult> copy_output_result) override;

  // Connections to/from FrameSinkManagerImpl.
  mojo::Remote<mojom::FrameSinkManager> frame_sink_manager_remote_;
  // This will point to |frame_sink_manager_remote_| if using mojo or it may
  // point directly at FrameSinkManagerImpl in tests. Use this to make function
  // calls.
  raw_ptr<mojom::FrameSinkManager> frame_sink_manager_ = nullptr;
  mojo::Receiver<mojom::FrameSinkManagerClient>
      frame_sink_manager_client_receiver_{this};
  mojo::Remote<mojom::FrameSinksMetricsRecorder> metrics_recorder_remote_;
  mojo::Remote<mojom::FrameSinkManagerTestApi> test_api_remote_;

  // Per CompositorFrameSink data.
  std::unordered_map<FrameSinkId, FrameSinkData, FrameSinkIdHash>
      frame_sink_data_map_;

  // If |frame_sink_manager_remote_| connection was lost.
  bool connection_was_lost_ = false;

  base::RepeatingClosure connection_lost_callback_;

  DisplayHitTestQueryMap display_hit_test_query_;

  // TODO(jonross): Separate out all hit testing work into its own separate
  // class.
  base::ObserverList<HitTestRegionObserver> observers_;

#if BUILDFLAG(IS_ANDROID)
  uint32_t next_cache_back_buffer_id_ = 1;
  uint32_t min_valid_cache_back_buffer_id_ = 1;
#endif

  // This is kept in sync with implementation.
  DebugRendererSettings debug_renderer_settings_;

  // When Viz sends the screenshot back to the host process,
  // `ScreenshotDestinationReadyCallback` is invoked to stash the screenshot to
  // the correct destination.
  base::flat_map<blink::SameDocNavigationScreenshotDestinationToken,
                 ScreenshotDestinationReadyCallback>
      screenshot_destinations_;

  base::WeakPtrFactory<HostFrameSinkManager> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_HOST_FRAME_SINK_MANAGER_H_
