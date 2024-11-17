// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/chromeos_test_suite.h"

#include "ash/test/ui_controls_ash.h"
#include "base/check.h"
#include "base/command_line.h"
#include "chrome/test/base/chromeos/crosier/helper/switches.h"
#include "chrome/test/base/chromeos/crosier/helper/test_sudo_helper_client.h"
#include "content/public/common/content_switches.h"
#include "ui/events/test/event_generator.h"
#include "ui/ozone/platform/drm/test/ui_controls_system_input_injector.h"

ChromeOSTestSuite::ChromeOSTestSuite(int argc, char** argv)
    : content::ContentTestSuiteBase(argc, argv) {}

ChromeOSTestSuite::~ChromeOSTestSuite() = default;

void ChromeOSTestSuite::Initialize() {
  content::ContentTestSuiteBase::Initialize();

  // chromeos_integration_tests must use functions in ui_controls.h.
  ui::test::EventGenerator::BanEventGenerator();

  ui::test::EnableUIControlsSystemInputInjector();

  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  // Wait for test_sudo_helper server socket if it is used.
  // See b/342392752.
  if (cmdline->HasSwitch(crosier::kSwitchSocketPath)) {
    CHECK(TestSudoHelperClient().WaitForServer(base::Minutes(2)))
        << "Unable to connect to test_sudo_helper.py's socket";
  }

  cmdline->AppendSwitch(switches::kDisableMojoBroker);
}
