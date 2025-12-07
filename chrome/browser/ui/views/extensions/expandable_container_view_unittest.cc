// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/expandable_container_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/views/chrome_views_test_base.h"

using ExpandableContainerViewTest = ChromeViewsTestBase;

TEST_F(ExpandableContainerViewTest, DetailLevelVisibility) {
  std::u16string details = u"- Detail #1 \n - Detail #2 \n - Detail #3";
  auto container = std::make_unique<ExpandableContainerView>(details);

  // Initially the details view should not be expanded or visible.
  EXPECT_FALSE(container->details_view()->GetVisible());

  // When the link is triggered, the details should get expanded and become
  // visible.
  container->ToggleDetailLevelForTest();
  EXPECT_TRUE(container->details_view()->GetVisible());
}
