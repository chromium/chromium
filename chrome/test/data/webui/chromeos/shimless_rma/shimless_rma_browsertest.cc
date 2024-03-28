// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "ash/webui/shimless_rma/url_constants.h"
#include "base/command_line.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

/**
 * @fileoverview Test suite for chrome://shimless-rma. Tests
 * individual polymer components in isolation. To run all tests in a single
 * instance (default, faster):
 * `browser_tests --gtest_filter=ShimlessRma*`
 * To run a single test suite such as 'AllInputsDisabled':
 * browser_tests --gtest_filter=ShimlessRmaBrowserTest.AllInputsDisabled
 */

namespace ash {
namespace {

class ShimlessRmaBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ShimlessRmaBrowserTest() {
    set_test_loader_host(::ash::kChromeUIShimlessRMAHost);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLaunchRma);
    WebUIMochaBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, AllInputsDisabled) {
  RunTest("chromeos/shimless_rma/all_inputs_disabled_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, HardwareErrorPage) {
  RunTest("chromeos/shimless_rma/hardware_error_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, CriticalErrorPage) {
  RunTest("chromeos/shimless_rma/critical_error_page_test.js", "mocha.run()");
}

}  // namespace

}  // namespace ash
