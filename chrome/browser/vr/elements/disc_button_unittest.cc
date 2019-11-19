// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/disc_button.h"

#include "cc/animation/transform_operation.h"
#include "cc/animation/transform_operations.h"
#include "cc/test/geometry_test_utils.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"

namespace vr {

TEST(DiscButton, HoverTest) {
  DiscButton button(base::RepeatingCallback<void()>(), vector_icons::kMicIcon,
                    nullptr);
  button.SetSize(1.0f, 1.0f);
  button.set_hover_offset(0.5f);

  cc::TransformOperation foreground_op =
      button.foreground()->GetTargetTransform().at(UiElement::kTranslateIndex);
  cc::TransformOperation background_op =
      button.background()->GetTargetTransform().at(UiElement::kTranslateIndex);
  cc::TransformOperation hit_plane_op =
      button.hit_plane()->GetTargetTransform().at(UiElement::kScaleIndex);

  button.OnHoverEnter(gfx::PointF(0.5f, 0.5f), base::TimeTicks());
  cc::TransformOperation foreground_op_hover =
      button.foreground()->GetTargetTransform().at(UiElement::kTranslateIndex);
  cc::TransformOperation background_op_hover =
      button.background()->GetTargetTransform().at(UiElement::kTranslateIndex);
  cc::TransformOperation hit_plane_op_hover =
      button.hit_plane()->GetTargetTransform().at(UiElement::kScaleIndex);

  EXPECT_TRUE(background_op_hover.translate.z - background_op.translate.z >
              0.f);
  EXPECT_TRUE(hit_plane_op_hover.scale.x - hit_plane_op.scale.x > 0.f);
}

TEST(DiscButton, SizePropagatesToSubElements) {
  DiscButton button(base::RepeatingCallback<void()>(), vector_icons::kMicIcon,
                    nullptr);
  gfx::SizeF size(1000.0f, 1000.0f);
  gfx::SizeF icon_size = size;
  icon_size.Scale(0.5f);
  button.SetSize(size.width(), size.height());

  for (auto& child : button.children()) {
    switch (child->type()) {
      case kTypeButtonBackground:
      case kTypeButtonHitTarget:
        EXPECT_SIZE_EQ(size, child->size());
        EXPECT_FLOAT_EQ(size.width() * 0.5f, child->corner_radius());
        break;
      case kTypeButtonForeground:
        EXPECT_SIZE_EQ(icon_size, child->size());
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

TEST(DiscButton, DrawPhasePropagatesToSubElements) {
  DiscButton button(base::RepeatingCallback<void()>(), vector_icons::kMicIcon,
                    nullptr);
  button.SetDrawPhase(kPhaseOverlayForeground);

  for (auto& child : button.children()) {
    EXPECT_EQ(kPhaseOverlayForeground, child->draw_phase());
  }
}

TEST(DiscButton, NamePropagatesToSubElements) {
  DiscButton button(base::RepeatingCallback<void()>(), vector_icons::kMicIcon,
                    nullptr);
  button.SetName(kCloseButton);

  for (auto& child : button.children()) {
    EXPECT_EQ(child->owner_name_for_test(), kCloseButton);
  }
}

}  // namespace vr
