// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/referenced_surface_tracker.h"

#include "base/check.h"

namespace viz {

void GetSurfaceReferenceDifference(
    const SurfaceId& parent_surface_id,
    const base::flat_set<SurfaceId>& old_referenced_surfaces,
    const base::flat_set<SurfaceId>& new_referenced_surfaces,
    std::vector<SurfaceReference>* references_to_add,
    std::vector<SurfaceReference>* references_to_remove) {
  DCHECK(parent_surface_id.is_valid());

  // Find SurfaceIds in |old_referenced_surfaces| that aren't referenced
  // anymore.
  for (const SurfaceId& surface_id : old_referenced_surfaces) {
    if (new_referenced_surfaces.count(surface_id) == 0) {
      references_to_remove->push_back(
          SurfaceReference(parent_surface_id, surface_id));
    }
  }

  // Find SurfaceIds in |new_referenced_surfaces| that aren't already
  // referenced.
  for (const SurfaceId& surface_id : new_referenced_surfaces) {
    if (old_referenced_surfaces.count(surface_id) == 0) {
      references_to_add->push_back(
          SurfaceReference(parent_surface_id, surface_id));
    }
  }
}

}  // namespace viz
