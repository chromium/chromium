// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <unordered_set>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/surfaces/surface_dependency_deadline.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class CopyOutputRequest;
}

namespace gfx {
struct PresentationFeedback;
}

namespace ui {
class LatencyInfo;
}

namespace viz {

class SurfaceClient;
class SurfaceManager;

// A Surface is a representation of a sequence of CompositorFrames with a
// common set of properties uniquely identified by a SurfaceId. In particular,
// all CompositorFrames submitted to a single Surface share properties described
// in SurfaceInfo: device scale factor and size. A Surface can hold up to two
// CompositorFrames at a given time:
//
//   Active frame:  An active frame is a candidate for display. A
//                  CompositorFrame is active if it has been explicitly marked
//                  as active after a deadline has passed or all its
//                  dependencies are active.
//
//   Pending frame: A pending CompositorFrame cannot be displayed on screen. A
//                  CompositorFrame is pending if it has unresolved
//                  dependencies: surface Ids to which there are no active
//                  CompositorFrames.
//
// This two stage mechanism for managing CompositorFrames from a client exists
// to enable best-effort synchronization across clients. A surface subtree will
// remain pending until all dependencies are resolved: all clients have
// submitted CompositorFrames corresponding to a new property of the subtree
// (e.g. a new size).
//
// Clients are assumed to be untrusted and so a client may not submit a
// CompositorFrame to satisfy the dependency of the parent. Thus, by default, a
// surface has an activation deadline associated with its dependencies. If the
// deadline passes, then the CompositorFrame will activate despite missing
// dependencies. The activated CompositorFrame can specify fallback behavior in
// the event of missing dependencies at display time.
class VIZ_SERVICE_EXPORT Surface final : public SurfaceDeadlineClient {
 public:
  using PresentedCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback&)>;

  Surface(const SurfaceInfo& surface_info,
          SurfaceManager* surface_manager,
          base::WeakPtr<SurfaceClient> surface_client,
          bool needs_sync_tokens,
          bool block_activation_on_parent);
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

  base::WeakPtr<SurfaceClient> client() { return surface_client_; }

  bool has_deadline() const { return deadline_ && deadline_->has_deadline(); }

  base::Optional<base::TimeTicks> deadline_for_testing() const {
    return deadline_->deadline_for_testing();
  }

  // Inherits the same deadline as the one specified by |surface|. A deadline
  // may be set further out in order to avoid doing unnecessary work while a
  // parent surface is blocked on dependencies. A deadline may be shortened
  // in order to minimize guttering (by unblocking children blocked on their
  // grandchildren sooner).
  void InheritActivationDeadlineFrom(Surface* surface);

  void SetPreviousFrameSurface(Surface* surface);

  // Increments the reference count on resources specified by |resources|.
  void RefResources(const std::vector<TransferableResource>& resources);

  // Decrements the reference count on resources specified by |resources|.
  void UnrefResources(const std::vector<ReturnedResource>& resources);

  bool needs_sync_tokens() const { return needs_sync_tokens_; }

  bool block_activation_on_parent() const {
    return block_activation_on_parent_;
  }

  // Returns false if |frame| is invalid.
  // |frame_rejected_callback| will be called once if the frame will not be
  // displayed.
  // |presented_callback| is called when the |frame| has been turned into light
  // the first time on display, or if the |frame| is replaced by another prior
  // to display.
  bool QueueFrame(CompositorFrame frame,
                  uint64_t frame_index,
                  base::ScopedClosureRunner frame_rejected_callback,
                  PresentedCallback presented_callback);

  // Notifies the Surface that a blocking SurfaceId now has an active
  // frame.
  void NotifySurfaceIdAvailable(const SurfaceId& surface_id);

  // Returns whether the Surface is blocked on the provided |surface_id| or a
  // predecessor.
  bool IsBlockedOn(const SurfaceId& surface_id) const;

  // Called if a deadline has been hit and this surface is not yet active but
  // it's marked as respecting deadlines.
  void ActivatePendingFrameForDeadline(
      base::Optional<base::TimeDelta> duration);

