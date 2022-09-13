// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DAMAGE_FRAME_ANNOTATOR_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DAMAGE_FRAME_ANNOTATOR_H_

#include <vector>

#include "components/viz/service/display/surface_aggregator.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace viz {

class AggregatedFrame;
class AggregatedRenderPass;

// Draws a red outline around the root RenderPasses damage rect.
class DamageFrameAnnotator : public SurfaceAggregator::FrameAnnotator {
 public:
  DamageFrameAnnotator();

  DamageFrameAnnotator(const DamageFrameAnnotator&) = delete;
  DamageFrameAnnotator& operator=(const DamageFrameAnnotator&) = delete;

  ~DamageFrameAnnotator() override;

  // SurfaceAggregator::FrameAnnotator implementation.
  void AnnotateAggregatedFrame(AggregatedFrame* frame) override;

 private:
  struct Highlight {
    SkColor4f color;
    int width;
  };

  struct AnnotationData {
    gfx::Rect rect;
    gfx::Transform transform;
    Highlight highlight;
  };

  void AnnotateRootRenderPass(AggregatedRenderPass* render_pass);

  std::vector<AnnotationData> annotations_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DAMAGE_FRAME_ANNOTATOR_H_
