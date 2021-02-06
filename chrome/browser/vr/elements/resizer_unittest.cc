// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/resizer.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "cc/test/geometry_test_utils.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace vr {

namespace {

class ResizerTest : public testing::Test {
 public:
  ResizerTest() {}
  ~ResizerTest() override {}

  void SetUp() override {
    auto resizer = std::make_unique<Resizer>();
    resizer->set_touch_position({0.5f, 0.5f});
    resizer->SetEnabled(true);
    resizer_ = resizer.get();
    scene_.AddUiElement(kRoot, std::move(resizer));
  }

  void Move(const gfx::PointF& from, const gfx::PointF& to) {
    resizer_->set_touch_position(from);
    resizer_->SetTouchingTouchpad(true);
    scene_.OnBeginFrame(MsToTicks(1), gfx::Transform());
    resizer_->set_touch_position(to);
    scene_.OnBeginFrame(MsToTicks(1), gfx::Transform());
    resizer_->SetTouchingTouchpad(false);
  }

  float ComputeScale() {
    gfx::Vector3dF v = {1.0f, 0.0f, 0.0f};
    static_cast<UiElement*>(resizer_)->LocalTransform().TransformVector(&v);
    return v.x();
  }

  void CheckScale(float scale) { EXPECT_FLOAT_EQ(scale, ComputeScale()); }

 protected:
  Resizer* resizer_ = nullptr;
  UiScene scene_;
};

}  // namespace

TEST_F(ResizerTest, HorizontalMove) {
  Move({0.5f, 0.5f}, {1.0f, 0.5f});
  CheckScale(1.0f);
}

TEST_F(ResizerTest, UpwardMove) {
  Move({0.5f, 0.5f}, {0.5f, 0.0f});
  CheckScale(kMinResizerScale);
}

TEST_F(ResizerTest, DownwardMove) {
  Move({0.5f, 0.5f}, {0.5f, 1.0f});
  CheckScale(kMaxResizerScale);
}

TEST_F(ResizerTest, AccumulatedMove) {
  Move({0.5f, 0.5f}, {0.5f, 0.75f});
  float scale = ComputeScale();
  EXPECT_LT(1.0f, scale);
  EXPECT_GT(kMaxResizerScale, scale);
  Move({0.5f, 0.5f}, {0.5f, 0.75f});
  CheckScale(kMaxResizerScale);
}

}  // namespace vr
