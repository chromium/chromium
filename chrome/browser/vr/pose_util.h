// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_POSE_UTIL_H_
#define CHROME_BROWSER_VR_POSE_UTIL_H_

#include "chrome/browser/vr/vr_base_export.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace gfx {
class Transform;
}

namespace vr {

// Provides the direction the head is looking towards as a 3x1 unit vector.
VR_BASE_EXPORT gfx::Vector3dF GetForwardVector(const gfx::Transform& head_pose);

}  // namespace vr

#endif  //  CHROME_BROWSER_VR_POSE_UTIL_H_
