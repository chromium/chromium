// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_DAMAGE_TRACKER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_DAMAGE_TRACKER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/surfaces/surface_observer.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class SurfaceAggregator;
class SurfaceInfo;
class SurfaceManager;

// DisplayDamageTracker is used to track Surfaces damage that belong to current
// Display. It tracks pending damage when clients received BeginFrames but
// didn't replied yet and it tracks whether damage to those surfaces contribute
// to Display damage. Used by DisplayScheduler to determine frame deadlines.
class VIZ_SERVICE_EXPORT DisplayDamageTracker : public SurfaceObserver {
 public:
  class VIZ_SERVICE_EXPORT Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnDisplayDamaged(SurfaceId surface_id) = 0;
    virtual void OnRootFrameMissing(bool missing) = 0;
    virtual void OnPendingSurfacesChanged() = 0;
  };

  DisplayDamageTracker(SurfaceManager* surface_manager,
                       SurfaceAggregator* aggregator);
  ~DisplayDamageTracker() override;

  DisplayDamageTracker(const DisplayDamageTracker&) = delete;
  DisplayDamageTracker& operator=(const DisplayDamageTracker&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

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

  bool root_frame_missing() const { return root_frame_missing_; }
  bool IsRootSurfaceValid() const;

  bool expecting_root_surface_damage_because_of_resize() const {
    return expecting_root_surface_damage_because_of_resize_;
  }

  void reset_expecting_root_surface_damage_because_of_resize() {
    expecting_root_surface_damage_because_of_resize_ = false;
  }

  // SurfaceObserver implementation.
  void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) override {}
  void OnSurfaceActivated(const SurfaceId& surface_id) override {}
  void OnSurfaceMarkedForDestruction(const SurfaceId& surface_id) override;
  bool OnSurfaceDamaged(const SurfaceId& surface_id,
                        const BeginFrameAck& ack) override;
  void OnSurfaceDestroyed(const SurfaceId& surface_id) override;
  void OnSurfaceDamageExpected(const SurfaceId& surface_id,
                               const BeginFrameArgs& args) override;

 protected:
  struct SurfaceBeginFrameState {
    BeginFrameArgs last_args;
    BeginFrameAck last_ack;
  };

  friend class base::RefCounted<DisplayDamageTracker>;
  virtual bool SurfaceHasUnackedFrame(const SurfaceId& surface_id) const;
  virtual void UpdateRootFrameMissing();
  void SetRootFrameMissing(bool missing);

  // Indicates that there was damage to one of the surfaces.
  void ProcessSurfaceDamage(const SurfaceId& surface_id,
                            const BeginFrameAck& ack,
                            bool display_damaged);

  // Used to send corresponding notifications to observers.
  void NotifyDisplayDamaged(SurfaceId surface_id);
  void NotifyRootFrameMissing(bool missing);
  void NotifyPendingSurfacesChanged();

  base::ObserverList<Observer>::Unchecked observers_;
  SurfaceManager* const surface_manager_;
  SurfaceAggregator* const aggregator_;

  bool root_frame_missing_ = true;

  bool expecting_root_surface_damage_because_of_resize_ = false;

  base::flat_map<SurfaceId, SurfaceBeginFrameState> surface_states_;
  std::vector<SurfaceId> surfaces_to_ack_on_next_draw_;

  SurfaceId root_surface_id_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_DAMAGE_TRACKER_H_
