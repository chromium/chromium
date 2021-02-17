// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/shadow.h"

#include <memory>

#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(Shadow, ShadowPaddingGrows) {
  UiScene scene;
  auto rect = std::make_unique<Rect>();
  auto* rect_ptr = rect.get();
  rect->SetSize(2.0, 2.0);

  auto shadow = std::make_unique<Shadow>();
  auto* shadow_ptr = shadow.get();
  shadow->AddChild(std::move(rect));
  scene.AddUiElement(kRoot, std::move(shadow));

  scene.OnBeginFrame(MsToTicks(0), kStartHeadPose);
  float old_left_padding = shadow_ptr->left_padding();
  float old_top_padding = shadow_ptr->top_padding();
  EXPECT_LE(0.0f, old_left_padding);
  EXPECT_LE(0.0f, old_top_padding);

  rect_ptr->SetTranslate(0, 0, 0.15);
  scene.OnBeginFrame(MsToTicks(0), kStartHeadPose);
  float new_left_padding = shadow_ptr->left_padding();
  float new_top_padding = shadow_ptr->top_padding();
  EXPECT_LE(old_left_padding, new_left_padding);
  EXPECT_LE(old_top_padding, new_top_padding);
}

}  // namespace vr
