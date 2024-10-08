// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/version_ui/version_handler_helper.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_list_including_low_anonymity.h"
#include "base/strings/string_util.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/net/variations_command_line.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"

namespace version_ui {
namespace {

#if !defined(NDEBUG)
std::string GetActiveGroupNameAsString(
    const base::FieldTrial::ActiveGroup& group) {
  static const unsigned char kNonBreakingHyphenUTF8[] = {0xE2, 0x80, 0x91,
                                                         '\0'};
  static std::string_view kNonBreakingHyphenUTF8String(
      reinterpret_cast<const char*>(kNonBreakingHyphenUTF8));

  std::string result = group.trial_name + ":" + group.group_name;
  base::ReplaceChars(result, "-", kNonBreakingHyphenUTF8String, &result);
  return result;
}
#endif  // !defined(NDEBUG)

}  // namespace

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
  for (const auto& group : active_groups) {
    variations.push_back(GetActiveGroupNameAsString(group));
  }

  // Synthetic field trials.
  for (const variations::SyntheticTrialGroup& group :
       variations::SyntheticTrialsActiveGroupIdProvider::GetInstance()
           ->GetGroups()) {
    variations.push_back(GetActiveGroupNameAsString(group.active_group()));
  }
#else
  // In release mode, display the hashes only.
  variations::GetFieldTrialActiveGroupIdsAsStrings(std::string_view(),
                                                   active_groups, &variations);

  // Synthetic field trials.
  std::vector<std::string> synthetic_field_trials;
  variations::GetSyntheticTrialGroupIdsAsString(&synthetic_field_trials);
  variations.insert(variations.end(), synthetic_field_trials.begin(),
                    synthetic_field_trials.end());
#endif

  base::Value::List variations_list;
  for (std::string& variation : variations) {
    variations_list.Append(std::move(variation));
  }

  return variations_list;
}

base::Value GetVariationsCommandLineAsValue() {
  return base::Value(
      variations::VariationsCommandLine::GetForCurrentProcess().ToString());
}

}  // namespace version_ui
