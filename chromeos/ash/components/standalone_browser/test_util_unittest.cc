// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/test_util.h"

#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::standalone_browser {

TEST(AddLacrosArguments, NoExistingArgs) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  ASSERT_FALSE(command_line.HasSwitch(switches::kLacrosChromeAdditionalArgs));

  std::vector<std::string> args = {"--arg1", "--arg2=value2"};
  AddLacrosArguments(args, &command_line);

  EXPECT_EQ(
      command_line.GetSwitchValueASCII(switches::kLacrosChromeAdditionalArgs),
      "--arg1####--arg2=value2");
}

TEST(AddLacrosArguments, WithExistingArgs) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  ASSERT_FALSE(command_line.HasSwitch(switches::kLacrosChromeAdditionalArgs));

  command_line.AppendSwitchASCII(switches::kLacrosChromeAdditionalArgs,
                                 "--arg1####--arg2=value2");
  std::vector<std::string> args = {"--arg3", "--arg4=value4"};
  AddLacrosArguments(args, &command_line);

  EXPECT_EQ(
      command_line.GetSwitchValueASCII(switches::kLacrosChromeAdditionalArgs),
      "--arg1####--arg2=value2####--arg3####--arg4=value4");
}

}  // namespace ash::standalone_browser
