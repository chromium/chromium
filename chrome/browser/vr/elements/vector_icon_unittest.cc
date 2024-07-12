// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/vector_icon.h"

#include <memory>

#include "base/test/gtest_util.h"
#include "cc/test/test_skcanvas.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "components/vector_icons/vector_icons.h"

namespace vr {

namespace {

using testing::_;
using testing::InSequence;

static constexpr int kMaximumWidth = 512;

class TestVectorIcon : public VectorIcon {
 public:
  explicit TestVectorIcon(int maximum_width) : VectorIcon(maximum_width) {}

  TestVectorIcon(const TestVectorIcon&) = delete;
  TestVectorIcon& operator=(const TestVectorIcon&) = delete;

  ~TestVectorIcon() override {}

  UiTexture* GetTexture() const override { return VectorIcon::GetTexture(); }
};

}  // namespace

// Tests get/set color API.
TEST(VectorIcon, SetColor) {
  auto icon = std::make_unique<TestVectorIcon>(kMaximumWidth);
  icon->SetColor(SK_ColorRED);
  EXPECT_EQ(SK_ColorRED, icon->GetColor());
  icon->SetColor(SK_ColorBLUE);
  EXPECT_EQ(SK_ColorBLUE, icon->GetColor());
}

TEST(VectorIcon, NilIcon) {
  UiScene scene;
  auto icon = std::make_unique<TestVectorIcon>(kMaximumWidth);
  UiTexture* texture = icon->GetTexture();
  scene.AddUiElement(kRoot, std::move(icon));
  base::TimeTicks start_time = gfx::MsToTicks(1);
  scene.OnBeginFrame(start_time, kStartHeadPose);

  InSequence scope;
  cc::MockCanvas canvas;

  // With a nil icon, the VectorIcon should early out of drawing.
  EXPECT_CALL(canvas, willSave()).Times(0);
  EXPECT_CALL(canvas, didScale(_, _)).Times(0);
  EXPECT_CALL(canvas, onDrawPath(_, _)).Times(0);
  EXPECT_CALL(canvas, willRestore()).Times(0);

  texture->Draw(&canvas, gfx::Size(kMaximumWidth, kMaximumWidth));
}

TEST(VectorIcon, SmokeTest) {
  UiScene scene;
  auto icon = std::make_unique<TestVectorIcon>(kMaximumWidth);
  icon->SetIcon(vector_icons::kCloseRoundedIcon);
  UiTexture* texture = icon->GetTexture();
  scene.AddUiElement(kRoot, std::move(icon));
  base::TimeTicks start_time = gfx::MsToTicks(1);
  scene.OnBeginFrame(start_time, kStartHeadPose);

  InSequence scope;
  cc::MockCanvas canvas;

  // The drawing of vector icons is bookended with a scoped save layer.
  EXPECT_CALL(canvas, willSave());

  // This matrix is concatenated to apply to the vector icon.
  EXPECT_CALL(canvas, didScale(_, _));

  // This is the call to draw the path comprising the vector icon.
  EXPECT_CALL(canvas, onDrawPath(_, _));

  // The drawing of vector icons is bookended with a scoped save layer.
  EXPECT_CALL(canvas, willRestore());

  texture->Draw(&canvas, gfx::Size(kMaximumWidth, kMaximumWidth));
}

}  // namespace vr
