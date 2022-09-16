// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/active_field_trials.h"

#include <stddef.h>

#include <vector>

#include "base/containers/contains.h"
#include "base/lazy_instance.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/variations/hashing.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"

namespace variations {

namespace {

base::LazyInstance<std::string>::Leaky g_seed_version;

void AppendActiveGroupIdsAsStrings(
    const std::vector<ActiveGroupId> name_group_ids,
    std::vector<std::string>* output) {
  for (const auto& active_group_id : name_group_ids) {
    output->push_back(base::StringPrintf("%x-%x", active_group_id.name,
                                         active_group_id.group));
  }
}

}  // namespace

ActiveGroupId MakeActiveGroupId(base::StringPiece trial_name,
                                base::StringPiece group_name) {
  ActiveGroupId id;
  id.name = HashName(trial_name);
  id.group = HashName(group_name);
  return id;
}

void GetFieldTrialActiveGroupIdsForActiveGroups(
    base::StringPiece suffix,
    const base::FieldTrial::ActiveGroups& active_groups,
    std::vector<ActiveGroupId>* name_group_ids) {
  DCHECK(name_group_ids->empty());
  for (const auto& active_group : active_groups) {
    name_group_ids->push_back(
        MakeActiveGroupId(active_group.trial_name + std::string(suffix),
                          active_group.group_name + std::string(suffix)));
  }
}

void GetFieldTrialActiveGroupIds(base::StringPiece suffix,
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

void GetFieldTrialActiveGroupIdsAsStrings(base::StringPiece suffix,
                                          std::vector<std::string>* output) {
  DCHECK(output->empty());
  std::vector<ActiveGroupId> name_group_ids;
  GetFieldTrialActiveGroupIds(suffix, &name_group_ids);
  AppendActiveGroupIdsAsStrings(name_group_ids, output);
}

void GetSyntheticTrialGroupIdsAsString(std::vector<std::string>* output) {
  std::vector<ActiveGroupId> name_group_ids;
  SyntheticTrialsActiveGroupIdProvider::GetInstance()->GetActiveGroupIds(
      &name_group_ids);
  AppendActiveGroupIdsAsStrings(name_group_ids, output);
}

bool HasSyntheticTrial(const std::string& trial_name) {
  std::vector<std::string> synthetic_trials;
  variations::GetSyntheticTrialGroupIdsAsString(&synthetic_trials);
  std::string trial_hash =
      base::StringPrintf("%x", variations::HashName(trial_name));
  return base::ranges::any_of(synthetic_trials, [trial_hash](
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
  g_seed_version.Get() = seed_version;
}

const std::string& GetSeedVersion() {
  return g_seed_version.Get();
}

namespace testing {

void TestGetFieldTrialActiveGroupIds(
    base::StringPiece suffix,
    const base::FieldTrial::ActiveGroups& active_groups,
    std::vector<ActiveGroupId>* name_group_ids) {
  GetFieldTrialActiveGroupIdsForActiveGroups(suffix, active_groups,
                                             name_group_ids);
}

}  // namespace testing

}  // namespace variations
