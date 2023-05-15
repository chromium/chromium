// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/version_ui/version_handler_helper.h"

#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_list_including_low_anonymity.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/net/variations_command_line.h"
#include "components/variations/service/safe_seed_manager.h"

namespace version_ui {

std::string SeedTypeToUiString(variations::SeedType seed_type) {
  switch (seed_type) {
    case variations::SeedType::kRegularSeed:
      // We only display if Safe or Null seed is used.
      return std::string();
    case variations::SeedType::kSafeSeed:
      return "Safe";
    case variations::SeedType::kNullSeed:
      return "Null";
  }
}

base::Value::List GetVariationsList() {
  std::vector<std::string> variations;
  base::FieldTrial::ActiveGroups active_groups;
  // Include low anonymity trial groups in the version string, as it is only
  // displayed locally (and is useful for diagnostics purposes).
  base::FieldTrialListIncludingLowAnonymity::GetActiveFieldTrialGroups(
      &active_groups);
#if !defined(NDEBUG)
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
                                                   active_groups, &variations);
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
