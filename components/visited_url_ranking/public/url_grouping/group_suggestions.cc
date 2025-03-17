// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"

namespace visited_url_ranking {

GroupSuggestion::GroupSuggestion() = default;
GroupSuggestion::~GroupSuggestion() = default;

GroupSuggestion::GroupSuggestion(GroupSuggestion&&) = default;
GroupSuggestion& GroupSuggestion::operator=(GroupSuggestion&&) = default;

GroupSuggestions::GroupSuggestions() = default;
GroupSuggestions::~GroupSuggestions() = default;

GroupSuggestions::GroupSuggestions(GroupSuggestions&&) = default;
GroupSuggestions& GroupSuggestions::operator=(GroupSuggestions&&) = default;

}  // namespace visited_url_ranking
