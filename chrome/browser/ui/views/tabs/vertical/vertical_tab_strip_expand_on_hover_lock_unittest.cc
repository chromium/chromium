// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_expand_on_hover_lock.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

class VerticalTabStripExpandOnHoverLockTest : public views::ViewsTestBase {
 public:
  VerticalTabStripExpandOnHoverLockTest() = default;
  ~VerticalTabStripExpandOnHoverLockTest() override = default;
};

TEST_F(VerticalTabStripExpandOnHoverLockTest,
       CreateWithNullRegionViewDoesNotCrash) {
  auto lock = std::make_unique<VerticalTabStripExpandOnHoverLock>(
      nullptr, ExpandOnHoverLockType::kKeepCurrentState);
  EXPECT_TRUE(lock);
  lock.reset();
}
