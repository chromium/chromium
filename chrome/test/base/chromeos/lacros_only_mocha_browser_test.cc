/// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/lacros_only_mocha_browser_test.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"

LacrosOnlyMochaBrowserTest::LacrosOnlyMochaBrowserTest() = default;

LacrosOnlyMochaBrowserTest::~LacrosOnlyMochaBrowserTest() = default;

void LacrosOnlyMochaBrowserTest::SetUpInProcessBrowserTestFixture() {
  if (!ash_starter_.HasLacrosArgument()) {
    return;
  }

  ASSERT_TRUE(ash_starter_.PrepareEnvironmentForLacros());
  WebUIMochaBrowserTest::SetUpInProcessBrowserTestFixture();
}

void LacrosOnlyMochaBrowserTest::SetUpOnMainThread() {
  if (!ash_starter_.HasLacrosArgument()) {
    GTEST_SKIP() << "This test needs to run together with Lacros but the "
                    "--lacros-chrome-path switch is missing.";
  }

  ash_starter_.StartLacros(this);
  WebUIMochaBrowserTest::SetUpOnMainThread();
}
