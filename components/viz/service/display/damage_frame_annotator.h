// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DAMAGE_FRAME_ANNOTATOR_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DAMAGE_FRAME_ANNOTATOR_H_

#include <vector>

#include "components/viz/service/display/surface_aggregator.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace viz {

class CompositorFrame;
class RenderPass;

// Draws a red outline around the root RenderPasses damage rect.
class DamageFrameAnnotator : public SurfaceAggregator::FrameAnnotator {
 public:
  DamageFrameAnnotator();
  ~DamageFrameAnnotator() override;

  // SurfaceAggregator::FrameAnnotator implementation.
  void AnnotateAggregatedFrame(CompositorFrame* frame) override;

 private:
  struct Highlight {
    SkColor color;
    int width;
  };

  struct AnnotationData {
    gfx::Rect rect;
    gfx::Transform transform;
    Highlight highlight;
  };

  void AnnotateRootRenderPass(RenderPass* render_pass);

  std::vector<AnnotationData> annotations_;

  DISALLOW_COPY_AND_ASSIGN(DamageFrameAnnotator);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DAMAGE_FRAME_ANNOTATOR_H_
