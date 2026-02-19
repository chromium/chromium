// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/call_to_action/call_to_action_lock.h"

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class CallToActionLockBrowserTest : public InProcessBrowserTest {
 public:
  CallToActionLockBrowserTest() = default;
  ~CallToActionLockBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(CallToActionLockBrowserTest, BasicFlow) {
  auto* controller = CallToActionLock::From(browser());
  ASSERT_TRUE(controller);

  EXPECT_TRUE(controller->CanAcquireLock());

  auto scoped_action = controller->AcquireLock();
  EXPECT_TRUE(scoped_action);
  EXPECT_FALSE(controller->CanAcquireLock());

  scoped_action.reset();
  EXPECT_TRUE(controller->CanAcquireLock());
}

IN_PROC_BROWSER_TEST_F(CallToActionLockBrowserTest,
                       MultipleInstancesPrevention) {
  auto* controller = CallToActionLock::From(browser());
  ASSERT_TRUE(controller);

  auto scoped_action1 = controller->AcquireLock();
  EXPECT_TRUE(scoped_action1);
  EXPECT_FALSE(controller->CanAcquireLock());

  EXPECT_DEATH_IF_SUPPORTED(controller->AcquireLock(), "");
}
