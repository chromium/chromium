// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/pose_util.h"

#include "ui/gfx/geometry/transform.h"

namespace vr {

// Provides the direction the head is looking towards as a 3x1 unit vector.
gfx::Vector3dF GetForwardVector(const gfx::Transform& head_pose) {
  // Same as multiplying the inverse of the rotation component of the matrix by
  // (0, 0, -1, 0).
  return gfx::Vector3dF(-head_pose.matrix().rc(2, 0),
                        -head_pose.matrix().rc(2, 1),
                        -head_pose.matrix().rc(2, 2));
}

gfx::Vector3dF GetUpVector(const gfx::Transform& head_pose) {
  return gfx::Vector3dF(head_pose.matrix().rc(1, 0),
                        head_pose.matrix().rc(1, 1),
                        head_pose.matrix().rc(1, 2));
}

}  // namespace vr
