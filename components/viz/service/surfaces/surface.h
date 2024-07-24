// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/surfaces/frame_index_constants.h"
#include "components/viz/service/surfaces/pending_copy_output_request.h"
#include "components/viz/service/surfaces/surface_client.h"
#include "components/viz/service/surfaces/surface_dependency_deadline.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
struct PresentationFeedback;
struct SwapTimings;
}

namespace ui {
class LatencyInfo;
}

namespace viz {

class CopyOutputRequest;
class SurfaceAllocationGroup;
class SurfaceManager;

// A Surface is a representation of a sequence of CompositorFrames with a
// common set of properties uniquely identified by a SurfaceId. In particular,
// all CompositorFrames submitted to a single Surface share properties described
// in SurfaceInfo: device scale factor and size. A Surface can hold up few
// CompositorFrames at a given time:
//
//   Uncommitted frames: It's frame that has been received, but hasn't been
//                       processed yet. There can be up to
//                       `max_uncommitted_frames_` in this state. If
//                       `max_uncommitted_frames_` is zero all frames are
//                       committed as soon as they are received.
//
//   Pending frame:      A pending CompositorFrame cannot be displayed on
//                       screen. A CompositorFrame is pending when it has been
//                       committed but has unresolved dependencies: surface Ids
//                       to which there are no active CompositorFrames. There
//                       can be only one pending frame.
//
//   Active frame:       An active frame is a candidate for display. A
//                       CompositorFrame is active if it has been explicitly
//                       marked as active after a deadline has passed or all
//                       its dependencies are active. There can be only one
//                       active frame.
//
// This pending+active frame mechanism for managing CompositorFrames from a
// client exists to enable best-effort synchronization across clients. A surface
// subtree will remain pending until all dependencies are resolved: all clients
// have submitted CompositorFrames corresponding to a new property of the
// subtree (e.g. a new size).
//
// Clients are assumed to be untrusted and so a client may not submit a
// CompositorFrame to satisfy the dependency of the parent. Thus, by default, a
// surface has an activation deadline associated with its dependencies. If the
// deadline passes, then the CompositorFrame will activate despite missing
// dependencies. The activated CompositorFrame can specify fallback behavior in
// the event of missing dependencies at display time.
//
// On WebView display compositor runs asynchronously in regards of BeginFrames
// and CompositorFrame submissions, to avoid frame drops due to racyness
// uncommitted queue mechanism is used. When clients submits frame it goes to
// the queue and when the display compositor draws frames are committed from
// the queue to the pending or active frame.

class VIZ_SERVICE_EXPORT Surface final {
 public:
  class PresentationHelper {
   public:
    PresentationHelper(base::WeakPtr<SurfaceClient> surface_client,
                       uint32_t frame_token);

    PresentationHelper(const PresentationHelper&) = delete;
    PresentationHelper& operator=(const PresentationHelper&) = delete;

    ~PresentationHelper();

    void DidPresent(base::TimeTicks draw_start_timestamp,
                    const gfx::SwapTimings& timings,
                    const gfx::PresentationFeedback& feedback);

   private:
    base::WeakPtr<SurfaceClient> surface_client_;
    const uint32_t frame_token_;
  };

  using PresentedCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback&)>;
  enum QueueFrameResult { REJECTED, ACCEPTED_ACTIVE, ACCEPTED_PENDING };

  using CommitPredicate =
      base::FunctionRef<bool(const SurfaceId&, const BeginFrameId&)>;

  // `pending_copy_surface_id`, when valid, becomes an
  // `active_referenced_surfaces_` of `this`.
  Surface(const SurfaceInfo& surface_info,
          SurfaceManager* surface_manager,
          SurfaceAllocationGroup* allocation_group,
          base::WeakPtr<SurfaceClient> surface_client,
          const SurfaceId& pending_copy_surface_id,
          size_t max_uncommitted_frames);

  Surface(const Surface&) = delete;
  Surface& operator=(const Surface&) = delete;

  ~Surface();

  void SetDependencyDeadline(
      std::unique_ptr<SurfaceDependencyDeadline> deadline);

