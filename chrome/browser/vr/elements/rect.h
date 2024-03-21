// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_RECT_H_
#define CHROME_BROWSER_VR_ELEMENTS_RECT_H_

#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"

namespace vr {

// A rect renders a rect via a shader (no texture). This rect can optionally
// have a corner radius and may have a difference center and edge color. If
// these colors are different, we will render a radial gradient between these
// two colors. This radial gradient is not aspect correct; it will be elliptical
// if the rect is stretched. This is intended to serve as a background to be put
// behind other elements.
class VR_UI_EXPORT Rect : public UiElement {
 public:
  Rect();

  Rect(const Rect&) = delete;
  Rect& operator=(const Rect&) = delete;

  ~Rect() override;

  // Syntactic sugar for setting both the edge and center colors simultaneously.
  void SetColor(SkColor color);

  SkColor center_color() const { return center_color_; }
  void SetCenterColor(SkColor color);

  SkColor edge_color() const { return edge_color_; }
  void SetEdgeColor(SkColor color);

  void OnColorAnimated(const SkColor& color,
                       int target_property_id,
                       gfx::KeyframeModel* keyframe_model) override;

  void Render(UiElementRenderer* renderer,
              const CameraModel& model) const override;

  void SetLocalOpacity(float opacity);

  void OnFloatAnimated(const float& value,
                       int target_property_id,
                       gfx::KeyframeModel* keyframe_model) override;

 private:
  SkColor center_color_ = SK_ColorWHITE;
  SkColor edge_color_ = SK_ColorWHITE;

  // This value is not inherited by descendants.
  float local_opacity_ = 1.0f;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_RECT_H_
