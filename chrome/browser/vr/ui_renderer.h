// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_RENDERER_H_
#define CHROME_BROWSER_VR_UI_RENDERER_H_

#include "chrome/browser/vr/ui_input_manager.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "ui/gfx/geometry/rect_f.h"

namespace vr {

class UiElement;
class UiScene;
class UiElement;
class UiElementRenderer;
struct CameraModel;
struct RenderInfo;

// Renders a UI scene.
class VR_UI_EXPORT UiRenderer {
 public:
  UiRenderer(UiScene* scene, UiElementRenderer* ui_element_renderer);
  ~UiRenderer();

  void Draw(const RenderInfo& render_info);

  // This is exposed separately because we do a separate pass to render this
  // content into an optimized viewport.
  void DrawWebVrOverlayForeground(const RenderInfo& render_info);

  static std::vector<const UiElement*> GetElementsInDrawOrder(
      const std::vector<const UiElement*>& elements);

 private:
  void DrawUiView(const RenderInfo& render_info,
                  const std::vector<const UiElement*>& elements);
  void DrawElements(const CameraModel& camera_model,
                    const std::vector<const UiElement*>& elements,
                    const RenderInfo& render_info);
  void DrawElement(const CameraModel& camera_model, const UiElement& element);

  UiScene* scene_ = nullptr;
  UiElementRenderer* ui_element_renderer_ = nullptr;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_RENDERER_H_
