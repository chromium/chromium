// Copyright 2016 The Chromium Authors
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

  auto old_it = old_referenced_surfaces.begin();
  auto old_end = old_referenced_surfaces.end();
  auto new_it = new_referenced_surfaces.begin();
  auto new_end = new_referenced_surfaces.end();

  // Do a linear walk through both old and new references to compute added and
  // removed entries.
  while (old_it != old_end || new_it != new_end) {
    if (old_it == old_end) {
      references_to_add->push_back(
          SurfaceReference(parent_surface_id, *new_it++));
    } else if (new_it == new_end) {
      references_to_remove->push_back(
          SurfaceReference(parent_surface_id, *old_it++));
    } else if (*old_it < *new_it) {
      references_to_remove->push_back(
          SurfaceReference(parent_surface_id, *old_it++));
    } else if (*new_it < *old_it) {
      references_to_add->push_back(
          SurfaceReference(parent_surface_id, *new_it++));
    } else {
      ++new_it;
      ++old_it;
    }
  }
}

}  // namespace viz
