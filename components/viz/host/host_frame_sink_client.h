// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_HOST_FRAME_SINK_CLIENT_H_
#define COMPONENTS_VIZ_HOST_HOST_FRAME_SINK_CLIENT_H_

#include <stdint.h>

#include "base/time/time.h"

namespace viz {

class SurfaceInfo;

class HostFrameSinkClient {
 public:
  // Called when a CompositorFrame with a new SurfaceId activates for the first
  // time.
  virtual void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) = 0;

  // Called when a CompositorFrame with a new frame token is provided.
  virtual void OnFrameTokenChanged(uint32_t frame_token,
                                   base::TimeTicks activation_time) = 0;

 protected:
  virtual ~HostFrameSinkClient() {}
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_HOST_FRAME_SINK_CLIENT_H_
