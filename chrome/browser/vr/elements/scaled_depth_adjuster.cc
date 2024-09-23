// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/scaled_depth_adjuster.h"

namespace vr {

ScaledDepthAdjuster::ScaledDepthAdjuster(float delta_z) : delta_z_(delta_z) {
  SetType(kTypeScaledDepthAdjuster);
  set_contributes_to_parent_bounds(false);
}

ScaledDepthAdjuster::~ScaledDepthAdjuster() = default;

gfx::Transform ScaledDepthAdjuster::LocalTransform() const {
  return transform_;
}

gfx::Transform ScaledDepthAdjuster::GetTargetLocalTransform() const {
  return transform_;
}

bool ScaledDepthAdjuster::OnBeginFrame(const gfx::Transform& head_pose) {
  // NB: we compute our local transform only once in the first call to
  // OnBeginFrame that occurs after this element's construction. This permits
  // ScaledDepthAdjuster elements to be added to subtrees that are later added
  // to the scene graph as opposed to requiring that the full hierarchy be
  // available at the time of construction, simplifying scene graph building.
  if (!transform_.IsIdentity())
    return false;

  gfx::Transform inherited;
  for (UiElement* anc = parent(); anc; anc = anc->parent()) {
    if (anc->type() == kTypeScaledDepthAdjuster) {
      inherited.PostConcat(anc->LocalTransform());
    }
  }
  transform_ = inherited.GetCheckedInverse();
  gfx::Point3F o = inherited.MapPoint(gfx::Point3F());
  float z = -o.z() + delta_z_;
  transform_.Scale3d(z, z, z);
  transform_.Translate3d(0, 0, -1);
  set_world_space_transform_dirty();
  return true;
}

#ifndef NDEBUG
void ScaledDepthAdjuster::DumpGeometry(std::ostringstream* os) const {
  *os << "tz(" << delta_z_ << ") ";
}
#endif

}  // namespace vr
