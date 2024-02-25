// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_UTIL_H_
#define COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_UTIL_H_

#include "third_party/omnibox_proto/groups.pb.h"

namespace omnibox {

using GroupConfigMap = std::unordered_map<GroupId, GroupConfig>;

// Builds the pre-defined static groups that are useful for sorting suggestions.
const omnibox::GroupConfigMap& BuildDefaultGroups();

// Returns the omnibox::GroupId enum object corresponding to |value|, or
// omnibox::GROUP_INVALID when there is no corresponding enum object.
GroupId GroupIdForNumber(int value);

// Releases all previously created group definitions for testing purposes.
void ResetDefaultGroupsForTest();

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_GROUP_UTIL_H_