  using CopyRequestsMap =
      std::multimap<RenderPassId, std::unique_ptr<CopyOutputRequest>>;

  // Adds each CopyOutputRequest in the current frame to copy_requests. The
  // caller takes ownership of them. |copy_requests| is keyed by RenderPass
  // ids.
  void TakeCopyOutputRequests(CopyRequestsMap* copy_requests);

  // Takes CopyOutputRequests made at the client level and adds them to this
  // Surface.
  void TakeCopyOutputRequestsFromClient();

  // Returns whether there is a CopyOutputRequest inside the active frame or at
  // the client level.
  bool HasCopyOutputRequests();

  // Returns the most recent frame that is eligible to be rendered.
  // You must check whether HasActiveFrame() returns true before calling this
  // method.
  const CompositorFrame& GetActiveFrame() const;

  // Returns the currently pending frame. You must check where HasPendingFrame()
  // returns true before calling this method.
  const CompositorFrame& GetPendingFrame();

  // Returns a number that increments by 1 every time a new frame is enqueued.
  uint64_t GetActiveFrameIndex() const {
    return active_frame_data_ ? active_frame_data_->frame_index : 0;
  }

  void TakeLatencyInfo(std::vector<ui::LatencyInfo>* latency_info);
  bool TakePresentedCallback(PresentedCallback* callback);
  void RunDrawCallback();
  void NotifyAggregatedDamage(const gfx::Rect& damage_rect,
                              base::TimeTicks expected_display_time);

  const base::flat_set<SurfaceId>& active_referenced_surfaces() const {
    return active_referenced_surfaces_;
  }

  // Returns the set of dependencies blocking this surface's pending frame
  // that themselves have not yet activated.
  const base::flat_set<SurfaceId>& activation_dependencies() const {
    return activation_dependencies_;
  }

  // Returns the set of activation dependencies that have been ignored because
  // the last CompositorFrame was activated due to a deadline. Late
  // dependencies activate immediately when they arrive.
  const base::flat_set<SurfaceId>& late_activation_dependencies() const {
    return late_activation_dependencies_;
  }

  bool HasActiveFrame() const { return active_frame_data_.has_value(); }
  bool HasPendingFrame() const { return pending_frame_data_.has_value(); }
  bool HasUndrawnActiveFrame() const {
    return HasActiveFrame() && !active_frame_data_->frame_processed;
  }

  // Returns true if at any point, another Surface's CompositorFrame has
  // depended on this Surface.
  bool HasDependentFrame() const { return seen_first_surface_dependency_; }

  // SurfaceDeadlineClient implementation:
  void OnDeadline(base::TimeDelta duration) override;

  // Called when this surface will be included in the next display frame.
  void OnWillBeDrawn();

  // Called when |surface_id| is activated for the first time and its part of a
  // referenced SurfaceRange.
  void OnChildActivated(const SurfaceId& surface_id);

  // Called when this surface is embedded by another Surface's CompositorFrame.
  void OnSurfaceDependencyAdded();

 private:
  struct SequenceNumbers {
    uint32_t parent_sequence_number = 0u;
    uint32_t child_sequence_number = 0u;
  };

  struct FrameData {
    FrameData(CompositorFrame&& frame,
              uint64_t frame_index,
              PresentedCallback presented_callback);
    FrameData(FrameData&& other);
    ~FrameData();
    FrameData& operator=(FrameData&& other);

    CompositorFrame frame;
    uint64_t frame_index;
    // Whether the frame has been processed (displayed, or discarded), or not.
    bool frame_processed = false;
    // TODO(sad): This callback would ideally become part of SurfaceClient API.
    PresentedCallback presented_callback;
  };

  // Rejects CompositorFrames submitted to surfaces referenced from this
  // CompositorFrame as fallbacks. This saves some CPU cycles to allow
  // children to catch up to the parent.
  void RejectCompositorFramesToFallbackSurfaces();

