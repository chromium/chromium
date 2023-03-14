// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/1368284): Remove this test when there are more use cases
// to verify the start ash chrome logic.
class StartUniqueAshBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    // Need to put this before starting lacros.
    StartUniqueAshChrome({}, {}, {"random-unused-example-cmdline"},
                         "Reason:Test");
    InProcessBrowserTest::SetUp();
  }

  void CheckExpectations() { EXPECT_TRUE(ash_process_.IsValid()); }
};

IN_PROC_BROWSER_TEST_F(StartUniqueAshBrowserTest, StartAshChrome) {
  CheckExpectations();
}
