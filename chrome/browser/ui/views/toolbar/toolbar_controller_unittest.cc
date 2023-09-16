// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"

#include <gtest/gtest.h>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

TEST(ToolbarControllerUnitTest, OverflowButtonVisibility) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyButton1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyButton2);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyButton3);

  std::unique_ptr<views::LayoutProvider> layout_provider =
      ChromeLayoutProvider::CreateLayoutProvider();

  views::View toolbar_container_view;
  toolbar_container_view
      .SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetDefault(views::kFlexBehaviorKey,
                  views::FlexSpecification(
                      views::LayoutOrientation::kHorizontal,
                      views::MinimumFlexSizeRule::kPreferredSnapToZero,
                      views::MaximumFlexSizeRule::kPreferred));

  gfx::Size button_preferred_size(10, 10);

  views::View button1;
  button1.SetProperty(views::kElementIdentifierKey, kDummyButton1);
  toolbar_container_view.AddChildView(&button1);
  button1.SetPreferredSize(button_preferred_size);

  views::View button2;
  button2.SetProperty(views::kElementIdentifierKey, kDummyButton2);
  toolbar_container_view.AddChildView(&button2);
  button2.SetPreferredSize(button_preferred_size);

  views::View button3;
  button3.SetProperty(views::kElementIdentifierKey, kDummyButton3);
  toolbar_container_view.AddChildView(&button3);
  button3.SetPreferredSize(button_preferred_size);

  OverflowButton overflow_button;
  toolbar_container_view.AddChildView(&overflow_button);

  std::vector<ui::ElementIdentifier> element_ids = {
      kDummyButton1, kDummyButton2, kDummyButton3};
  int element_flex_order_start = 1;
  ToolbarController controller(element_ids, element_flex_order_start,
                               &toolbar_container_view, &overflow_button);

  // Enough space to accommodate 3 buttons; overflow button does not show.
  toolbar_container_view.SetSize(gfx::Size(30, 10));
  EXPECT_TRUE(button1.GetVisible());
  EXPECT_TRUE(button2.GetVisible());
  EXPECT_TRUE(button3.GetVisible());
  controller.UpdateOverflowButtonVisibility();
  EXPECT_FALSE(overflow_button.GetVisible());

  // One button overflows.
  toolbar_container_view.SetSize(gfx::Size(25, 10));
  EXPECT_TRUE(button1.GetVisible());
  EXPECT_TRUE(button2.GetVisible());
  EXPECT_FALSE(button3.GetVisible());

  // Overflow button appears.
  controller.UpdateOverflowButtonVisibility();
  EXPECT_TRUE(overflow_button.GetVisible());
}
