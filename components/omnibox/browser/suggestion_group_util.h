// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_UTIL_H_
#define COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_UTIL_H_

#include "components/omnibox/browser/suggestion_group.pb.h"

namespace omnibox {

// A map of omnibox::GroupId to omnibox::SuggestionGroup.
using SuggestionGroupsMap = std::unordered_map<GroupId, SuggestionGroup>;

// Returns the omnibox::GroupId enum object corresponding to |value|, or
// omnibox::GROUP_INVALID when there is no corresponding enum object.
GroupId GroupIdForNumber(int value);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_UTIL_H_
