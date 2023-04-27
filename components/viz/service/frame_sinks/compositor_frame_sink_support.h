// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_COMPOSITOR_FRAME_SINK_SUPPORT_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_COMPOSITOR_FRAME_SINK_SUPPORT_H_

#include <memory>
#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/power_scheduler/power_mode_voter.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_bundle_id.h"
#include "components/viz/common/surfaces/region_capture_bounds.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/common/surfaces/video_capture_target.h"
#include "components/viz/service/frame_sinks/begin_frame_tracker.h"
#include "components/viz/service/frame_sinks/surface_resource_holder.h"
#include "components/viz/service/frame_sinks/surface_resource_holder_client.h"
#include "components/viz/service/frame_sinks/video_capture/capturable_frame_sink.h"
#include "components/viz/service/hit_test/hit_test_aggregator.h"
#include "components/viz/service/layers/layer_context_impl.h"
#include "components/viz/service/surfaces/frame_index_constants.h"
#include "components/viz/service/surfaces/surface_client.h"
#include "components/viz/service/transitions/surface_animation_manager.h"
#include "components/viz/service/viz_service_export.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "services/viz/public/mojom/hit_test/hit_test_region_list.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace viz {

class FrameSinkManagerImpl;
class LatestLocalSurfaceIdLookupDelegate;
class LayerContextImpl;
class Surface;
class SurfaceManager;

// Possible outcomes of MaybeSubmitCompositorFrame().
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SubmitResult {
  ACCEPTED = 0,
  COPY_OUTPUT_REQUESTS_NOT_ALLOWED = 1,
  // SURFACE_INVARIANTS_VIOLATION = 2 is deprecated.
  SIZE_MISMATCH = 3,
  SURFACE_ID_DECREASED = 4,
  SURFACE_OWNED_BY_ANOTHER_CLIENT = 5,
  // Magic constant used by the histogram macros.
  kMaxValue = SURFACE_OWNED_BY_ANOTHER_CLIENT,
};

