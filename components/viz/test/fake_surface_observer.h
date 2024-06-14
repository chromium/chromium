// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_FAKE_SURFACE_OBSERVER_H_
#define COMPONENTS_VIZ_TEST_FAKE_SURFACE_OBSERVER_H_

#include "base/containers/flat_set.h"
#include "base/scoped_observation.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/surfaces/surface_observer.h"

namespace viz {

class SurfaceManager;

class FakeSurfaceObserver : public SurfaceObserver {
 public:
  // If |damage_display| is true, the observer will indicate display damage when
  // a surface is damaged.
  explicit FakeSurfaceObserver(SurfaceManager* manager,
                               bool damage_display = true);
  ~FakeSurfaceObserver() override;

  const BeginFrameAck& last_ack() const { return last_ack_; }

  bool IsSurfaceDamaged(const SurfaceId& surface_id) const;

  bool IsSurfaceSubtreeDamaged(const SurfaceId& surface_id) const;

  const SurfaceId& last_created_surface_id() const {
    return last_created_surface_id_;
  }

  const SurfaceInfo& last_surface_info() const { return last_surface_info_; }

  void set_damage_display(bool damage_display) {
    damage_display_ = damage_display;
  }

  void Reset();

 private:
  // SurfaceObserver implementation:
  bool OnSurfaceDamaged(const SurfaceId& surface_id,
                        const BeginFrameAck& ack,
                        HandleInteraction handle_interaction) override;
  void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) override;
  void OnSurfaceActivated(const SurfaceId& surface_id) override;

  base::ScopedObservation<SurfaceManager, SurfaceObserver>
      observer_registration_{this};
  bool damage_display_;
  BeginFrameAck last_ack_;
  base::flat_set<SurfaceId> damaged_surfaces_;
  base::flat_set<SurfaceId> surface_subtree_damaged_;
  SurfaceId last_created_surface_id_;
  SurfaceInfo last_surface_info_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_FAKE_SURFACE_OBSERVER_H_