  const SurfaceId& surface_id() const { return surface_info_.id(); }
  const SurfaceId& previous_frame_surface_id() const {
    return previous_frame_surface_id_;
  }
  const gfx::Size& size_in_pixels() const {
    return surface_info_.size_in_pixels();
  }
  float device_scale_factor() const {
    return surface_info_.device_scale_factor();
  }

  base::WeakPtr<SurfaceClient> client() { return surface_client_; }

  bool has_deadline() const { return deadline_ && deadline_->has_deadline(); }

  std::optional<base::TimeTicks> deadline_for_testing() const {
    return deadline_->deadline_for_testing();
  }

  void SetPreviousFrameSurface(Surface* surface);

  // Returns false if |frame| is invalid. |frame_rejected_callback| will be
  // called once if the frame will not be displayed.
  QueueFrameResult QueueFrame(
      CompositorFrame frame,
      uint64_t frame_index,
      base::ScopedClosureRunner frame_rejected_callback);

  // Commits frame(s) in this Surface and its dependencies. For each affected
  // surface, the predicate will be called for each uncommitted frame in each
  // surface from the oldest to the newest and will abort at first case of
  // returning false.
  void CommitFramesRecursively(const CommitPredicate& predicate);

  // Notifies the Surface that a blocking SurfaceId now has an active
  // frame.
  void NotifySurfaceIdAvailable(const SurfaceId& surface_id);

  // Called if a deadline has been hit and this surface is not yet active but
  // it's marked as respecting deadlines.
  void ActivatePendingFrameForDeadline();

  // Places the copy-of-output request on the render pass defined by
  // |PendingCopyOutputRequest::subtree_capture_id| if such a render pass
  // exists, otherwise the request will be ignored.
  void RequestCopyOfOutput(
      PendingCopyOutputRequest pending_copy_output_request);

  using CopyRequestsMap =
      std::multimap<CompositorRenderPassId, std::unique_ptr<CopyOutputRequest>>;

  // Adds each CopyOutputRequest in the current frame to copy_requests. The
  // caller takes ownership of them. |copy_requests| is keyed by RenderPass
  // ids.
  void TakeCopyOutputRequests(CopyRequestsMap* copy_requests);

  // Takes CopyOutputRequests made at the client level and adds them to this
  // Surface.
  void TakeCopyOutputRequestsFromClient();

  // Returns whether there is a CopyOutputRequest inside the active frame.
  bool HasCopyOutputRequests() const;

  // Returns the most recent frame or frame metadata that is eligible to be
  // rendered. You must check whether HasActiveFrame() returns true before
  // calling these methods.
  // Note that we prefer to call GetActiveFrameMetadata or
  // GetFrameIntervalInputs if the only thing that is required from the frame.
  const CompositorFrame& GetActiveFrame() const;
  const CompositorFrameMetadata& GetActiveFrameMetadata() const;
  const FrameIntervalInputs& GetFrameIntervalInputs() const;

  // ViewTransition needs to interpolate a new CompositorFrame from the active
  // one of this Surface. The interpolated new frame replaces the currently
  // active one via this API.
  void SetActiveFrameForViewTransition(CompositorFrame frame);

  // Returns true if the active frame has damage due to a surface animation.
  // This means that the damage should be respected even if the active frame
  // index has not changed.
  bool HasSurfaceAnimationDamage() const;

  // Returns the currently pending frame. You must check where HasPendingFrame()
  // returns true before calling this method.
  const CompositorFrame& GetPendingFrame();

  // Returns a number that increments by 1 every time a new frame is enqueued.
  uint64_t GetActiveFrameIndex() const {
    return active_frame_data_ ? active_frame_data_->frame_index
                              : kInvalidFrameIndex;
  }

  void TakeActiveLatencyInfo(std::vector<ui::LatencyInfo>* latency_info);
  void TakeActiveAndPendingLatencyInfo(
      std::vector<ui::LatencyInfo>* latency_info);
  // Callers of this function must call |DidPresent| on the returned
  // PresentationHelper, at the appropriate point in the future.
  std::unique_ptr<Surface::PresentationHelper>
  TakePresentationHelperForPresentNotification();
  void SendAckToClient();
  void MarkAsDrawn();
  void NotifyAggregatedDamage(const gfx::Rect& damage_rect,
                              base::TimeTicks expected_display_time);

