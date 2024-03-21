// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_SCALED_DEPTH_ADJUSTER_H_
#define CHROME_BROWSER_VR_ELEMENTS_SCALED_DEPTH_ADJUSTER_H_

#include <sstream>

#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

// A Scaler adjusts the depth of its descendents by applying a scale. This
// permits dimensions in the subtree to be expressed in DM directly. Its main
// contribution is a tailored local transform that accounts for adjustments made
// by other ScaledDepthAdjuster elements on its ancestor chain.
class VR_UI_EXPORT ScaledDepthAdjuster : public UiElement {
 public:
  explicit ScaledDepthAdjuster(float delta_z);

  ScaledDepthAdjuster(const ScaledDepthAdjuster&) = delete;
  ScaledDepthAdjuster& operator=(const ScaledDepthAdjuster&) = delete;

  ~ScaledDepthAdjuster() override;

 private:
  gfx::Transform LocalTransform() const override;
  gfx::Transform GetTargetLocalTransform() const override;
  bool OnBeginFrame(const gfx::Transform& head_pose) override;

#ifndef NDEBUG
  void DumpGeometry(std::ostringstream* os) const override;
#endif

  gfx::Transform transform_;

  // This is relative to its ancestor ScaledDepthAdjuster. For example, if we
  // have a single ScaledDepthAdjuster ancestor and it translates to depth 2.5,
  // if we use a delta_z_ of -0.1, this will cause our descendants to be
  // positioned at depth 2.4.
  float delta_z_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_SCALED_DEPTH_ADJUSTER_H_
