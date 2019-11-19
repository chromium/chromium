// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_CLIENT_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_CLIENT_H_

#include "components/viz/common/quads/render_pass.h"

namespace gfx {
struct CALayerParams;
}  // namespace gfx

namespace viz {
class FrameSinkId;

class DisplayClient {
 public:
  virtual ~DisplayClient() {}
  virtual void DisplayOutputSurfaceLost() = 0;
  // It is expected that |render_pass| would only be modified to insert debug
  // quads.
  virtual void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                                      RenderPassList* render_passes) = 0;
  virtual void DisplayDidDrawAndSwap() = 0;
  virtual void DisplayDidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) = 0;
  virtual void DisplayDidCompleteSwapWithSize(const gfx::Size& pixel_size) = 0;
  virtual void SetPreferredFrameInterval(base::TimeDelta interval) = 0;
  virtual base::TimeDelta GetPreferredFrameIntervalForFrameSinkId(
      const FrameSinkId& id) = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_CLIENT_H_
