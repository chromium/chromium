// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_DAMAGE_TRACKER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_DAMAGE_TRACKER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/surfaces/surface_observer.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class SurfaceAggregator;
class SurfaceManager;

// DisplayDamageTracker is used to track Surfaces damage that belong to current
// Display. It tracks pending damage when clients received BeginFrames but
// didn't replied yet and it tracks whether damage to those surfaces contribute
// to Display damage. Used by DisplayScheduler to determine frame deadlines.
class VIZ_SERVICE_EXPORT DisplayDamageTracker : public SurfaceObserver {
 public:
  class VIZ_SERVICE_EXPORT Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnDisplayDamaged(SurfaceId surface_id) = 0;
    virtual void OnRootFrameMissing(bool missing) = 0;
    virtual void OnPendingSurfacesChanged() = 0;
  };

  DisplayDamageTracker(SurfaceManager* surface_manager,
                       SurfaceAggregator* aggregator);
  ~DisplayDamageTracker() override;

  DisplayDamageTracker(const DisplayDamageTracker&) = delete;
  DisplayDamageTracker& operator=(const DisplayDamageTracker&) = delete;

  // Sets the source_id associated with this displays begin frame source.
  // DisplayDamageTracker ignores expected damage from frame sinks that received
  // a begin frame from a different begin frame source after this is set.
  void SetDisplayBeginFrameSourceId(uint64_t begin_frame_source_id);

  void SetDelegate(Delegate* delegate);

  // Notification that there was a resize and we should expect root surface
  // damage.
  void DisplayResized();

  // Notification that the root surface changed.
  void SetNewRootSurface(const SurfaceId& root_surface_id);

  // Mark root surface as damaged.
  void SetRootSurfaceDamaged();

  // Send Surface Acks to damaged surfaces after draw.
  void RunDrawCallbacks();

  // This returns whether there are pending surfaces. SurfaceIs pending if the
  // corresponding CompositorFrameSink has received BeginFrame but hasn't
  // replied with Ack yet.
  bool HasPendingSurfaces(const BeginFrameArgs& begin_frame_args);

  // Returns true if any of the damage received was due to an ongoing scroll or
  // touch interaction.
  bool HasDamageDueToInteraction();

  // Called after a frame finishes (may or may not result in a draw).
  void DidFinishFrame();

  // Returns true if damage to this Surface could affect the display.
  bool CheckForDisplayDamage(const SurfaceId& surface_id);

  bool root_frame_missing() const { return root_frame_missing_; }
  bool IsRootSurfaceValid() const;

  bool expecting_root_surface_damage_because_of_resize() const {
    return expecting_root_surface_damage_because_of_resize_;
  }

  void reset_expecting_root_surface_damage_because_of_resize() {
    expecting_root_surface_damage_because_of_resize_ = false;
  }

  // SurfaceObserver implementation.
  void OnSurfaceMarkedForDestruction(const SurfaceId& surface_id) override;
  bool OnSurfaceDamaged(const SurfaceId& surface_id,
                        const BeginFrameAck& ack,
                        HandleInteraction handle_interaction) override;
  void OnSurfaceDamageExpected(const SurfaceId& surface_id,
                               const BeginFrameArgs& args) override;

 protected:
  struct SurfaceBeginFrameState {
    BeginFrameArgs last_args;
    BeginFrameAck last_ack;
  };

  virtual bool SurfaceHasUnackedFrame(const SurfaceId& surface_id) const;
  virtual void UpdateRootFrameMissing();
  void SetRootFrameMissing(bool missing);

  // Checks if the begin frame `source_id` is for this display. This will return
  // true if:
  // 1. `source_id` matches the display source id.
  // 2. Display source id was never set.
  // 3. `source_id` is a manual source id since that could be relevant for any
  //    display.
  bool CheckBeginFrameSourceId(uint64_t source_id);

  // Indicates that there was damage to one of the surfaces.
  void ProcessSurfaceDamage(const SurfaceId& surface_id,
                            const BeginFrameAck& ack,
                            bool display_damaged,
                            HandleInteraction handle_interaction);

  // Used to send corresponding notifications to observers.
  void NotifyDisplayDamaged(SurfaceId surface_id);
  void NotifyRootFrameMissing(bool missing);
  void NotifyPendingSurfacesChanged();

  raw_ptr<Delegate> delegate_ = nullptr;
  const raw_ptr<SurfaceManager> surface_manager_;
  const raw_ptr<SurfaceAggregator> aggregator_;

  std::optional<uint64_t> begin_frame_source_id_;
  bool root_frame_missing_ = true;

  bool expecting_root_surface_damage_because_of_resize_ = false;

  bool has_surface_damage_due_to_interaction_ = false;

  base::flat_map<SurfaceId, SurfaceBeginFrameState> surface_states_;
  std::vector<SurfaceId> surfaces_to_ack_on_next_draw_;

  SurfaceId root_surface_id_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_DAMAGE_TRACKER_H_
