// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_RENDER_INFO_H_
#define CHROME_BROWSER_VR_RENDER_INFO_H_

#include "chrome/browser/vr/model/camera_model.h"
#include "chrome/browser/vr/vr_base_export.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

// Provides information for rendering such as the viewport and view/projection
// matrix.
struct VR_BASE_EXPORT RenderInfo {
  gfx::Transform head_pose;
  CameraModel left_eye_model;
  CameraModel right_eye_model;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_RENDER_INFO_H_
