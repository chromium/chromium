// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_REFERENCED_SURFACE_TRACKER_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_REFERENCED_SURFACE_TRACKER_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/surfaces/surface_reference.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// Finds the difference between |old_referenced_surfaces| and
// |new_referenced_surfaces|. Populates |references_to_add| and
// |references_to_remove| based on the difference using |parent_surface_id| as
// the parent for references.
void VIZ_SERVICE_EXPORT GetSurfaceReferenceDifference(
    const SurfaceId& parent_surface_id,
    const base::flat_set<SurfaceId>& old_referenced_surfaces,
    const base::flat_set<SurfaceId>& new_referenced_surfaces,
    std::vector<SurfaceReference>* references_to_add,
    std::vector<SurfaceReference>* references_to_remove);

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_REFERENCED_SURFACE_TRACKER_H_
