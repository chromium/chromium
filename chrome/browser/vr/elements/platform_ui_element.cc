// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/platform_ui_element.h"

#include "chrome/browser/vr/platform_ui_input_delegate.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "ui/gfx/geometry/rect_f.h"

namespace vr {

PlatformUiElement::PlatformUiElement() {
  set_scrollable(true);
}

PlatformUiElement::~PlatformUiElement() = default;

void PlatformUiElement::Render(UiElementRenderer* renderer,
                               const CameraModel& model) const {
  if (texture_id_) {
    renderer->DrawTexturedQuad(texture_id_, 0, texture_location_,
                               model.view_proj_matrix * world_space_transform(),
                               GetClipRect(), computed_opacity(), size(),
                               corner_radius(), true);
  }
}

void PlatformUiElement::OnHoverEnter(const gfx::PointF& position,
                                     base::TimeTicks timestamp) {
  if (delegate_)
    delegate_->OnHoverEnter(position, timestamp);
}

void PlatformUiElement::OnHoverLeave(base::TimeTicks timestamp) {
  if (delegate_)
    delegate_->OnHoverLeave(timestamp);
}

void PlatformUiElement::OnHoverMove(const gfx::PointF& position,
                                    base::TimeTicks timestamp) {
  if (delegate_)
    delegate_->OnHoverMove(position, timestamp);
}

void PlatformUiElement::OnButtonDown(const gfx::PointF& position,
                                     base::TimeTicks timestamp) {
  if (delegate_)
    delegate_->OnButtonDown(position, timestamp);
}

void PlatformUiElement::OnButtonUp(const gfx::PointF& position,
                                   base::TimeTicks timestamp) {
  if (delegate_)
    delegate_->OnButtonUp(position, timestamp);
}

void PlatformUiElement::OnTouchMove(const gfx::PointF& position,
                                    base::TimeTicks timestamp) {
  if (delegate_)
    delegate_->OnTouchMove(position, timestamp);
}

void PlatformUiElement::OnFlingCancel(std::unique_ptr<InputEvent> gesture,
                                      const gfx::PointF& position) {
  if (delegate_)
    delegate_->OnInputEvent(std::move(gesture), position);
}

void PlatformUiElement::OnScrollBegin(std::unique_ptr<InputEvent> gesture,
                                      const gfx::PointF& position) {
  if (delegate_)
    delegate_->OnInputEvent(std::move(gesture), position);
}

void PlatformUiElement::OnScrollUpdate(std::unique_ptr<InputEvent> gesture,
                                       const gfx::PointF& position) {
  if (delegate_)
    delegate_->OnInputEvent(std::move(gesture), position);
}

void PlatformUiElement::OnScrollEnd(std::unique_ptr<InputEvent> gesture,
                                    const gfx::PointF& position) {
  if (delegate_)
    delegate_->OnInputEvent(std::move(gesture), position);
}

void PlatformUiElement::SetTextureId(unsigned int texture_id) {
  texture_id_ = texture_id;
}

void PlatformUiElement::SetTextureLocation(GlTextureLocation location) {
  texture_location_ = location;
}

void PlatformUiElement::SetDelegate(PlatformUiInputDelegate* delegate) {
  delegate_ = delegate;
}

}  // namespace vr
