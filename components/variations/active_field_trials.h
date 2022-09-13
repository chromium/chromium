// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_ACTIVE_FIELD_TRIALS_H_
#define COMPONENTS_VARIATIONS_ACTIVE_FIELD_TRIALS_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_piece.h"

namespace variations {

// The Unique ID of a trial and its active group, where the name and group
// identifiers are hashes of the trial and group name strings.
struct COMPONENT_EXPORT(VARIATIONS) ActiveGroupId {
  uint32_t name;
  uint32_t group;
};

// Returns an ActiveGroupId struct for the given trial and group names.
COMPONENT_EXPORT(VARIATIONS)
ActiveGroupId MakeActiveGroupId(base::StringPiece trial_name,
                                base::StringPiece group_name);

// We need to supply a Compare class for templates since ActiveGroupId is a
// user-defined type.
struct COMPONENT_EXPORT(VARIATIONS) ActiveGroupIdCompare {
  bool operator() (const ActiveGroupId& lhs, const ActiveGroupId& rhs) const {
    // The group and name fields are just SHA-1 Hashes, so we just need to treat
    // them as IDs and do a less-than comparison. We test group first, since
    // name is more likely to collide.
    if (lhs.group != rhs.group)
      return lhs.group < rhs.group;
    return lhs.name < rhs.name;
  }
};

// Populates |name_group_ids| based on |active_groups|. Field trial names are
// suffixed with |suffix| before hashing is executed.
COMPONENT_EXPORT(VARIATIONS)
void GetFieldTrialActiveGroupIdsForActiveGroups(
    base::StringPiece suffix,
    const base::FieldTrial::ActiveGroups& active_groups,
    std::vector<ActiveGroupId>* name_group_ids);

// Fills the supplied vector |name_group_ids| (which must be empty when called)
// with unique ActiveGroupIds for each Field Trial that has a chosen group.
// Field Trials for which a group has not been chosen yet are NOT returned in
// this list. Field trial names are suffixed with |suffix| before hashing is
// executed.
COMPONENT_EXPORT(VARIATIONS)
void GetFieldTrialActiveGroupIds(base::StringPiece suffix,
                                 std::vector<ActiveGroupId>* name_group_ids);

// Fills the supplied vector |output| (which must be empty when called) with
// unique string representations of ActiveGroupIds for each Field Trial that
// has a chosen group. The strings are formatted as "<TrialName>-<GroupName>",
// with the names as hex strings. Field Trials for which a group has not been
// chosen yet are NOT returned in this list. Field trial names are suffixed with
// |suffix| before hashing is executed.
COMPONENT_EXPORT(VARIATIONS)
void GetFieldTrialActiveGroupIdsAsStrings(base::StringPiece suffix,
                                          std::vector<std::string>* output);

// TODO(rkaplow): Support suffixing for synthetic trials.
// Fills the supplied vector |output| (which must be empty when called) with
// unique string representations of ActiveGroupIds for each Syntehtic Trial
// group. The strings are formatted as "<TrialName>-<GroupName>",
// with the names as hex strings. Synthetic Field Trials for which a group
// which hasn't been chosen yet are NOT returned in this list.
COMPONENT_EXPORT(VARIATIONS)
void GetSyntheticTrialGroupIdsAsString(std::vector<std::string>* output);

// Returns true if a synthetic trial with the name `trial_name` is currently
// active, i.e. the named trial has chosen a group. Returns false otherwise.
COMPONENT_EXPORT(VARIATIONS)
bool HasSyntheticTrial(const std::string& trial_name);

// Returns true if a synthetic trial with the name `trial_name` is active
// with its chosen group matching `trial_group`. Returns false otherwise.
COMPONENT_EXPORT(VARIATIONS)
bool IsInSyntheticTrialGroup(const std::string& trial_name,
                             const std::string& trial_group);

// Sets the version of the seed that the current set of FieldTrials was
// generated from.
// TODO(crbug/507665): Move this to field_trials_provider once it moves
// into components/variations
COMPONENT_EXPORT(VARIATIONS)
void SetSeedVersion(const std::string& seed_version);

// Gets the version of the seed that the current set of FieldTrials was
// generated from.
// Only works on the browser process; returns the empty string from other
// processes.
// TODO(crbug/507665): Move this to field_trials_provider once it moves
// into components/variations
COMPONENT_EXPORT(VARIATIONS)
const std::string& GetSeedVersion();

// Expose some functions for testing. These functions just wrap functionality
// that is implemented above.
namespace testing {

COMPONENT_EXPORT(VARIATIONS)
void TestGetFieldTrialActiveGroupIds(
    base::StringPiece suffix,
    const base::FieldTrial::ActiveGroups& active_groups,
    std::vector<ActiveGroupId>* name_group_ids);

}  // namespace testing

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_ACTIVE_FIELD_TRIALS_H_
