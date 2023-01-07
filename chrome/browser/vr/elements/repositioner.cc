// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/repositioner.h"

#include "chrome/browser/vr/pose_util.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/quaternion.h"

namespace vr {

namespace {

constexpr float kDragThresholdDegrees = 3.0f;
constexpr float kHeadUpTransitionStartDegrees = 60.0f;
constexpr float kHeadUpTransitionEndDegrees = 30.0f;
constexpr gfx::Vector3dF kUp = {0, 1, 0};

gfx::Vector3dF GetEffectiveUpVector(const gfx::Vector3dF& forward,
                                    const gfx::Vector3dF& head_forward,
                                    const gfx::Vector3dF& right,
                                    const gfx::Vector3dF& head_up) {
  float degrees_from_up =
      std::min(gfx::AngleBetweenVectorsInDegrees(forward, kUp),
               gfx::AngleBetweenVectorsInDegrees(head_forward, kUp));
  if (degrees_from_up < kHeadUpTransitionEndDegrees)
    return head_up;

  if (degrees_from_up > kHeadUpTransitionStartDegrees)
    return kUp;

  gfx::Quaternion q(kUp, head_up);
  q = gfx::Quaternion().Slerp(
      q, (kHeadUpTransitionStartDegrees - degrees_from_up) /
             (kHeadUpTransitionStartDegrees - kHeadUpTransitionEndDegrees));

  gfx::Vector3dF interpolated_up = gfx::Transform(q).MapVector(kUp);
  return interpolated_up;
}

}  // namespace

Repositioner::Repositioner() = default;
Repositioner::~Repositioner() = default;

bool Repositioner::ShouldUpdateWorldSpaceTransform(
    bool parent_transform_changed) const {
  return true;
}

gfx::Transform Repositioner::LocalTransform() const {
  return transform_;
}

gfx::Transform Repositioner::GetTargetLocalTransform() const {
  return transform_;
}

void Repositioner::SetEnabled(bool enabled) {
  enabled_ = enabled;
  if (enabled) {
    initial_transform_ = transform_;
    initial_laser_direction_ = laser_direction_;
    has_moved_beyond_threshold_ = false;
  }
}

void Repositioner::Reset() {
  reset_yaw_ = true;
}

void Repositioner::UpdateTransform(const gfx::Transform& head_pose) {
  set_world_space_transform_dirty();

  gfx::Vector3dF head_up = vr::GetUpVector(head_pose);
  gfx::Vector3dF head_forward = vr::GetForwardVector(head_pose);

  if (reset_yaw_) {
    gfx::Vector3dF current_right =
        transform_.MapVector(gfx::Vector3dF(1, 0, 0));
    transform_.PostConcat(
        gfx::Transform(gfx::Quaternion(current_right, {1, 0, 0})));
  } else {
    transform_ = initial_transform_;
    transform_.PostConcat(gfx::Transform(
        gfx::Quaternion(initial_laser_direction_, laser_direction_)));
  }

  gfx::Vector3dF new_right = transform_.MapVector(gfx::Vector3dF(1, 0, 0));
  gfx::Vector3dF new_forward = transform_.MapVector(gfx::Vector3dF(0, 0, -1));

  // Finally we have to correct the roll. I.e., we want to rotate the content
  // window so that it's oriented "up" and we want to favor world up when we're
  // near the horizon. GetEffectiveUpVector handles producing the up vector we'd
  // like to respect.
  gfx::Vector3dF up =
      GetEffectiveUpVector(new_forward, head_forward, new_right, head_up);

  gfx::Vector3dF expected_right = gfx::CrossProduct(new_forward, up);
  gfx::Quaternion rotate_to_expected_right(new_right, expected_right);
  transform_.PostConcat(gfx::Transform(rotate_to_expected_right));
  if (gfx::AngleBetweenVectorsInDegrees(
          initial_laser_direction_, laser_direction_) > kDragThresholdDegrees) {
    has_moved_beyond_threshold_ = true;
  }

  // Potentially bake our current transform, to avoid situations where
  // |laser_direction_| and |initial_laser_direction_| are nearly 180 degrees
  // apart causing numeric ambiguity and strange artifacts.
  //
  // The reason we do this periodically and not every frame is because of the
  // pitch and yaw clamping. Effectively, these "throw away" deltas and this can
  // lead to a situation where the controller moves far past one of the clamp
  // boundaries (and has no effect), but as soon as it changes direction
  // (producing deltas in the other direction), the moves do have an effect.
  // This results in the user's controller being pointed in a very odd direction
  // to manipulate the window.
  if (gfx::AngleBetweenVectorsInDegrees(initial_laser_direction_,
                                        laser_direction_) > 90.0f) {
    initial_laser_direction_ = laser_direction_;
    initial_transform_ = transform_;
  }
}

bool Repositioner::OnBeginFrame(const gfx::Transform& head_pose) {
  if (enabled_ || reset_yaw_) {
    UpdateTransform(head_pose);
    reset_yaw_ = false;
    return true;
  }
  return false;
}

#ifndef NDEBUG
void Repositioner::DumpGeometry(std::ostringstream* os) const {
  gfx::Transform t = world_space_transform();
  gfx::Vector3dF forward = t.MapVector(gfx::Vector3dF(0, 0, -1));
  // Decompose the rotation to world x axis followed by world y axis
  float x_rotation = std::asin(forward.y() / forward.Length());
  gfx::Vector3dF projected_forward = {forward.x(), 0, forward.z()};
  float y_rotation = std::acos(gfx::DotProduct(projected_forward, {0, 0, -1}) /
                               projected_forward.Length());
  if (projected_forward.x() > 0.f)
    y_rotation *= -1;
  *os << "rx(" << gfx::RadToDeg(x_rotation) << ") "
      << "ry(" << gfx::RadToDeg(y_rotation) << ") ";
}
#endif

}  // namespace vr
