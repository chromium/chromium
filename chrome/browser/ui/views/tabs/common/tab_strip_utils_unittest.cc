// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_strip_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

TEST(TabStripUtilsTest, ViewLayerLivesWhileAnyReferenceIsHeld) {
  views::View view;
  ASSERT_EQ(view.layer(), nullptr);

  AcquireViewLayer(&view);
  EXPECT_NE(view.layer(), nullptr);

  // A second holder does not create a second layer.
  ui::Layer* const layer = view.layer();
  AcquireViewLayer(&view);
  EXPECT_EQ(view.layer(), layer);

  // Releasing one of two references keeps the layer.
  ReleaseViewLayer(&view);
  EXPECT_EQ(view.layer(), layer);

  // Releasing the last reference destroys it.
  ReleaseViewLayer(&view);
  EXPECT_EQ(view.layer(), nullptr);

  // The count starts over once it reaches zero.
  AcquireViewLayer(&view);
  EXPECT_NE(view.layer(), nullptr);
  ReleaseViewLayer(&view);
  EXPECT_EQ(view.layer(), nullptr);
}
