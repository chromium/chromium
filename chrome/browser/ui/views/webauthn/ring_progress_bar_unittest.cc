// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/ring_progress_bar.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_base.h"

namespace {

using RingProgressBarTest = views::ViewsTestBase;

TEST_F(RingProgressBarTest, AccessibleProperties) {
  auto progress_bar = std::make_unique<RingProgressBar>();
  ui::AXNodeData data;

  progress_bar->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kProgressIndicator);
}

}  // namespace
