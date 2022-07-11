// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_H_
#define COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_H_

#include <map>
#include <string>

// Indicates an invalid suggestion group Id.
extern const int kInvalidSuggestionGroupId;

// Contains the information about the suggestion groups.
struct SuggestionGroup {
  SuggestionGroup() = default;
  ~SuggestionGroup() = default;
  SuggestionGroup(const SuggestionGroup&) = delete;
  SuggestionGroup& operator=(const SuggestionGroup&) = delete;

  void MergeFrom(const SuggestionGroup& suggestion_group);
  void Clear();

  // Group header provided by the server.
  std::u16string header{u""};
  // Whether the group should be hidden by default.
  bool hidden{false};
};

using SuggestionGroupsMap = std::map<int, SuggestionGroup>;

#endif  // COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_H_
