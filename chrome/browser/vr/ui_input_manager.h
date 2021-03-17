// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_INPUT_MANAGER_H_
#define CHROME_BROWSER_VR_UI_INPUT_MANAGER_H_

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform.h"

namespace vr {

class UiScene;
class UiElement;
class InputEvent;
struct RenderInfo;
struct ReticleModel;
struct EditedText;

using InputEventList = std::vector<std::unique_ptr<InputEvent>>;

// Based on controller input finds the hit UI element and determines the
// interaction with UI elements and the web contents.
class VR_UI_EXPORT UiInputManager {
 public:
  // When testing, it can be useful to hit test directly along the laser.
  // Updating the strategy permits this behavior, but it should not be used in
  // production. In production, we hit test along a ray that extends from the
  // world origin to a point far along the laser.
  enum HitTestStrategy {
    PROJECT_TO_WORLD_ORIGIN,
    PROJECT_TO_LASER_ORIGIN_FOR_TEST,
  };

  explicit UiInputManager(UiScene* scene);
  virtual ~UiInputManager();
  void HandleInput(base::TimeTicks current_time,
                   const RenderInfo& render_info,
                   const ControllerModel& controller_model,
                   ReticleModel* reticle_model,
                   InputEventList* input_event_list);

  void OnPause();

  // Text input related.
  void RequestFocus(int element_id);
  void RequestUnfocus(int element_id);
  void OnInputEdited(const EditedText& info);
  void OnInputCommitted(const EditedText& info);
  void OnKeyboardHidden();

  virtual bool ControllerRestingInViewport() const;

  void set_hit_test_strategy(HitTestStrategy strategy) {
    hit_test_strategy_ = strategy;
  }

 private:
  void SendFlingCancel(InputEventList* input_event_list,
                       const gfx::PointF& target_point);
  void SendScrollEnd(InputEventList* input_event_list,
                     const gfx::PointF& target_point,
                     ControllerModel::ButtonState button_state);
  void SendScrollBegin(UiElement* target,
                       InputEventList* input_event_list,
                       const gfx::PointF& target_point);
  void SendScrollUpdate(InputEventList* input_event_list,
                        const gfx::PointF& target_point);

  void SendHoverLeave(UiElement* current_target, base::TimeTicks timestamp);
  void SendHoverEnter(UiElement* target,
                      const gfx::PointF& target_point,
                      base::TimeTicks timestamp);
  void SendHoverMove(UiElement* target,
                     const gfx::PointF& target_point,
                     base::TimeTicks timestamp);

  void SendButtonUp(const gfx::PointF& target_point,
                    ControllerModel::ButtonState button_state,
                    base::TimeTicks timestamp);
  void SendButtonDown(UiElement* target,
                      const gfx::PointF& target_point,
                      ControllerModel::ButtonState button_state,
                      base::TimeTicks timestamp);
  void SendTouchMove(const gfx::PointF& target_point,
                     base::TimeTicks timestamp);

  UiElement* GetTargetElement(const ControllerModel& controller_model,
                              ReticleModel* reticle_model,
                              const InputEventList& input_event_list) const;
  void UpdateControllerFocusState(base::TimeTicks current_time,
                                  const RenderInfo& render_info,
                                  const ControllerModel& controller_model);

  void UnfocusFocusedElement();

  gfx::PointF GetCapturedElementHitPoint(
      const gfx::Point3F& target_point) const;

  UiScene* scene_;
  int hover_target_id_ = 0;
  // TODO(mthiesse): We shouldn't have a fling target. Elements should fling
  // independently and we should only cancel flings on the relevant element
  // when we do cancel flings.
  int fling_target_id_ = 0;
  int input_capture_element_id_ = 0;
  int focused_element_id_ = 0;
  bool in_click_ = false;
  bool in_scroll_ = false;

  HitTestStrategy hit_test_strategy_ = HitTestStrategy::PROJECT_TO_WORLD_ORIGIN;

  ControllerModel::ButtonState previous_button_state_ =
      ControllerModel::ButtonState::kUp;

  base::TimeTicks last_controller_outside_viewport_time_;
  bool controller_resting_in_viewport_ = false;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_INPUT_MANAGER_H_
