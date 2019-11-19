// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/viewport_aware_root.h"

#include <cmath>

#include "chrome/browser/vr/pose_util.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace vr {

namespace {

bool ElementHasVisibleChildren(UiElement* element) {
  for (auto& child : element->children()) {
    // Note that we do NOT use IsVisible here. IsVisible takes inherited opacity
    // into consideration. However, the parent element (the viewport aware root
    // element) might be invisible at first due to opacity animation, which
    // makes all children becomes invisible even if it has children that become
    // visible immediately when animation starts.
    if (child->opacity() > 0.f) {
      if (child->draw_phase() != kPhaseNone)
        return true;
      if (ElementHasVisibleChildren(child.get()))
        return true;
    }
  }
  return false;
}

}  // namespace

// static
const float ViewportAwareRoot::kViewportRotationTriggerDegrees = 55.0f;

ViewportAwareRoot::ViewportAwareRoot() {
  SetTransitionedProperties({OPACITY});
}

ViewportAwareRoot::~ViewportAwareRoot() = default;

bool ViewportAwareRoot::OnBeginFrame(const gfx::Transform& head_pose) {
  // head_pose is head_from_world.  Invert to get world_from_head.
  gfx::Vector3dF look_at = vr::GetForwardVector(head_pose);
  bool changed = AdjustRotationForHeadPose(look_at);
  if (recenter_on_rotate_) {
    gfx::Transform world_from_head;
    bool invertable = head_pose.GetInverse(&world_from_head);
    DCHECK(invertable);  // Pose data has been validated already.
    gfx::Point3F head_pos_in_world_space{0.f, 0.f, 0.f};
    world_from_head.TransformPoint(&head_pos_in_world_space);
    changed = AdjustTranslation(head_pos_in_world_space.x(),
                                head_pos_in_world_space.z(), changed);
  }
  return changed;
}

bool ViewportAwareRoot::AdjustTranslation(float head_in_world_x,
                                          float head_in_world_z,
                                          bool did_rotate) {
  gfx::Point3F center_point_in_world{0.f, 0.f, 0.f};
  LocalTransform().TransformPoint(&center_point_in_world);
  gfx::Vector2dF offset = {head_in_world_x - center_point_in_world.x(),
                           head_in_world_z - center_point_in_world.z()};

  const float kMinTranslationLength =
      1.2f;  // If you move 1.2m, we'll recenter.
  if (did_rotate || offset.Length() > kMinTranslationLength) {
    SetTranslate(head_in_world_x, center_point_in_world.y(), head_in_world_z);
    return true;
  }
  return false;
}

bool ViewportAwareRoot::AdjustRotationForHeadPose(
    const gfx::Vector3dF& look_at) {
  DCHECK(!look_at.IsZero());
  bool rotated = false;

  bool has_visible_children = HasVisibleChildren();
  if (has_visible_children && !children_visible_) {
    Reset();
    rotated = true;
  }
  children_visible_ = has_visible_children;
  if (!children_visible_)
    return false;

  gfx::Vector3dF rotated_center_vector{0.f, 0.f, -1.0f};
  LocalTransform().TransformVector(&rotated_center_vector);
  gfx::Vector3dF top_projected_look_at{look_at.x(), 0.f, look_at.z()};
  float degrees = gfx::ClockwiseAngleBetweenVectorsInDegrees(
      top_projected_look_at, rotated_center_vector, {0.f, 1.0f, 0.f});
  if (degrees <= kViewportRotationTriggerDegrees ||
      degrees >= 360.0f - kViewportRotationTriggerDegrees) {
    return rotated;
  }
  viewport_aware_total_rotation_ += degrees;
  viewport_aware_total_rotation_ = fmod(viewport_aware_total_rotation_, 360.0f);
  SetRotate(0.f, 1.f, 0.f, gfx::DegToRad(viewport_aware_total_rotation_));

  // Immediately hide the element.
  SetVisibleImmediately(false);

  // Fade it back in.
  SetVisible(true);
  return true;
}

void ViewportAwareRoot::Reset() {
  viewport_aware_total_rotation_ = 0.f;
  x_center = 0;
  z_center = 0;
  SetRotate(0.f, 1.f, 0.f, gfx::DegToRad(viewport_aware_total_rotation_));
}

bool ViewportAwareRoot::HasVisibleChildren() {
  return ElementHasVisibleChildren(this);
}

}  // namespace vr
