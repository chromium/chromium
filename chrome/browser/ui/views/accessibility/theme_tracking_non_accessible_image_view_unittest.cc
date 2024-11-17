// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_base.h"

using ThemeTrackingNonAccessibleImageViewTest = views::ViewsTestBase;

TEST_F(ThemeTrackingNonAccessibleImageViewTest, AccessibleProperties) {
  auto view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      ui::ImageModel(), ui::ImageModel(), base::RepeatingCallback<SkColor()>());
  ui::AXNodeData node_data;
  view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kInvisible));
}
