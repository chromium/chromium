// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_MANAGER_DELEGATE_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_MANAGER_DELEGATE_H_

#include <string_view>

#include "components/viz/service/viz_service_export.h"

namespace viz {

class VIZ_SERVICE_EXPORT SurfaceManagerDelegate {
 public:
  virtual ~SurfaceManagerDelegate() = default;

  // Returns the debug label associated with |frame_sink_id| if any.
  virtual std::string_view GetFrameSinkDebugLabel(
      const FrameSinkId& frame_sink_id) const = 0;

  // Indicates that the set of frame sinks being aggregated for display has
  // changed since the previous aggregation.
  virtual void AggregatedFrameSinksChanged() = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_MANAGER_DELEGATE_H_
