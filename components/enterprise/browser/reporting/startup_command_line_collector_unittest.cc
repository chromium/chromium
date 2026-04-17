// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/startup_command_line_collector.h"

#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/test/scoped_command_line.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

using ::testing::Contains;
using ::testing::Not;

TEST(StartupCommandLineCollectorTest, CollectCommandLine) {
  StartupCommandLineCollector::GetInstance()->ResetForTesting();
  base::test::ScopedCommandLine scoped_command_line;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch("test-switch-1");
  command_line->AppendSwitchASCII("test-switch-2", "value-2");

  StartupCommandLineCollector::GetInstance()->CollectCommandLine();
  std::vector<std::string> switches = StartupCommandLineCollector::GetInstance()->GetCollectedSwitchKeys();

  EXPECT_THAT(switches, Contains("test-switch-1"));
  EXPECT_THAT(switches, Contains("test-switch-2"));
}

TEST(StartupCommandLineCollectorTest, Idempotency) {
  StartupCommandLineCollector::GetInstance()->ResetForTesting();
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch("first-switch");

  StartupCommandLineCollector::GetInstance()->CollectCommandLine();
  EXPECT_THAT(StartupCommandLineCollector::GetInstance()->GetCollectedSwitchKeys(),
              Contains("first-switch"));

  // Adding a switch after the snapshot has been captured should cause a crash
  // now due to CHECK();.
  // We can test that the check fails instead of testing idempotency.
  command_line->AppendSwitch("second-switch");
  EXPECT_DEATH(StartupCommandLineCollector::GetInstance()->CollectCommandLine(), "");
}

TEST(StartupCommandLineCollectorTest, ValueExclusion) {
  StartupCommandLineCollector::GetInstance()->ResetForTesting();
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII("switch-with-value", "the-value");

  StartupCommandLineCollector::GetInstance()->CollectCommandLine();
  std::vector<std::string> switches =
      StartupCommandLineCollector::GetInstance()->GetCollectedSwitchKeys();
  EXPECT_THAT(switches, Contains("switch-with-value"));
  // Verify that only the switch key is stored, not the value.
  EXPECT_THAT(switches, Not(Contains("the-value")));
}

TEST(StartupCommandLineCollectorTest, EmptyCommandLine) {
  StartupCommandLineCollector::GetInstance()->ResetForTesting();
  base::test::ScopedCommandLine scoped_command_line;
  // Clear any switches from the test runner.
  *base::CommandLine::ForCurrentProcess() =
      base::CommandLine(base::FilePath(FILE_PATH_LITERAL("program")));

  StartupCommandLineCollector::GetInstance()->CollectCommandLine();
  EXPECT_TRUE(StartupCommandLineCollector::GetInstance()->GetCollectedSwitchKeys().empty());
}

}  // namespace enterprise_reporting
