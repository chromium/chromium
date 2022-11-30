// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_LATEST_LOCAL_SURFACE_ID_LOOKUP_DELEGATE_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_LATEST_LOCAL_SURFACE_ID_LOOKUP_DELEGATE_H_

namespace viz {
// Used by HitTestManager to talk to Display.
class LatestLocalSurfaceIdLookupDelegate {
 public:
  // Called to get the surface with |frame_sink_id| that is used at surface
  // aggregation time.
  virtual LocalSurfaceId GetSurfaceAtAggregation(
      const FrameSinkId& frame_sink_id) const = 0;

 protected:
  virtual ~LatestLocalSurfaceIdLookupDelegate() = default;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_LATEST_LOCAL_SURFACE_ID_LOOKUP_DELEGATE_H_
