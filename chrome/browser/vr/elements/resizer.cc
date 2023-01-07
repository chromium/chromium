// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/resizer.h"

#include "base/cxx17_backports.h"
#include "base/numerics/math_constants.h"
#include "chrome/browser/vr/pose_util.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quaternion.h"

namespace vr {

namespace {

// Fraction here is in reference to "time fraction" terminology in web
// animations.
constexpr float kDefaultFraction = 0.5f;

static_assert(0.5f * (kMinResizerScale + kMaxResizerScale) == 1.0f,
              "1.0 must be the midpoint of the min and max scale");

}  // namespace

Resizer::Resizer() : t_(kDefaultFraction), initial_t_(kDefaultFraction) {
  set_bounds_contain_children(true);
}

Resizer::~Resizer() = default;

gfx::Transform Resizer::LocalTransform() const {
  return transform_;
}

gfx::Transform Resizer::GetTargetLocalTransform() const {
  return transform_;
}

void Resizer::SetTouchingTouchpad(bool touching) {
  if (enabled_ && touching) {
    initial_t_ = t_;
    initial_touchpad_position_ = touchpad_position_;
  }
}

void Resizer::SetEnabled(bool enabled) {
  enabled_ = enabled;
  if (enabled) {
    initial_t_ = t_;
    initial_touchpad_position_ = touchpad_position_;
  }
}

void Resizer::Reset() {
  transform_.MakeIdentity();
  set_world_space_transform_dirty();
  t_ = initial_t_ = kDefaultFraction;
}

void Resizer::UpdateTransform(const gfx::Transform& head_pose) {
  float delta = touchpad_position_.y() - initial_touchpad_position_.y();
  t_ = base::clamp(initial_t_ + delta, 0.0f, 1.0f);
  float scale =
      gfx::Tween::FloatValueBetween(t_, kMinResizerScale, kMaxResizerScale);
  transform_.MakeIdentity();
  transform_.Scale(scale, scale);
  set_world_space_transform_dirty();
}

bool Resizer::OnBeginFrame(const gfx::Transform& head_pose) {
  if (enabled_) {
    UpdateTransform(head_pose);
    return true;
  }
  return false;
}

#ifndef NDEBUG
void Resizer::DumpGeometry(std::ostringstream* os) const {
  gfx::Vector3dF right = LocalTransform().MapVector(gfx::Vector3dF(1, 0, 0));
  *os << "s(" << right.x() << ") ";
}
#endif

bool Resizer::ShouldUpdateWorldSpaceTransform(
    bool parent_transform_changed) const {
  return true;
}

}  // namespace vr
