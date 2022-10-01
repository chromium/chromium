// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_input_manager.h"

#include <algorithm>

#include "base/containers/adapters.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/model/reticle_model.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/render_info.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"

namespace vr {

namespace {

constexpr gfx::PointF kInvalidTargetPoint =
    gfx::PointF(std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max());

constexpr float kControllerFocusThresholdSeconds = 1.0f;

bool IsCentroidInViewport(const gfx::Transform& view_proj_matrix,
                          const gfx::Transform& world_matrix) {
  if (world_matrix.IsIdentity()) {
    // Uninitialized matrices are considered out of the viewport.
    return false;
  }
  gfx::Transform m = view_proj_matrix * world_matrix;
  gfx::Point3F o = m.MapPoint(gfx::Point3F());
  return o.x() > -1.0f && o.x() < 1.0f && o.y() > -1.0f && o.y() < 1.0f;
}

bool IsScrollOrFling(const InputEventList& list) {
  if (list.empty()) {
    return false;
  }
  // We assume that we only need to consider the first gesture in the list.
  auto type = list.front()->type();
  return InputEvent::IsScrollEventType(type) ||
         type == InputEvent::kFlingCancel;
}

void HitTestElements(UiScene* scene,
                     ReticleModel* reticle_model,
                     HitTestRequest* request) {
  std::vector<const UiElement*> elements = scene->GetElementsToHitTest();
  std::vector<const UiElement*> sorted =
      UiRenderer::GetElementsInDrawOrder(elements);

  for (const auto* element : base::Reversed(sorted)) {
    DCHECK(element->IsHitTestable());

    HitTestResult result;
    element->HitTest(*request, &result);
    if (result.type != HitTestResult::Type::kHits) {
      continue;
    }

    reticle_model->target_element_id = element->id();
    reticle_model->target_local_point = result.local_hit_point;
    reticle_model->target_point = result.hit_point;
    reticle_model->cursor_type = element->cursor_type();
    break;
  }
}

}  // namespace

UiInputManager::UiInputManager(UiScene* scene) : scene_(scene) {}

UiInputManager::~UiInputManager() {}

void UiInputManager::HandleInput(base::TimeTicks current_time,
                                 const RenderInfo& render_info,
                                 const ControllerModel& controller_model,
                                 ReticleModel* reticle_model,
                                 InputEventList* input_event_list) {
  UpdateControllerFocusState(current_time, render_info, controller_model);
  reticle_model->target_element_id = 0;
  reticle_model->target_local_point = kInvalidTargetPoint;
  UiElement* target_element =
      GetTargetElement(controller_model, reticle_model, *input_event_list);

  auto element_local_point = reticle_model->target_local_point;
  if (input_capture_element_id_)
    element_local_point =
        GetCapturedElementHitPoint(reticle_model->target_point);

  // Sending end and cancel events.
  SendFlingCancel(input_event_list, element_local_point);
  SendScrollEnd(input_event_list, element_local_point,
                controller_model.touchpad_button_state);
  SendButtonUp(element_local_point, controller_model.touchpad_button_state,
               controller_model.last_button_timestamp);
  SendHoverLeave(target_element, controller_model.last_orientation_timestamp);

  // Sending update events.
  if (in_scroll_) {
    SendScrollUpdate(input_event_list, element_local_point);
  } else if (in_click_) {
    SendTouchMove(element_local_point,
                  controller_model.last_orientation_timestamp);
  } else {
    SendHoverMove(target_element, reticle_model->target_local_point,
                  controller_model.last_orientation_timestamp);
  }

  // Sending begin events.
  SendHoverEnter(target_element, reticle_model->target_local_point,
                 controller_model.last_orientation_timestamp);
  SendScrollBegin(target_element, input_event_list, element_local_point);
  SendButtonDown(target_element, reticle_model->target_local_point,
                 controller_model.touchpad_button_state,
                 controller_model.last_button_timestamp);

  previous_button_state_ = controller_model.touchpad_button_state;
}

void UiInputManager::OnPause() {
  if (hover_target_id_) {
    UiElement* prev_hovered = scene_->GetUiElementById(hover_target_id_);
    if (prev_hovered)
      prev_hovered->OnHoverLeave(base::TimeTicks::Now());
    hover_target_id_ = 0;
  }
}

void UiInputManager::SendFlingCancel(InputEventList* input_event_list,
                                     const gfx::PointF& target_point) {
  if (!fling_target_id_) {
    return;
  }
  if (input_event_list->empty() ||
      (input_event_list->front()->type() != InputEvent::kFlingCancel)) {
    return;
  }

  // Scrolling currently only supported on content window.
  UiElement* element = scene_->GetUiElementById(fling_target_id_);
  if (element) {
    DCHECK(element->scrollable());
    element->OnFlingCancel(std::move(input_event_list->front()), target_point);
  }
  input_event_list->erase(input_event_list->begin());
  fling_target_id_ = 0;
}

void UiInputManager::SendScrollEnd(InputEventList* input_event_list,
                                   const gfx::PointF& target_point,
                                   ControllerModel::ButtonState button_state) {
  if (!in_scroll_) {
    return;
  }
  DCHECK_GT(input_capture_element_id_, 0);
  UiElement* element = scene_->GetUiElementById(input_capture_element_id_);

  if (previous_button_state_ != button_state &&
      button_state == ControllerModel::ButtonState::kDown) {
    DCHECK_GT(input_event_list->size(), 0LU);
    DCHECK_EQ(input_event_list->front()->type(), InputEvent::kScrollEnd);
  }
  DCHECK(!element || element->scrollable());
  if (input_event_list->empty() ||
      input_event_list->front()->type() != InputEvent::kScrollEnd) {
    return;
  }
  DCHECK_LE(input_event_list->size(), 1LU);
  fling_target_id_ = input_capture_element_id_;
  element->OnScrollEnd(std::move(input_event_list->front()), target_point);
  input_event_list->erase(input_event_list->begin());
  input_capture_element_id_ = 0;
  in_scroll_ = false;
}

void UiInputManager::SendScrollBegin(UiElement* target,
                                     InputEventList* input_event_list,
                                     const gfx::PointF& target_point) {
  if (in_scroll_ || !target || !target->scrollable())
    return;

  if (input_event_list->empty() ||
      input_event_list->front()->type() != InputEvent::kScrollBegin) {
    return;
  }
  input_capture_element_id_ = target->id();
  in_scroll_ = true;
  target->OnScrollBegin(std::move(input_event_list->front()), target_point);
  input_event_list->erase(input_event_list->begin());
}

void UiInputManager::SendScrollUpdate(InputEventList* input_event_list,
                                      const gfx::PointF& target_point) {
  DCHECK(input_capture_element_id_);
  if (input_event_list->empty() ||
      (input_event_list->front()->type() != InputEvent::kScrollUpdate)) {
    return;
  }
  // Scrolling currently only supported on content window.
  UiElement* element = scene_->GetUiElementById(input_capture_element_id_);
  if (element) {
    DCHECK(element->scrollable());
    element->OnScrollUpdate(std::move(input_event_list->front()), target_point);
  }
  input_event_list->erase(input_event_list->begin());
}

void UiInputManager::SendHoverLeave(UiElement* current_target,
                                    base::TimeTicks timestamp) {
  if (hover_target_id_ &&
      (!current_target || current_target->id() != hover_target_id_)) {
    UiElement* prev_hovered = scene_->GetUiElementById(hover_target_id_);
    if (prev_hovered)
      prev_hovered->OnHoverLeave(timestamp);
    hover_target_id_ = 0;
  }
}

void UiInputManager::SendHoverEnter(UiElement* target,
                                    const gfx::PointF& target_point,
                                    base::TimeTicks timestamp) {
  if (!target || target->id() == hover_target_id_)
    return;
  if ((in_click_ || in_scroll_) && target->id() != input_capture_element_id_)
    return;
  target->OnHoverEnter(target_point, timestamp);
  hover_target_id_ = target->id();
}

void UiInputManager::SendHoverMove(UiElement* target,
                                   const gfx::PointF& target_point,
                                   base::TimeTicks timestamp) {
  if (target && target->id() == hover_target_id_)
    target->OnHoverMove(target_point, timestamp);
}

void UiInputManager::SendButtonUp(const gfx::PointF& target_point,
                                  ControllerModel::ButtonState button_state,
                                  base::TimeTicks timestamp) {
  if (!in_click_ || previous_button_state_ == button_state ||
      button_state != ControllerModel::ButtonState::kUp) {
    return;
  }
  in_click_ = false;
  if (!input_capture_element_id_)
    return;
  UiElement* element = scene_->GetUiElementById(input_capture_element_id_);
  if (element) {
    element->OnButtonUp(target_point, timestamp);
    // Clicking outside of the focused element causes it to lose focus.
    if (element->id() != focused_element_id_ && element->focusable())
      UnfocusFocusedElement();
  }

  input_capture_element_id_ = 0;
}

void UiInputManager::SendButtonDown(UiElement* target,
                                    const gfx::PointF& target_point,
                                    ControllerModel::ButtonState button_state,
                                    base::TimeTicks timestamp) {
  if (previous_button_state_ == button_state ||
      button_state != ControllerModel::ButtonState::kDown) {
    return;
  }
  in_click_ = true;
  if (target) {
    target->OnButtonDown(target_point, timestamp);
    input_capture_element_id_ = target->id();
  } else {
    input_capture_element_id_ = 0;
  }
}

void UiInputManager::SendTouchMove(const gfx::PointF& target_point,
                                   base::TimeTicks timestamp) {
  if (!input_capture_element_id_)
    return;
  UiElement* element = scene_->GetUiElementById(input_capture_element_id_);
  if (element)
    element->OnTouchMove(target_point, timestamp);
}

UiElement* UiInputManager::GetTargetElement(
    const ControllerModel& controller_model,
    ReticleModel* reticle_model,
    const InputEventList& input_event_list) const {
  // If we place the reticle based on elements intersecting the controller beam,
  // we can end up with the reticle hiding behind elements, or jumping laterally
  // in the field of view. This is physically correct, but hard to use. For
  // usability, do the following instead:
  //
  // - Project the controller laser onto a distance-limiting sphere.
  // - Create a vector between the eyes and the point on the sphere.
  // - If any UI elements intersect this vector, and are within the bounding
  //   sphere, choose the element that is last in scene draw order (which is
  //   typically the closest to the eye).

  // Compute the distance from the eyes to the distance limiting sphere. Note
  // that the sphere is centered at the controller, rather than the eye, for
  // simplicity.
  float distance = scene_->background_distance();
  reticle_model->target_point =
      controller_model.laser_origin +
      gfx::ScaleVector3d(controller_model.laser_direction, distance);

  // Determine which UI element (if any) intersects the line between the ray
  // origin and the controller target position. The ray origin will typically be
  // the world origin (roughly the eye) to make targeting with a real controller
  // more intuitive. For testing, however, we occasionally hit test along the
  // laser precisely since this geometric accuracy is important and we are not
  // dealing with a physical controller.
  gfx::Point3F ray_origin;
  if (hit_test_strategy_ == HitTestStrategy::PROJECT_TO_LASER_ORIGIN_FOR_TEST) {
    ray_origin = controller_model.laser_origin;
  }

  float distance_limit = (reticle_model->target_point - ray_origin).Length();

  HitTestRequest request;
  request.ray_origin = ray_origin;
  request.ray_target = reticle_model->target_point;
  request.max_distance_to_plane = distance_limit;
  HitTestElements(scene_, reticle_model, &request);

  // TODO(vollick): support multiple dispatch. We may want to, for example,
  // dispatch raw events to several elements we hit (imagine nested horizontal
  // and vertical scrollers). Currently, we only dispatch to one "winner".
  UiElement* target_element =
      scene_->GetUiElementById(reticle_model->target_element_id);
  if (target_element) {
    if (IsScrollOrFling(input_event_list) && !input_capture_element_id_) {
      DCHECK(!in_scroll_ && !in_click_);
      UiElement* ancestor = target_element;
      while (!ancestor->scrollable() && ancestor->parent())
        ancestor = ancestor->parent();
      if (ancestor->scrollable())
        target_element = ancestor;
    }
  }
  return target_element;
}

void UiInputManager::UpdateControllerFocusState(
    base::TimeTicks current_time,
    const RenderInfo& render_info,
    const ControllerModel& controller_model) {
  if (!IsCentroidInViewport(render_info.left_eye_model.view_proj_matrix,
                            controller_model.transform) &&
      !IsCentroidInViewport(render_info.right_eye_model.view_proj_matrix,
                            controller_model.transform)) {
    last_controller_outside_viewport_time_ = current_time;
    controller_resting_in_viewport_ = false;
    return;
  }

  controller_resting_in_viewport_ =
      (current_time - last_controller_outside_viewport_time_).InSecondsF() >
      kControllerFocusThresholdSeconds;
}

void UiInputManager::UnfocusFocusedElement() {
  if (!focused_element_id_)
    return;

  UiElement* focused = scene_->GetUiElementById(focused_element_id_);
  if (focused && focused->focusable()) {
    focused->OnFocusChanged(false);
  }
  focused_element_id_ = 0;
}

void UiInputManager::RequestFocus(int element_id) {
  if (element_id == focused_element_id_)
    return;

  UnfocusFocusedElement();

  UiElement* focused = scene_->GetUiElementById(element_id);
  if (!focused || !focused->focusable())
    return;

  focused_element_id_ = element_id;
  focused->OnFocusChanged(true);
}

void UiInputManager::RequestUnfocus(int element_id) {
  if (element_id != focused_element_id_)
    return;

  UnfocusFocusedElement();
}

void UiInputManager::OnInputEdited(const EditedText& info) {
  UiElement* focused = scene_->GetUiElementById(focused_element_id_);
  if (!focused)
    return;
  DCHECK(focused->focusable());
  focused->OnInputEdited(info);
}

void UiInputManager::OnInputCommitted(const EditedText& info) {
  UiElement* focused = scene_->GetUiElementById(focused_element_id_);
  if (!focused || !focused->focusable())
    return;
  DCHECK(focused->focusable());
  focused->OnInputCommitted(info);
}

void UiInputManager::OnKeyboardHidden() {
  UnfocusFocusedElement();
}

bool UiInputManager::ControllerRestingInViewport() const {
  return controller_resting_in_viewport_;
}

gfx::PointF UiInputManager::GetCapturedElementHitPoint(
    const gfx::Point3F& target_point) const {
  UiElement* captured_element =
      scene_->GetUiElementById(input_capture_element_id_);
  if (captured_element && captured_element->IsVisible()) {
    HitTestRequest request;
    request.ray_target = target_point;
    request.max_distance_to_plane = 2 * scene_->background_distance();
    HitTestResult result;
    captured_element->HitTest(request, &result);
    if (result.type != HitTestResult::Type::kNone)
      return result.local_hit_point;
  }
  return kInvalidTargetPoint;
}

}  // namespace vr
