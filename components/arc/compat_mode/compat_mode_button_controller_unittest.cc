// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/compat_mode_button_controller.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"

namespace arc {

class CompatModeButtonControllerTest : public views::ViewsTestBase {
 private:
  CompatModeButtonController controller_;
};

TEST_F(CompatModeButtonControllerTest, ConstructDestruct) {}

// TODO(b/191956214): Add more test cases.

}  // namespace arc
