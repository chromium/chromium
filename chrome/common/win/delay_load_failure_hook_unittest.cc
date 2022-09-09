// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <delayimp.h>

#include "testing/gtest/include/gtest/gtest.h"

TEST(ChromeDelayLoadHookTest, HooksAreSetAtLinkTime) {
  // This test verifies that delay load hooks are correctly in place for the
  // current module.
  EXPECT_NE(__pfnDliFailureHook2, nullptr);
}
