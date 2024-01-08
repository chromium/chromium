// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/test_util.h"

#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace ash::standalone_browser {

void AddLacrosArguments(base::span<std::string> new_args,
                        base::CommandLine* command_line) {
  // Extract the old arguments (if any). `old_switch` and `args` will be
  // empty if the command line switch isn't present.
  std::string old_switch =
      command_line->GetSwitchValueASCII(switches::kLacrosChromeAdditionalArgs);
  std::vector<std::string_view> args = base::SplitStringPieceUsingSubstr(
      old_switch, "####", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Append the new args.
  args.insert(args.end(), new_args.begin(), new_args.end());

  // Replace the ash switch.
  command_line->RemoveSwitch(switches::kLacrosChromeAdditionalArgs);
  std::string new_switch = base::JoinString(args, "####");
  command_line->AppendSwitchASCII(switches::kLacrosChromeAdditionalArgs,
                                  new_switch);
}

}  // namespace ash::standalone_browser