  // True if video capture has been started. False if it has been stopped.
  // This information is used by direct composition overlays to decide whether
  // overlay should be used. Not all frames have copy requests after video
  // capture. We don't want to constantly switch between overlay and non-overlay
  // during video playback.
  bool IsVideoCaptureOnFromClient();
  base::flat_set<base::PlatformThreadId> GetThreadIds();

  const base::flat_set<SurfaceId>& active_referenced_surfaces() const {
    return active_referenced_surfaces_;
  }

  // Returns the set of dependencies blocking this surface's pending frame
  // that themselves have not yet activated.
  const base::flat_set<SurfaceId>& activation_dependencies() const {
    return activation_dependencies_;
  }

  bool HasActiveFrame() const { return active_frame_data_.has_value(); }
  bool HasPendingFrame() const { return pending_frame_data_.has_value(); }
  bool HasUndrawnActiveFrame() const {
    return HasActiveFrame() && !active_frame_data_->frame_drawn;
  }
  bool HasUnackedActiveFrame() const {
    return HasActiveFrame() && !active_frame_data_->frame_acked;
  }

  bool seen_first_surface_embedding() const {
    return seen_first_surface_embedding_;
  }

  SurfaceAllocationGroup* allocation_group() const { return allocation_group_; }

  // Called when this surface will be included in the next display frame.
  void OnWillBeDrawn();

  // Called when |surface_id| is activated for the first time and its part of a
  // referenced SurfaceRange.
  void OnChildActivatedForActiveFrame(const SurfaceId& surface_id);

  // Called when the embedder of this surface has been activated and therefore
  // this surface should activate too by deadline inheritance.
  void ActivatePendingFrameForInheritedDeadline();

  // Returns whether the LatencyInfo of the current pending and active frames
  // is already taken.
  bool is_latency_info_taken() { return is_latency_info_taken_; }

  // Called by a blocking SurfaceAllocationGroup when |activation_dependency|
  // is resolved. |this| will be automatically unregistered from |group|, the
  // SurfaceAllocationGroup corresponding to |activation_dependency|.
  void OnActivationDependencyResolved(const SurfaceId& activation_dependency,
                                      SurfaceAllocationGroup* group);

  // Notifies that this surface is no longer the primary surface of the
  // embedder. All future CompositorFrames will activate as soon as they arrive
  // and if a pending frame currently exists it will immediately activate as
  // well. This allows the client to not wait for acks from the fallback
  // surfaces and be able to submit to the primary surface.
  void SetIsFallbackAndMaybeActivate();

  void ActivateIfDeadlinePassed();

  std::unique_ptr<gfx::DelegatedInkMetadata> TakeDelegatedInkMetadata();

  base::WeakPtr<Surface> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

  // Always placed the given |copy_request| on the root render pass.
  void RequestCopyOfOutputOnRootRenderPass(
      std::unique_ptr<CopyOutputRequest> copy_request);

  // Places the copy-of-output request on the render pass defined by the given
  // id. Returns true if the request has been successfully queued and false
  // otherwise.
  bool RequestCopyOfOutputOnActiveFrameRenderPassId(
      std::unique_ptr<CopyOutputRequest> copy_request,
      CompositorRenderPassId render_pass_id);

  // Returns frame id of the oldest uncommitted frame if any,
  std::optional<uint64_t> GetFirstUncommitedFrameIndex();

  // Returns frame index of the oldest uncommitted frame that is newer than
  // provided `frame_index`.
  std::optional<uint64_t> GetUncommitedFrameIndexNewerThan(
      uint64_t frame_index);

  // Called when `pending_copy_surface_id_` no longer needs to be referenced
  // from `this`. `activation_dependencies_` will also recomputed.
  void ResetPendingCopySurfaceId();

  const SurfaceId& pending_copy_surface_id_for_testing() const {
    return pending_copy_surface_id_;
  }

 private:
  struct FrameData {
    FrameData(CompositorFrame&& frame, uint64_t frame_index);
    FrameData(FrameData&& other);
    ~FrameData();
    FrameData& operator=(FrameData&& other);

    // Delegated ink metadata should only be used for a single frame, so it
    // should be taken from the FrameData to use.
    std::unique_ptr<gfx::DelegatedInkMetadata> TakeDelegatedInkMetadata() {
      return std::move(frame.metadata.delegated_ink_metadata);
    }