class VIZ_SERVICE_EXPORT CompositorFrameSinkSupport
    : public BeginFrameObserver,
      public SurfaceResourceHolderClient,
      public SurfaceClient,
      public CapturableFrameSink {
 public:
  using AggregatedDamageCallback =
      base::RepeatingCallback<void(const LocalSurfaceId& local_surface_id,
                                   const gfx::Size& frame_size_in_pixels,
                                   const gfx::Rect& damage_rect,
                                   base::TimeTicks expected_display_time)>;

  // Determines maximum number of allowed undrawn frames. Once this limit is
  // exceeded, we throttle sBeginFrames to 1 per second. Limit must be at least
  // 1, as the relative ordering of renderer / browser frame submissions allows
  // us to have one outstanding undrawn frame under normal operation.
  static constexpr uint32_t kUndrawnFrameLimit = 3;

  CompositorFrameSinkSupport(mojom::CompositorFrameSinkClient* client,
                             FrameSinkManagerImpl* frame_sink_manager,
                             const FrameSinkId& frame_sink_id,
                             bool is_root);

  CompositorFrameSinkSupport(const CompositorFrameSinkSupport&) = delete;
  CompositorFrameSinkSupport& operator=(const CompositorFrameSinkSupport&) =
      delete;

  ~CompositorFrameSinkSupport() override;

  const FrameSinkId& frame_sink_id() const { return frame_sink_id_; }

  const SurfaceId& last_activated_surface_id() const {
    return last_activated_surface_id_;
  }

  const LocalSurfaceId& last_activated_local_surface_id() const {
    return last_activated_surface_id_.local_surface_id();
  }

  bool is_root() const { return is_root_; }

  FrameSinkManagerImpl* frame_sink_manager() { return frame_sink_manager_; }
  BeginFrameSource* begin_frame_source() { return begin_frame_source_; }

  const FrameTimingDetailsMap& timing_details() {
    return frame_timing_details_;
  }

  bool needs_begin_frame() const { return needs_begin_frame_; }

  [[nodiscard]] FrameTimingDetailsMap TakeFrameTimingDetailsMap();

  // Viz hit-test setup is only called when |is_root_| is true (except on
  // android webview).
  void SetUpHitTest(
      LatestLocalSurfaceIdLookupDelegate* local_surface_id_lookup_delegate);

  // The provided callback will be run every time a surface owned by this object
  // or one of its descendents is determined to be damaged at aggregation time.
  void SetAggregatedDamageCallbackForTesting(AggregatedDamageCallback callback);

  // This allows the FrameSinkManagerImpl to pass a BeginFrameSource to use.
  void SetBeginFrameSource(BeginFrameSource* begin_frame_source);

  // Sets the ID of the FrameSinkBundle to which this sink should belong. If the
  // sink is incompatible with the bundle (i.e. uses a different
  // BeginFrameSource than this CompositorFrameSinkSupport) then the sink will
  // be removed from the bundle and destroyed asynchronously, disconnecting its
  // client.
  void SetBundle(const FrameSinkBundleId& bundle_id);

  base::TimeDelta GetPreferredFrameInterval(
      mojom::CompositorFrameSinkType* type) const;
  void InitializeCompositorFrameSinkType(mojom::CompositorFrameSinkType type);
  void BindLayerContext(mojom::PendingLayerContext& context);
  void SetThreadIds(
      bool from_untrusted_client,
      base::flat_set<base::PlatformThreadId> unverified_thread_ids);
  // Throttles the BeginFrames to send at |interval| if |interval| is greater
  // than zero, or clears previously set throttle if zero.
  void ThrottleBeginFrame(base::TimeDelta interval);

  // SurfaceClient implementation.
  void OnSurfaceCommitted(Surface* surface) override;
  void OnSurfaceActivated(Surface* surface) override;
  void OnSurfaceDestroyed(Surface* surface) override;
  void OnSurfaceWillDraw(Surface* surface) override;
  void RefResources(
      const std::vector<TransferableResource>& resources) override;
  void UnrefResources(std::vector<ReturnedResource> resources) override;
  void ReturnResources(std::vector<ReturnedResource> resources) override;
  void ReceiveFromChild(
      const std::vector<TransferableResource>& resources) override;
  // Takes the CopyOutputRequests that were requested for a surface with at
  // most |local_surface_id|.
  std::vector<PendingCopyOutputRequest> TakeCopyOutputRequests(
      const LocalSurfaceId& local_surface_id) override;
  void OnFrameTokenChanged(uint32_t frame_token) override;
  void SendCompositorFrameAck() override;
  void OnSurfaceAggregatedDamage(
      Surface* surface,
      const LocalSurfaceId& local_surface_id,
      const CompositorFrame& frame,
      const gfx::Rect& damage_rect,
      base::TimeTicks expected_display_time) override;
  void OnSurfacePresented(uint32_t frame_token,
                          base::TimeTicks draw_start_timestamp,
                          const gfx::SwapTimings& swap_timings,
                          const gfx::PresentationFeedback& feedback) override;
  bool IsVideoCaptureStarted() override;
  base::flat_set<base::PlatformThreadId> GetThreadIds() override;

  // mojom::CompositorFrameSink helpers.
  void SetNeedsBeginFrame(bool needs_begin_frame);
  void SetWantsAnimateOnlyBeginFrames();
  void SetWantsBeginFrameAcks();
  void DidNotProduceFrame(const BeginFrameAck& ack);
  void SubmitCompositorFrame(
      const LocalSurfaceId& local_surface_id,
      CompositorFrame frame,
      absl::optional<HitTestRegionList> hit_test_region_list = absl::nullopt,
      uint64_t submit_time = 0);
  // Returns false if the notification was not valid (a duplicate).
  bool DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                               const SharedBitmapId& id);
  void DidDeleteSharedBitmap(const SharedBitmapId& id);

  // Mark |id| and all surfaces with smaller ids for destruction. Note that |id|
  // doesn't have to exist at the time of calling.
  void EvictSurface(const LocalSurfaceId& id);

  // Attempts to submit a new CompositorFrame to |local_surface_id| and returns
  // whether the frame was accepted or the reason why it was rejected. If
  // |local_surface_id| hasn't been submitted before then a new Surface will be
  // created for it.
  //
  // This is called by SubmitCompositorFrame(), which DCHECK-fails on a
  // non-accepted result. Prefer calling SubmitCompositorFrame() instead of this
  // method unless the result value affects what the caller will do next.
  SubmitResult MaybeSubmitCompositorFrame(
      const LocalSurfaceId& local_surface_id,
      CompositorFrame frame,
      absl::optional<HitTestRegionList> hit_test_region_list,
      uint64_t submit_time,
      mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback callback);

  // CapturableFrameSink implementation.
  const FrameSinkId& GetFrameSinkId() const override;
  void AttachCaptureClient(CapturableFrameSink::Client* client) override;
  void DetachCaptureClient(CapturableFrameSink::Client* client) override;
  gfx::Rect GetCopyOutputRequestRegion(
      const VideoCaptureSubTarget& sub_target) const override;
  void OnClientCaptureStarted() override;
  void OnClientCaptureStopped() override;
  void RequestCopyOfOutput(
      PendingCopyOutputRequest pending_copy_output_request) override;
  const CompositorFrameMetadata* GetLastActivatedFrameMetadata() override;

  HitTestAggregator* GetHitTestAggregator();

  // Permits submitted CompositorFrames to contain CopyOutputRequests, for
  // special-case testing purposes only.
  void set_allow_copy_output_requests_for_testing() {
    allow_copy_output_requests_ = true;
  }

  Surface* GetLastCreatedSurfaceForTesting();

  // Maps the |result| from MaybeSubmitCompositorFrame() to a human-readable
  // string.
  static const char* GetSubmitResultAsString(SubmitResult result);

  const std::vector<PendingCopyOutputRequest>&
  copy_output_requests_for_testing() const {
    return copy_output_requests_;
  }

  bool IsEvicted(const LocalSurfaceId& local_surface_id) const;

  SurfaceAnimationManager* GetSurfaceAnimationManagerForTesting();

  const RegionCaptureBounds& current_capture_bounds() const {
    return current_capture_bounds_;
  }

  mojom::CompositorFrameSinkType frame_sink_type() const {
    return frame_sink_type_;
  }

 private:
  friend class CompositorFrameSinkSupportTest;
  friend class DisplayTest;
  friend class FrameSinkManagerTest;
  friend class OnBeginFrameAcksCompositorFrameSinkSupportTest;
  friend class SurfaceAggregatorWithResourcesTest;

  // Creates a surface reference from the top-level root to |surface_id|.
  SurfaceReference MakeTopLevelRootReference(const SurfaceId& surface_id);

  void ProcessCompositorFrameTransitionDirective(
      const CompositorFrameTransitionDirective& directive,
      Surface* surface);
  void OnCompositorFrameTransitionDirectiveProcessed(
      const CompositorFrameTransitionDirective& directive);

  void DidReceiveCompositorFrameAck();
  void DidPresentCompositorFrame(uint32_t frame_token,
                                 base::TimeTicks draw_start_timestamp,
                                 const gfx::SwapTimings& swap_timings,
                                 const gfx::PresentationFeedback& feedback);
  void DidRejectCompositorFrame(
      uint32_t frame_token,
      std::vector<TransferableResource> frame_resource_list,
      std::vector<ui::LatencyInfo> latency_info);

  // Update the display root reference with |surface|.
  void UpdateDisplayRootReference(const Surface* surface);

  // BeginFrameObserver implementation.
  void OnBeginFrame(const BeginFrameArgs& args) override;
  const BeginFrameArgs& LastUsedBeginFrameArgs() const override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;
  bool WantsAnimateOnlyBeginFrames() const override;
  bool IsRoot() const override;

  void UpdateNeedsBeginFramesInternal();
  void StartObservingBeginFrameSource();
  void StopObservingBeginFrameSource();

  // For the sync API calls, if we are blocking a client callback, runs it once
  // BeginFrame and FrameAck are done.
  void HandleCallback();

  int64_t ComputeTraceId();

  void MaybeEvictSurfaces();
  void EvictLastActiveSurface();
  bool ShouldSendBeginFrame(base::TimeTicks timestamp);

  // Checks if any of the pending surfaces should activate now because their
  // deadline has passed. This is called every BeginFrame.
  void CheckPendingSurfaces();

  // When features::kOnBeginFrameAcks is enabled and `wants_begin_frame_acks_`
  // was requested by a client, we want to throttle sending
  // DidReceiveCompositorFrameAck and ReclaimResources. Instead merging them
  // into OnBeginFrame.
  bool ShouldMergeBeginFrameWithAcks() const;

  // When throttling is requested by a client, a BeginFrame will not be sent
  // until the time elapsed has passed the requested throttle interval since the
  // last sent BeginFrame. This function returns true if such interval has
  // passed and a BeginFrame should be sent.
  bool ShouldThrottleBeginFrameAsRequested(base::TimeTicks frame_time);

  // Instructs the FrameSinkManager to destroy our CompositorFrameSinkImpl.
  // To avoid reentrancy issues, this should be called from its own task.
  void DestroySelf();

  // Posts a task to invoke DestroySelf() ASAP.
  void ScheduleSelfDestruction();

  const raw_ptr<mojom::CompositorFrameSinkClient> client_;

  const raw_ptr<FrameSinkManagerImpl> frame_sink_manager_;
  const raw_ptr<SurfaceManager> surface_manager_;

  const FrameSinkId frame_sink_id_;
  SurfaceId last_activated_surface_id_;
  SurfaceId last_created_surface_id_;

  // If this contains a value then a surface reference from the top-level root
  // to SurfaceId(frame_sink_id_, referenced_local_surface_id_.value()) was
  // added. This will not contain a value if |is_root_| is false.
  absl::optional<LocalSurfaceId> referenced_local_surface_id_;

  SurfaceResourceHolder surface_resource_holder_;

  // This has a HitTestAggregator if and only if |is_root_| is true.
  std::unique_ptr<HitTestAggregator> hit_test_aggregator_;

  // Counts the number of CompositorFrames that have been submitted and have not
  // yet received an ACK from their Surface.
  int ack_pending_from_surface_count_ = 0;
  // Counts the number of ACKs that have been received from a Surface and have
  // not yet been sent to the CompositorFrameSinkClient.
  int ack_queued_for_client_count_ = 0;
  bool ack_pending_during_on_begin_frame_ = false;

  // When `true` we have received frames from a client using its own
  // BeginFrameSource. While dealing with frames from multiple sources we cannot
  // rely on `ack_pending_from_surface_count_` to throttle frame production.
  //
  // TODO(crbug.com/1396081): Track acks, presentation feedback, and resources
  // being returned, on a per BeginFrameSource basis. For
  // BeginFrameArgs::kManualSourceId the feedback and resources should not be
  // tied to the current `begin_frame_source_`;
  bool pending_manual_begin_frame_source_ = false;
  std::vector<ReturnedResource> surface_returned_resources_;

  // The begin frame source being observered. Null if none.
  raw_ptr<BeginFrameSource> begin_frame_source_ = nullptr;

  // The last begin frame args generated by the begin frame source.
  BeginFrameArgs last_begin_frame_args_;

  // Whether a request for begin frames has been issued.
  bool client_needs_begin_frame_ = false;

  // Whether the sink currently needs begin frames for any reason.
  bool needs_begin_frame_ = false;

  // Whether or not a frame observer has been added.
  bool added_frame_observer_ = false;

  bool wants_animate_only_begin_frames_ = false;
  bool wants_begin_frame_acks_ = false;

  // Indicates the FrameSinkBundle to which this sink belongs, if any.
  absl::optional<FrameSinkBundleId> bundle_id_;

  const bool is_root_;

  // By default, this is equivalent to |is_root_|, but may be overridden for
  // testing. Generally, for non-roots, there must not be any CopyOutputRequests
  // contained within submitted CompositorFrames. Otherwise, unprivileged
  // clients would be able to capture content for which they are not authorized.
  bool allow_copy_output_requests_;

  // Used for tests only.
  AggregatedDamageCallback aggregated_damage_callback_;

  uint64_t last_frame_index_ = kFrameIndexStart - 1;

  // The video capture clients hooking into this instance to observe frame
  // begins and damage, and then make CopyOutputRequests on the appropriate
  // frames.
  std::vector<CapturableFrameSink::Client*> capture_clients_;

  // The set of SharedBitmapIds that have been reported as allocated to this
  // interface. On closing this interface, the display compositor should drop
  // ownership of the bitmaps with these ids to avoid leaking them.
  std::set<SharedBitmapId> owned_bitmaps_;

  // These are the CopyOutputRequests made on the frame sink (as opposed to
  // being included as a part of a CompositorFrame). They stay here until a
  // Surface with a LocalSurfaceId which is at least the stored LocalSurfaceId
  // takes them. For example, for a stored PendingCopyOutputRequest, a surface
  // with LocalSurfaceId >= PendingCopyOutputRequest::local_surface_id will take
  // it, but a surface with LocalSurfaceId <
  // PendingCopyOutputRequest::local_surface_id will not. Note that if the
  // PendingCopyOutputRequest::local_surface_id is default initialized, then the
  // next surface will take it regardless of its LocalSurfaceId.
  std::vector<PendingCopyOutputRequest> copy_output_requests_;

  mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback
      compositor_frame_callback_;
  bool callback_received_begin_frame_ = true;
  bool callback_received_receive_ack_ = true;
  uint32_t trace_sequence_ = 0;

  BeginFrameTracker begin_frame_tracker_;

  // Maps |frame_token| to the timestamp when that frame was received. This
  // timestamp is combined with the information received in OnSurfacePresented()
  // and stored in |frame_timing_details_|.
  base::flat_map<uint32_t, base::TimeTicks> pending_received_frame_times_;
  FrameTimingDetailsMap frame_timing_details_;
  LocalSurfaceId last_evicted_local_surface_id_;

  base::TimeTicks last_frame_time_;

  // Initialize |last_drawn_frame_index_| as though the frame before the first
  // has been drawn.
  static_assert(kFrameIndexStart > 1,
                "|last_drawn_frame_index| relies on kFrameIndexStart > 1");
  uint64_t last_drawn_frame_index_ = kFrameIndexStart - 1;

  // This value represents throttling on sending a BeginFrame. If non-zero, it
  // represents the duration of time in between sending two consecutive frames.
  // If zero, no throttling would be applied.
  base::TimeDelta begin_frame_interval_;

  // The set of surfaces owned by this frame sink that have pending frame.
  base::flat_set<Surface*> pending_surfaces_;

  base::TimeDelta preferred_frame_interval_ = BeginFrameArgs::MinInterval();
  mojom::CompositorFrameSinkType frame_sink_type_ =
      mojom::CompositorFrameSinkType::kUnspecified;

  // This is responsible for transitioning between two CompositorFrames. The
  // frames may be produced by Surfaces managed by distinct
  // CompositorFrameSinks.
  std::unique_ptr<SurfaceAnimationManager> surface_animation_manager_;
  // The sequence ID for the save directive pending copy.
  uint32_t in_flight_save_sequence_id_ = 0;

  std::unique_ptr<power_scheduler::PowerModeVoter> power_mode_voter_;

  base::flat_set<base::PlatformThreadId> thread_ids_;

  // Number of frames skipped during throttling since last BeginFrame sent.
  uint64_t frames_throttled_since_last_ = 0;

  // Number of clients that have started video capturing.
  uint32_t number_clients_capturing_ = 0;

  // Region capture bounds associated with the last surface that was aggregated.
  RegionCaptureBounds current_capture_bounds_;

  // In LayerContext mode only, this is the Viz LayerContext implementation
  // which owns the backend layer tree for this frame sink.
  std::unique_ptr<LayerContextImpl> layer_context_impl_;

  base::WeakPtrFactory<CompositorFrameSinkSupport> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_COMPOSITOR_FRAME_SINK_SUPPORT_H_
