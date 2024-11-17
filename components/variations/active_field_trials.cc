// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/active_field_trials.h"

#include <stddef.h>

#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/process/launch.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/variations/hashing.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/variations/variations_crash_keys.h"
#include "components/variations/variations_switches.h"

namespace variations {

namespace {

std::string& GetSeedVersionInternal() {
  static base::NoDestructor<std::string> seed_version;
  return *seed_version;
}

void AppendActiveGroupIdsAsStrings(
    const std::vector<ActiveGroupId> name_group_ids,
    std::vector<std::string>* output) {
  for (const auto& active_group_id : name_group_ids) {
    output->push_back(base::StringPrintf("%x-%x", active_group_id.name,
                                         active_group_id.group));
  }
}

uint32_t HashNameAndSuffix(std::string_view base_name,
                           std::string_view optional_suffix) {
  // Note that most of the time, suffixes are empty, so this avoids creating new
  // strings if not necessary.
  if (optional_suffix.empty()) {
    return HashName(base_name);
  }
  return HashName(base::StrCat({base_name, optional_suffix}));
}

uint32_t HashNameAndSuffix(std::string_view base_name,
                           std::string_view optional_suffix,
                           std::string_view optional_suffix2) {
  if (optional_suffix.empty() && optional_suffix2.empty()) {
    return HashName(base_name);
  }
  return HashName(base::StrCat({base_name, optional_suffix, optional_suffix2}));
}

ActiveGroupId MakeActiveGroupIdWithSuffix(std::string_view trial_name,
                                          std::string_view group_name,
                                          std::string_view optional_suffix,
                                          bool is_overridden) {
  ActiveGroupId id;
  id.name = HashNameAndSuffix(trial_name, optional_suffix);
  id.group =
      HashNameAndSuffix(group_name, optional_suffix,
                        is_overridden ? kOverrideSuffix : std::string_view());
  return id;
}

}  // namespace

ActiveGroupId MakeActiveGroupId(std::string_view trial_name,
                                std::string_view group_name) {
  return MakeActiveGroupId(trial_name, group_name, /*is_overridden=*/false);
}

ActiveGroupId MakeActiveGroupId(std::string_view trial_name,
                                std::string_view group_name,
                                bool is_overridden) {
  ActiveGroupId id;
  id.name = HashName(trial_name);
  id.group = !is_overridden ? HashName(group_name)
                            : HashNameAndSuffix(group_name, kOverrideSuffix);

  return id;
}

void GetFieldTrialActiveGroupIdsForActiveGroups(
    std::string_view suffix,
    const base::FieldTrial::ActiveGroups& active_groups,
    std::vector<ActiveGroupId>* name_group_ids) {
  DCHECK(name_group_ids->empty());
  for (const auto& active_group : active_groups) {
    ActiveGroupId group_id = MakeActiveGroupIdWithSuffix(
        active_group.trial_name, active_group.group_name, suffix,
        active_group.is_overridden);
    name_group_ids->push_back(std::move(group_id));
  }
}

void GetFieldTrialActiveGroupIds(std::string_view suffix,
                                 std::vector<ActiveGroupId>* name_group_ids) {
  DCHECK(name_group_ids->empty());
  // A note on thread safety: Since GetActiveFieldTrialGroups() is thread
  // safe, and we operate on a separate list of that data, this function is
  // technically thread safe as well, with respect to the FieldTrialList data.
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  GetFieldTrialActiveGroupIdsForActiveGroups(suffix, active_groups,
                                             name_group_ids);
}

void GetFieldTrialActiveGroupIds(
    std::string_view suffix,
    const base::FieldTrial::ActiveGroups& active_groups,
    std::vector<ActiveGroupId>* name_group_ids) {
  DCHECK(name_group_ids->empty());
  GetFieldTrialActiveGroupIdsForActiveGroups(suffix, active_groups,
                                             name_group_ids);
}

void GetFieldTrialActiveGroupIdsAsStrings(std::string_view suffix,
                                          std::vector<std::string>* output) {
  DCHECK(output->empty());
  std::vector<ActiveGroupId> name_group_ids;
  GetFieldTrialActiveGroupIds(suffix, &name_group_ids);
  AppendActiveGroupIdsAsStrings(name_group_ids, output);
}

void GetFieldTrialActiveGroupIdsAsStrings(
    std::string_view suffix,
    const base::FieldTrial::ActiveGroups& active_groups,
    std::vector<std::string>* output) {
  DCHECK(output->empty());
  std::vector<ActiveGroupId> name_group_ids;
  GetFieldTrialActiveGroupIds(suffix, active_groups, &name_group_ids);
  AppendActiveGroupIdsAsStrings(name_group_ids, output);
}

void GetSyntheticTrialGroupIdsAsString(std::vector<std::string>* output) {
  std::vector<ActiveGroupId> name_group_ids =
      SyntheticTrialsActiveGroupIdProvider::GetInstance()->GetActiveGroupIds();
  AppendActiveGroupIdsAsStrings(name_group_ids, output);
}

bool HasSyntheticTrial(const std::string& trial_name) {
  std::vector<std::string> synthetic_trials;
  variations::GetSyntheticTrialGroupIdsAsString(&synthetic_trials);
  std::string trial_hash = variations::HashNameAsHexString(trial_name);
  return base::ranges::any_of(synthetic_trials, [&trial_hash](
                                                    const auto& trial) {
    return base::StartsWith(trial, trial_hash, base::CompareCase::SENSITIVE);
  });
}

bool IsInSyntheticTrialGroup(const std::string& trial_name,
                             const std::string& trial_group) {
  std::vector<std::string> synthetic_trials;
  GetSyntheticTrialGroupIdsAsString(&synthetic_trials);
  return base::Contains(
      synthetic_trials,
      base::StringPrintf("%x-%x", HashName(trial_name), HashName(trial_group)));
}

void SetSeedVersion(const std::string& seed_version) {
  GetSeedVersionInternal() = seed_version;
  SetVariationsSeedVersionCrashKey(seed_version);
}

const std::string& GetSeedVersion() {
  return GetSeedVersionInternal();
}

#if BUILDFLAG(USE_BLINK)
void PopulateLaunchOptionsWithVariationsInfo(
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
    base::GlobalDescriptors::Key descriptor_key,
    base::ScopedFD& descriptor_to_share,
#endif
    base::CommandLine* command_line,
    base::LaunchOptions* launch_options) {
  base::FieldTrialList::PopulateLaunchOptionsWithFieldTrialState(
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
      descriptor_key, descriptor_to_share,
#endif
      command_line, launch_options);
  command_line->AppendSwitchASCII(switches::kVariationsSeedVersion,
                                  GetSeedVersion());
}
#endif  // !BUILDFLAG(USE_BLINK)

namespace testing {

void TestGetFieldTrialActiveGroupIds(
    std::string_view suffix,
    const base::FieldTrial::ActiveGroups& active_groups,
    std::vector<ActiveGroupId>* name_group_ids) {
  GetFieldTrialActiveGroupIdsForActiveGroups(suffix, active_groups,
                                             name_group_ids);
}

}  // namespace testing

}  // namespace variations
