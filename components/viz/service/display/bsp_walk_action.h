// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_BSP_WALK_ACTION_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_BSP_WALK_ACTION_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/draw_polygon.h"

namespace viz {

class VIZ_SERVICE_EXPORT BspWalkAction {
 public:
  virtual void operator()(DrawPolygon* item) = 0;
};

// The BspTree class takes ownership of all the DrawPolygons returned in list_
// so the BspTree must be preserved while elements in that vector are in use.
class VIZ_SERVICE_EXPORT BspWalkActionDrawPolygon : public BspWalkAction {
 public:
  void operator()(DrawPolygon* item) override;

  BspWalkActionDrawPolygon(DirectRenderer* renderer,
                           const gfx::Rect& render_pass_scissor,
                           bool using_scissor_as_optimization);

 private:
  raw_ptr<DirectRenderer> renderer_;
  const raw_ref<const gfx::Rect> render_pass_scissor_;
  bool using_scissor_as_optimization_;
};

class VIZ_SERVICE_EXPORT BspWalkActionToVector : public BspWalkAction {
 public:
  explicit BspWalkActionToVector(
      std::vector<raw_ptr<DrawPolygon, VectorExperimental>>* in_list);
  void operator()(DrawPolygon* item) override;

 private:
  raw_ptr<std::vector<raw_ptr<DrawPolygon, VectorExperimental>>> list_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_BSP_WALK_ACTION_H_
