/// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_LACROS_ONLY_MOCHA_BROWSER_TEST_H_
#define CHROME_TEST_BASE_CHROMEOS_LACROS_ONLY_MOCHA_BROWSER_TEST_H_

#include "chrome/test/base/chromeos/ash_browser_test_starter.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"

// Base test class that performs additional setup that is only needed when the
// `ash::standalone_browser::features::kLacrosOnly` flag is used.
// TODO(crbug.com/1457360): Decide whether this needs to be added to
// WebUIMochaBrowserTest directly. Keeping it separate for now until more tests
// that need it are migrated to WebUIMochaBrowserTest.
class LacrosOnlyMochaBrowserTest : public WebUIMochaBrowserTest {
 public:
  LacrosOnlyMochaBrowserTest();
  LacrosOnlyMochaBrowserTest(const LacrosOnlyMochaBrowserTest&) = delete;
  LacrosOnlyMochaBrowserTest& operator=(const LacrosOnlyMochaBrowserTest&) =
      delete;
  ~LacrosOnlyMochaBrowserTest() override;

 protected:
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

 private:
  std::unique_ptr<test::AshBrowserTestStarter> ash_starter_;
};

#endif  // CHROME_TEST_BASE_CHROMEOS_LACROS_ONLY_MOCHA_BROWSER_TEST_H_
