// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/keyboard.h"

#include "chrome/browser/vr/frame_lifecycle.h"
#include "chrome/browser/vr/ui_element_renderer.h"

namespace vr {

Keyboard::Keyboard() {
  SetName(kKeyboard);
  set_focusable(false);
  set_hit_testable(true);
  SetVisibleImmediately(false);
}

Keyboard::~Keyboard() = default;

void Keyboard::SetKeyboardDelegate(KeyboardDelegate* keyboard_delegate) {
  delegate_ = keyboard_delegate;
  UpdateDelegateVisibility();
}

void Keyboard::OnTouchStateUpdated(bool is_touching,
                                   const gfx::PointF& touch_position) {
  if (!delegate_)
    return;

  delegate_->OnTouchStateUpdated(is_touching, touch_position);
}

void Keyboard::HitTest(const HitTestRequest& request,
                       HitTestResult* result) const {
  if (!delegate_)
    return;

  result->type = HitTestResult::Type::kNone;
  gfx::Point3F hit_point;
  if (!delegate_->HitTest(request.ray_origin, request.ray_target, &hit_point))
    return;

  float distance_to_plane = (hit_point - request.ray_origin).Length();
  if (distance_to_plane < 0 ||
      distance_to_plane > request.max_distance_to_plane) {
    return;
  }

  result->type = HitTestResult::Type::kHits;
  result->distance_to_plane = distance_to_plane;
  result->hit_point = hit_point;
  // The local hit point is unused since the keyboard delegate does the
  // local  hittesting for us.
  result->local_hit_point = gfx::PointF(0, 0);
}

void Keyboard::OnFloatAnimated(const float& value,
                               int target_property_id,
                               gfx::KeyframeModel* animation) {
  DCHECK(target_property_id == OPACITY);
  UiElement::OnFloatAnimated(value, target_property_id, animation);
  UpdateDelegateVisibility();
}

void Keyboard::OnHoverEnter(const gfx::PointF& position,
                            base::TimeTicks timestamp) {
  if (!delegate_)
    return;

  delegate_->OnHoverEnter(position);
}

void Keyboard::OnHoverLeave(base::TimeTicks) {
  if (!delegate_)
    return;

  delegate_->OnHoverLeave();
}

void Keyboard::OnHoverMove(const gfx::PointF& position,
                           base::TimeTicks timestamp) {
  if (!delegate_)
    return;

  delegate_->OnHoverMove(position);
}

void Keyboard::OnButtonDown(const gfx::PointF& position,
                            base::TimeTicks timestamp) {
  if (!delegate_)
    return;

  delegate_->OnButtonDown(position);
}

void Keyboard::OnButtonUp(const gfx::PointF& position,
                          base::TimeTicks timestamp) {
  if (!delegate_)
    return;

  delegate_->OnButtonUp(position);
}

void Keyboard::AdvanceKeyboardFrameIfNeeded() {
  // This is the keyboard's equivalent to OnBeginFrame(), but is separate
  // because it must run on every frame - not just if the keyboard is visible.
  if (!delegate_)
    return;

  delegate_->OnBeginFrame();
}

bool Keyboard::OnBeginFrame(const gfx::Transform& head_pose) {
  // We return false here because any visible changes to the keyboard, such as
  // hover effects and showing/hiding of the keyboard will be drawn by the
  // controller's dirtyness, so it's safe to assume no visual changes here.
  return false;
}

void Keyboard::OnUpdatedWorldSpaceTransform() {
  if (!delegate_)
    return;

  delegate_->SetTransform(world_space_transform());
}

void Keyboard::Render(UiElementRenderer* renderer,
                      const CameraModel& camera_model) const {
  if (!delegate_)
    return;

  renderer->DrawKeyboard(camera_model, delegate_);
}

void Keyboard::OnSetFocusable() {
  DCHECK(!focusable());
}

void Keyboard::UpdateDelegateVisibility() {
  if (!delegate_)
    return;

  if (opacity() > 0)
    delegate_->ShowKeyboard();
  else
    delegate_->HideKeyboard();
}

Keyboard::Renderer::Renderer() {}

Keyboard::Renderer::~Renderer() {}

void Keyboard::Renderer::Draw(const CameraModel& camera_model,
                              KeyboardDelegate* delegate) {
  delegate->Draw(camera_model);
}

}  // namespace vr
