// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/overlay_controls_fade_animation.h"

#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/compositor/layer.h"
#include "ui/views/view.h"

namespace {

using OverlayControlsFadeAnimationTest = ChromeViewsTestBase;

TEST_F(OverlayControlsFadeAnimationTest, AnimatesViewLayerOpacity) {
  auto view = std::make_unique<views::View>();
  view->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  ASSERT_EQ(1.0, view->layer()->opacity());

  // Create an animation to hide the view.
  auto animation = std::make_unique<OverlayControlsFadeAnimation>(
      *view, OverlayControlsFadeAnimation::Type::kToHidden);
  animation->Start();

  // Partway through the animation, the opacity should be between 1 and 0.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  const float middle_hide_opacity = view->layer()->opacity();
  EXPECT_LT(middle_hide_opacity, 1.0);
  EXPECT_GT(middle_hide_opacity, 0.0);

  // By the end of the animation, the opacity should be 0.
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(view->layer()->opacity(), 0.0);

  // Create an animation to show the view.
  animation = std::make_unique<OverlayControlsFadeAnimation>(
      *view, OverlayControlsFadeAnimation::Type::kToShown);
  animation->Start();

  // Partway through the animation, the opacity should be between 0 and 1.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  const float middle_show_opacity = view->layer()->opacity();
  EXPECT_GT(middle_show_opacity, 0.0);
  EXPECT_LT(middle_show_opacity, 1.0);

  // By the end of the animation, the opacity should be 1.
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(view->layer()->opacity(), 1.0);
}

}  // namespace
