// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_ui.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager::ui {

TEST(PasswordManagerUITest, StateToString) {
  // Test a few valid states.
  EXPECT_EQ(StateToString(INACTIVE_STATE), "INACTIVE_STATE (0)");
  EXPECT_EQ(StateToString(PENDING_PASSWORD_STATE),
            "PENDING_PASSWORD_STATE (1)");

  // Test invalid state (e.g. cast from int).
  EXPECT_EQ(StateToString(static_cast<State>(-1)), "UNKNOWN_STATE (-1)");
  EXPECT_EQ(StateToString(static_cast<State>(999)), "UNKNOWN_STATE (999)");
}

}  // namespace password_manager::ui
