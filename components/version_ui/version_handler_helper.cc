// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/version_ui/version_handler_helper.h"

#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/net/variations_command_line.h"

namespace version_ui {

base::Value::List GetVariationsList() {
  std::vector<std::string> variations;
#if !defined(NDEBUG)
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);

  const unsigned char kNonBreakingHyphenUTF8[] = {0xE2, 0x80, 0x91, '\0'};
  const std::string kNonBreakingHyphenUTF8String(
      reinterpret_cast<const char*>(kNonBreakingHyphenUTF8));
  for (const auto& group : active_groups) {
    std::string line = group.trial_name + ":" + group.group_name;
    base::ReplaceChars(line, "-", kNonBreakingHyphenUTF8String, &line);
    variations.push_back(line);
  }
#else
  // In release mode, display the hashes only.
  variations::GetFieldTrialActiveGroupIdsAsStrings(base::StringPiece(),
                                                   &variations);
#endif

  base::Value::List variations_list;
  for (std::string& variation : variations)
    variations_list.Append(std::move(variation));

  return variations_list;
}

base::Value GetVariationsCommandLineAsValue() {
  return base::Value(variations::GetVariationsCommandLine());
}

}  // namespace version_ui
