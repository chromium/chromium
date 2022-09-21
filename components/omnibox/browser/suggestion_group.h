// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_H_
#define COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_H_

#include <string>
#include <unordered_map>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/omnibox_proto/group_config_info.pb.h"
#include "third_party/omnibox_proto/group_id.pb.h"

// Determines the order in which suggestion groups appear in the final displayed
// list relative to one another. A higher numeric value places a given group
// towards the bottom of the suggestion list relative to the other groups with
// lower priority numeric values.
enum class SuggestionGroupPriority {
  // The default suggestion group priority. Any suggestion with this priority is
  // placed above the remote zero-prefix suggestions (see below).
  kDefault = 0,
  // Reserved for remote zero-prefix suggestions. The priorities are dynamically
  // assigned to the groups found in the server response based on the order in
  // which they appear in the results.
  kRemoteZeroSuggest1 = 1,
  kRemoteZeroSuggest2 = 2,
  kRemoteZeroSuggest3 = 3,
  kRemoteZeroSuggest4 = 4,
  kRemoteZeroSuggest5 = 5,
  kRemoteZeroSuggest6 = 6,
  kRemoteZeroSuggest7 = 7,
  kRemoteZeroSuggest8 = 8,
  kRemoteZeroSuggest9 = 9,
  kRemoteZeroSuggest10 = 10,
};

// This allows using omnibox::GroupId as the key in SuggestionGroupsMap.
struct GroupIdHash {
  template <typename T>
  int operator()(T t) const {
    return static_cast<int>(t);
  }
};

// Returns the omnibox::GroupId enum object corresponding to |value|. Returns
// omnibox::GroupId::INVALID when there is no corresponding enum object.
omnibox::GroupId GroupIdForNumber(int value);

// Contains the information about the suggestion groups.
struct SuggestionGroup {
  SuggestionGroup() = default;
  ~SuggestionGroup() = default;
  SuggestionGroup(const SuggestionGroup&) = default;
  SuggestionGroup& operator=(const SuggestionGroup&) = default;

  // Merges the fields from |from|, if specified in |from|.
  void MergeFrom(const SuggestionGroup& other);
  void Clear();

  // Determines how this group is placed in the final list of suggestions with
  // relative to the other groups.
  // Inferred from the server response for remote zero-prefix suggestions.
  SuggestionGroupPriority priority{SuggestionGroupPriority::kDefault};
  // The original group ID provided by the server, if applicable.
  absl::optional<int> original_group_id;
  // The Suggestion group configurations.
  omnibox::GroupConfigInfo group_config_info;
};

// A map of omnibox::GroupId to SuggestionGroup.
using SuggestionGroupsMap =
    std::unordered_map<omnibox::GroupId, SuggestionGroup, GroupIdHash>;

#endif  // COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_H_
