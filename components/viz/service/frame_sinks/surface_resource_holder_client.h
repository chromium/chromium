// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_SURFACE_RESOURCE_HOLDER_CLIENT_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_SURFACE_RESOURCE_HOLDER_CLIENT_H_

#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class VIZ_SERVICE_EXPORT SurfaceResourceHolderClient {
 public:
  virtual ~SurfaceResourceHolderClient() = default;

  // ReturnResources gets called when the display compositor is done using the
  // resources so that the client can use them.
  virtual void ReturnResources(std::vector<ReturnedResource> resources) = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_SURFACE_RESOURCE_HOLDER_CLIENT_H_
