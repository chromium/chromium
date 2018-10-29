// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_renderer.h"

#include "base/trace_event/trace_event.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/gl_bindings.h"
#include "chrome/browser/vr/render_info.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_scene.h"

namespace vr {

UiRenderer::UiRenderer(UiScene* scene, UiElementRenderer* ui_element_renderer)
    : scene_(scene), ui_element_renderer_(ui_element_renderer) {}

UiRenderer::~UiRenderer() = default;

// TODO(crbug.com/767515): UiRenderer must not care about the elements it's
// rendering and be platform agnostic, each element should know how to render
// itself correctly.
void UiRenderer::Draw(const RenderInfo& render_info) {
  glEnable(GL_CULL_FACE);
  DrawUiView(render_info, scene_->GetElementsToDraw());
}

void UiRenderer::DrawWebVrOverlayForeground(const RenderInfo& render_info) {
  // The WebVR overlay foreground is drawn as a separate pass, so we need to set
  // up our gl state before drawing.
  glEnable(GL_CULL_FACE);

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  DrawUiView(render_info, scene_->GetWebVrOverlayElementsToDraw());
}

void UiRenderer::DrawUiView(const RenderInfo& render_info,
                            const std::vector<const UiElement*>& elements) {
  if (elements.empty())
    return;

  TRACE_EVENT0("gpu", "UiRenderer::DrawUiView");

  auto sorted_elements = GetElementsInDrawOrder(elements);

  for (auto& camera_model :
       {render_info.left_eye_model, render_info.right_eye_model}) {
    glViewport(camera_model.viewport.x(), camera_model.viewport.y(),
               camera_model.viewport.width(), camera_model.viewport.height());

    DrawElements(camera_model, sorted_elements, render_info);
  }
}

void UiRenderer::DrawElements(const CameraModel& camera_model,
                              const std::vector<const UiElement*>& elements,
                              const RenderInfo& render_info) {
  if (elements.empty()) {
    return;
  }
  for (const auto* element : elements) {
    DrawElement(camera_model, *element);
  }
  ui_element_renderer_->Flush();
}

void UiRenderer::DrawElement(const CameraModel& camera_model,
                             const UiElement& element) {
  DCHECK_GE(element.draw_phase(), 0);

  // Set default GL parameters for each element.
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  element.Render(ui_element_renderer_, camera_model);
}

std::vector<const UiElement*> UiRenderer::GetElementsInDrawOrder(
    const std::vector<const UiElement*>& elements) {
  std::vector<const UiElement*> sorted_elements = elements;

  // Sort elements primarily based on their draw phase (lower draw phase first)
  // and secondarily based on their tree order (as specified by the sorted
  // |elements| vector).
  // TODO(vollick): update the predicate to take into account some notion of "3d
  // rendering contexts" and the ordering of the reticle wrt to other elements.
  std::stable_sort(sorted_elements.begin(), sorted_elements.end(),
                   [](const UiElement* first, const UiElement* second) {
                     return first->draw_phase() < second->draw_phase();
                   });

  return sorted_elements;
}

}  // namespace vr
