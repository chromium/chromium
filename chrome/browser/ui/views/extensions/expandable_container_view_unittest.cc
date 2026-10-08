// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/expandable_container_view.h"

#include "chrome/test/views/chrome_views_test_base.h"

using ExpandableContainerViewTest = ChromeViewsTestBase;

TEST_F(ExpandableContainerViewTest, VisibleAndCollapsedDetails) {
  std::u16string visible_details = u"• Detail #1\n• Detail #2\n• Detail #3";
  std::u16string collapsed_details = u"• Detail #4";
  auto container = std::make_unique<ExpandableContainerView>(visible_details,
                                                             collapsed_details);

  // Initially the details view is visible because it displays visible_details.
  EXPECT_TRUE(container->details_view()->GetVisible());
  EXPECT_EQ(container->GetDetailsTextForTest(), visible_details);

  // When the link is triggered, the details remain visible and expand to show
  // all details in the same label.
  container->ToggleDetailLevelForTest();
  EXPECT_TRUE(container->details_view()->GetVisible());
  EXPECT_EQ(container->GetDetailsTextForTest(),
            visible_details + u"\n" + collapsed_details);

  // Triggering the link again collapses it back to visible_details.
  container->ToggleDetailLevelForTest();
  EXPECT_TRUE(container->details_view()->GetVisible());
  EXPECT_EQ(container->GetDetailsTextForTest(), visible_details);
}
