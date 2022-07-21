// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_H_
#define COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_H_

#include <string>
#include <unordered_map>

#include "third_party/abseil-cpp/absl/types/optional.h"

// Determines the order in which suggestion groups appear in the final displayed
// list relative to one another. A higher numeric value places a given group
// towards the bottom of the suggestion list relative to the other groups with
// lower priority numeric values.
//
// Use a fixed underlying int type for this enum to ensure that the 0-based
// integer indices of the remote zero-prefix suggestions can be safely converted
// to this enum type.
enum class SuggestionGroupPriority : int {
  // The default suggestion group priority. Any suggestion with this priority is
  // placed above the remote zero-prefix suggestions (see below).
  kDefault = 0,
  // Reserved for remote zero-prefix suggestions. The priorities are dynamically
  // assigned to the groups found in the server response based on the 0-based
  // index of the first zero-prefix suggestion in the group.
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

// These values uniquely identify the suggestion groups in SuggestionGroupsMap.
//
// Use a fixed underlying int type for this enum to ensure its values can be
// safely converted to the equivalent Java and Mojom types in Android and WebUI.
enum class SuggestionGroupId : int {
  // SuggestionGroupIds::INVALID in suggestion_config.proto.
  kInvalid = -1,
  // Reserved for non-personalized zero-prefix suggestions. These values don't
  // match the reserved range for these suggestions in suggestion_config.proto.
  // Produced by SearchSuggestionParser.
  kNonPersonalizedZeroSuggest1 = 10000,
  kNonPersonalizedZeroSuggest2 = 10001,
  kNonPersonalizedZeroSuggest3 = 10002,
  kNonPersonalizedZeroSuggest4 = 10003,
  kNonPersonalizedZeroSuggest5 = 10004,
  kNonPersonalizedZeroSuggest6 = 10005,
  kNonPersonalizedZeroSuggest7 = 10006,
  kNonPersonalizedZeroSuggest8 = 10007,
  kNonPersonalizedZeroSuggest9 = 10008,
  kNonPersonalizedZeroSuggest10 = 10009,
  // SuggestionGroupIds::PERSONALIZED_HISTORY_GROUP in suggestion_config.proto.
  // Found in server response. Also Produced by LocalHistoryZeroSuggestProvider.
  kPersonalizedZeroSuggest = 40000,

  // Produced by HistoryClusterProvider.
  kHistoryCluster = 100000,
};

// This allows using SuggestionGroupId as the key in SuggestionGroupsMap.
struct SuggestionGroupIdHash {
  template <typename T>
  int operator()(T t) const {
    return static_cast<int>(t);
  }
};

// Contains the information about the suggestion groups.
struct SuggestionGroup {
  SuggestionGroup() = default;
  ~SuggestionGroup() = default;
  SuggestionGroup(const SuggestionGroup&) = delete;
  SuggestionGroup& operator=(const SuggestionGroup&) = delete;

  void MergeFrom(const SuggestionGroup& suggestion_group);
  void Clear();

  // Determines how this group is placed in the final list of suggestions with
  // relative to the other groups.
  // Inferred from the server response for remote zero-prefix suggestions.
  SuggestionGroupPriority priority{SuggestionGroupPriority::kDefault};
  // The original group ID provided by the server, if applicable.
  absl::optional<int> original_group_id;
  // Group header provided by the server, if applicable.
  std::u16string header{u""};
  // Default visibility provided by the server, if applicable.
  bool hidden{false};
};

// A map of SuggestionGroupId to SuggestionGroup.
using SuggestionGroupsMap = std::
    unordered_map<SuggestionGroupId, SuggestionGroup, SuggestionGroupIdHash>;

#endif  // COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_H_
