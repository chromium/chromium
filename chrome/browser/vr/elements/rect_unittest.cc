// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/rect.h"

#include <memory>

#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

TEST(Rect, SetColorCorrectly) {
  auto rect = std::make_unique<Rect>();

  EXPECT_NE(SK_ColorCYAN, rect->edge_color());
  EXPECT_NE(SK_ColorCYAN, rect->center_color());

  rect->SetColor(SK_ColorCYAN);
  EXPECT_EQ(SK_ColorCYAN, rect->edge_color());
  EXPECT_EQ(SK_ColorCYAN, rect->center_color());

  rect->SetEdgeColor(SK_ColorRED);
  rect->SetCenterColor(SK_ColorBLUE);

  EXPECT_EQ(SK_ColorRED, rect->edge_color());
  EXPECT_EQ(SK_ColorBLUE, rect->center_color());
}

TEST(Rect, AnimateColorCorrectly) {
  UiScene scene;
  scene.RunFirstFrameForTest();
  auto element = std::make_unique<Rect>();
  Rect* rect = element.get();
  scene.AddUiElement(kRoot, std::move(element));

  rect->SetEdgeColor(SK_ColorRED);
  rect->SetCenterColor(SK_ColorBLUE);

  rect->SetTransitionedProperties({BACKGROUND_COLOR, FOREGROUND_COLOR});
  rect->SetColor(SK_ColorBLACK);

  scene.OnBeginFrame(MsToTicks(1), kStartHeadPose);
  EXPECT_EQ(SK_ColorRED, rect->edge_color());
  EXPECT_EQ(SK_ColorBLUE, rect->center_color());

  scene.OnBeginFrame(MsToTicks(5000), kStartHeadPose);
  EXPECT_EQ(SK_ColorBLACK, rect->edge_color());
  EXPECT_EQ(SK_ColorBLACK, rect->center_color());
}

}  // namespace vr
