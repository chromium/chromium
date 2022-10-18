// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggestion_group_util.h"

#include "third_party/omnibox_proto/groups.pb.h"

namespace omnibox {

omnibox::GroupConfigMap BuildDefaultGroups() {
  omnibox::GroupConfigMap groups;
  groups[omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX].set_section(
      omnibox::SECTION_MOBILE_VERBATIM);
  groups[omnibox::GROUP_MOBILE_MOST_VISITED].set_section(
      omnibox::SECTION_MOBILE_MOST_VISITED);
  groups[omnibox::GROUP_MOBILE_CLIPBOARD].set_section(
      omnibox::SECTION_MOBILE_CLIPBOARD);
  groups[omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST].set_section(
      omnibox::SECTION_PERSONALIZED_ZERO_SUGGEST);
  return groups;
}

GroupId GroupIdForNumber(int value) {
  return GroupId_IsValid(value) ? static_cast<GroupId>(value) : GROUP_INVALID;
}

}  // namespace omnibox
