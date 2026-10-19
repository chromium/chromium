// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shell_delegate/chrome_shell_delegate.h"

#include "chrome/browser/ui/chromeos/tab_scrubber/tab_scrubber.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ChromeShellDelegateUnitTest = ChromeAshTestBase;

// Verifies that `SetTabScrubberEnabled()` does not crash when
// `TabScrubber::GetInstance()` is null (such as during `ash::Shell` teardown
// after `ChromeBrowserMainExtraPartsAsh` has already reset `tab_scrubber_`).
TEST_F(ChromeShellDelegateUnitTest,
       SetTabScrubberEnabledHandlesNullTabScrubber) {
  ChromeShellDelegate delegate;
  ASSERT_EQ(nullptr, TabScrubber::GetInstance());

  delegate.SetTabScrubberEnabled(false);
  delegate.SetTabScrubberEnabled(true);
}

}  // namespace ash
