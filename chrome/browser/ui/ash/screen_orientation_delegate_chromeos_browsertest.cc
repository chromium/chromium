// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/ui_base_features.h"

using ScreenOrientationDelegateChromeosTest = InProcessBrowserTest;

// Tests that an orientation delegate is created and set. Regression test for
// https://crbug.com/889981
IN_PROC_BROWSER_TEST_F(ScreenOrientationDelegateChromeosTest, Basic) {
  EXPECT_NE(nullptr, content::GetScreenOrientationDelegate());
}