  // Updates surface references of the surface using the referenced
  // surfaces from the most recent CompositorFrame.
  // Modifies surface references stored in SurfaceManager.
  void UpdateSurfaceReferences();

  // Called to prevent additional CompositorFrames from being accepted into this
  // surface. Once a Surface is closed, it cannot accept CompositorFrames again.
  void Close();

  // Updates the FrameSinkIds observed by this surface to be equal to
  // |new_observed_sinks|.
  void UpdateObservedSinks(
      const base::flat_set<FrameSinkId>& new_observed_sinks);

  // Recomputes active references for this surface when it activates. This
  // method will also update the observed sinks based on the referenced ranges
  // in the submitted compositor frame.
  void RecomputeActiveReferencedSurfaces();

  void ActivatePendingFrame(base::Optional<base::TimeDelta> duration);

  // Called when all of the surface's dependencies have been resolved.
  void ActivateFrame(FrameData frame_data,
                     base::Optional<base::TimeDelta> duration);

  // Resolve the activation deadline specified by |current_frame| into a wall
  // time to be used by SurfaceDependencyDeadline.
  FrameDeadline ResolveFrameDeadline(const CompositorFrame& current_frame);

  // Updates the set of unresolved activation dependenices of the
  // |current_frame|. If the deadline requested by the frame is 0 then no
  // dependencies will be added even if they're not yet available.
  void UpdateActivationDependencies(const CompositorFrame& current_frame);

  void ComputeChangeInDependencies(
      const base::flat_map<FrameSinkId, SequenceNumbers>& new_dependencies);

  void UnrefFrameResourcesAndRunCallbacks(base::Optional<FrameData> frame_data);
  void ClearCopyRequests();

  void TakeLatencyInfoFromPendingFrame(
      std::vector<ui::LatencyInfo>* latency_info);
  static void TakeLatencyInfoFromFrame(
      CompositorFrame* frame,
      std::vector<ui::LatencyInfo>* latency_info);

  void RequestCopyOfOutput(std::unique_ptr<CopyOutputRequest> copy_request);

  const SurfaceInfo surface_info_;
  SurfaceId previous_frame_surface_id_;
  SurfaceManager* const surface_manager_;
  base::WeakPtr<SurfaceClient> surface_client_;
  std::unique_ptr<SurfaceDependencyDeadline> deadline_;

  base::Optional<FrameData> pending_frame_data_;
  base::Optional<FrameData> active_frame_data_;
  bool closed_ = false;
  bool seen_first_frame_activation_ = false;
  bool seen_first_surface_embedding_ = false;
  bool seen_first_surface_dependency_ = false;
  const bool needs_sync_tokens_;
  const bool block_activation_on_parent_;

  base::flat_set<SurfaceId> activation_dependencies_;
  base::flat_set<SurfaceId> late_activation_dependencies_;

  // A map from FrameSinkIds of SurfaceIds that this surface depends on for
  // activation to the latest local_id associated with the given FrameSinkId
  // that this surface is dependent on. This map is used to determine which
  // FrameSinkIds this surface would like to observe activations for. Once
  // the latest activated SurfaceId associated with the given FrameSinkId
  // passes the local_id in the map, then this surface is no longer interested
  // in observing activations for that FrameSinkId.
  base::flat_map<FrameSinkId, SequenceNumbers> frame_sink_id_dependencies_;

  // A set of all valid SurfaceIds contained |last_surface_id_for_range_| to
  // avoid recompution.
  base::flat_set<SurfaceId> active_referenced_surfaces_;

  // Keeps track of the referenced surface for each SurfaceRange. i.e the i-th
  // element is the referenced SurfaceId in the i-th SurfaceRange. If a
  // SurfaceRange doesn't contain any active surfaces then the corresponding
  // entry in this vector is an unvalid SurfaceId.
  std::vector<SurfaceId> last_surface_id_for_range_;

  // Frame sinks that this surface observe for activation events.
  base::flat_set<FrameSinkId> observed_sinks_;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_H_