    void SendAckIfNeeded(SurfaceClient* client);

    CompositorFrame frame;
    uint64_t frame_index;
    // Whether the frame has been displayed or not.
    bool frame_drawn = false;
    bool frame_acked = false;
    // Whether there is a pending presentation callback (via DidPresentSurface).
    // This typically happens when a frame is swapped - the Display will ask
    // for a callback that will supply presentation feedback to the client.
    bool will_be_notified_of_presentation = false;
  };

  // Updates surface references of the surface using the referenced
  // surfaces from the most recent CompositorFrame.
  // Modifies surface references stored in SurfaceManager.
  void UpdateSurfaceReferences();

  // Updates the set of allocation groups referenced by the active frame. Calls
  // RegisterEmbedder and UnregisterEmbedder on the allocation groups as
  // appropriate.
  void UpdateReferencedAllocationGroups(
      std::vector<SurfaceAllocationGroup*> new_referenced_allocation_groups);

  // Recomputes active references for this surface when it activates. This
  // method will also update the observed sinks based on the referenced ranges
  // in the submitted compositor frame.
  void RecomputeActiveReferencedSurfaces();

  void ActivatePendingFrame();

  // Called when all of the surface's dependencies have been resolved.
  void ActivateFrame(FrameData frame_data);

  // Called when display compositor is ready for this frame to be processed and
  // it can become pending or active.
  QueueFrameResult CommitFrame(FrameData frame);

  // Resolve the activation deadline specified by |current_frame| into a wall
  // time to be used by SurfaceDependencyDeadline.
  FrameDeadline ResolveFrameDeadline(const CompositorFrame& current_frame);

  // Updates the set of unresolved activation dependenices of the
  // |current_frame|. If the deadline requested by the frame is 0 then no
  // dependencies will be added even if they're not yet available.
  void UpdateActivationDependencies(const CompositorFrame& current_frame);

  void UnrefFrameResourcesAndRunCallbacks(std::optional<FrameData> frame_data);
  void ClearCopyRequests();

  void TakePendingLatencyInfo(std::vector<ui::LatencyInfo>* latency_info);
  static void TakeLatencyInfoFromFrame(
      CompositorFrame* frame,
      std::vector<ui::LatencyInfo>* latency_info);

  const SurfaceInfo surface_info_;
  SurfaceId previous_frame_surface_id_;
  const raw_ptr<SurfaceManager> surface_manager_;
  base::WeakPtr<SurfaceClient> surface_client_;
  std::unique_ptr<SurfaceDependencyDeadline> deadline_;

  std::optional<FrameData> pending_frame_data_;
  std::optional<FrameData> active_frame_data_;

  // Queue of uncommitted frames, oldest first.
  base::circular_deque<FrameData> uncommitted_frames_;

  bool seen_first_frame_activation_ = false;
  bool seen_first_surface_embedding_ = false;

  // A set of all valid SurfaceIds contained |last_surface_id_for_range_| to
  // avoid recompution.
  base::flat_set<SurfaceId> active_referenced_surfaces_;

  // Allocation groups that this surface references by its active frame.
  base::flat_set<raw_ptr<SurfaceAllocationGroup, CtnExperimental>>
      referenced_allocation_groups_;

  // The set of the SurfaceIds that are blocking the pending frame from being
  // activated.
  base::flat_set<SurfaceId> activation_dependencies_;

  // The SurfaceAllocationGroups corresponding to the surfaces in
  // |activation_dependencies_|. When an activation dependency is
  // resolved, the corresponding SurfaceAllocationGroup will call back into this
  // surface to let us know.
  base::flat_set<raw_ptr<SurfaceAllocationGroup, CtnExperimental>>
      blocking_allocation_groups_;

  bool is_fallback_ = false;

  bool is_latency_info_taken_ = false;

  // Indicates there is a pending `CopyOutputRequest` against
  // `pending_copy_surface_id_`. When valid, it keeps `pending_copy_surface_id_`
  // reachable from `this`, and keeps `pending_copy_surface_id_` alive during
  // the aggregation.
  SurfaceId pending_copy_surface_id_;

  const raw_ptr<SurfaceAllocationGroup> allocation_group_;

  const size_t max_uncommitted_frames_;

  base::WeakPtrFactory<Surface> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_H_
