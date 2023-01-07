// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/spinner.h"

#include <vector>

#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "cc/test/test_skcanvas.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"

namespace vr {

namespace {

using testing::_;
static constexpr int kMaximumWidth = 512;
static constexpr float kArcEpsilon = 1e-3f;

class TestSpinner : public Spinner {
 public:
  explicit TestSpinner(int maximum_width) : Spinner(maximum_width) {}

  TestSpinner(const TestSpinner&) = delete;
  TestSpinner& operator=(const TestSpinner&) = delete;

  ~TestSpinner() override {}

  UiTexture* GetTexture() const override { return Spinner::GetTexture(); }
};

}  // namespace

void CheckArc(UiTexture* texture, float start_angle, float sweep_angle) {
  cc::MockCanvas canvas;
  EXPECT_CALL(canvas, onDrawArc(_, testing::FloatNear(start_angle, kArcEpsilon),
                                testing::FloatNear(sweep_angle, kArcEpsilon),
                                false, _));
  texture->Draw(&canvas, gfx::Size(kMaximumWidth, kMaximumWidth));
}

TEST(Spinner, Animation) {
  UiScene scene;
  scene.RunFirstFrameForTest();
  auto spinner_element = std::make_unique<TestSpinner>(kMaximumWidth);
  UiTexture* texture = spinner_element->GetTexture();
  scene.AddUiElement(kRoot, std::move(spinner_element));
  base::TimeTicks start_time = gfx::MsToTicks(1);
  scene.OnBeginFrame(start_time, kStartHeadPose);

  struct TestCase {
    float start_angle;
    float sweep_angle;
    base::TimeDelta delta;
  };
  std::vector<TestCase> test_cases = {
      {38.2652f, 63.8781f, base::Seconds(1.0 / 6.0)},
      {76.5305f, 209.402f, base::Seconds(2.0 / 6.0)},
      {114.796f, 259.029f, base::Seconds(3.0 / 6.0)},
      {153.061f, 270.0f, base::Seconds(4.0 / 6.0)},
      {255.206f, 206.121f, base::Seconds(5.0 / 6.0)},
      {438.994f, 60.5979f, base::Seconds(6.0 / 6.0)},
      {526.886f, 10.9706f, base::Seconds(7.0 / 6.0)},
      {576.122f, 0.0f, base::Seconds(8.0 / 6.0)},
  };

  for (const auto& test_case : test_cases) {
    scene.OnBeginFrame(gfx::MsToTicks(1) + test_case.delta, kStartHeadPose);
    CheckArc(texture, test_case.start_angle, test_case.sweep_angle);
  }
}

}  // namespace vr
