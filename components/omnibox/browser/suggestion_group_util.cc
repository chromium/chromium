// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggestion_group_util.h"

#include "third_party/omnibox_proto/groups.pb.h"

namespace {
omnibox::GroupConfig CreateGroup(omnibox::GroupSection section) {
  omnibox::GroupConfig group;
  group.set_section(section);
  return group;
}
}  // namespace

namespace omnibox {

const omnibox::GroupConfigMap& BuildDefaultGroups() {
  static omnibox::GroupConfigMap groups = {
      // clang-format off
      {omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX, CreateGroup(omnibox::SECTION_MOBILE_VERBATIM)},
      {omnibox::GROUP_MOBILE_MOST_VISITED,         CreateGroup(omnibox::SECTION_MOBILE_MOST_VISITED)},
      {omnibox::GROUP_MOBILE_CLIPBOARD,            CreateGroup(omnibox::SECTION_MOBILE_CLIPBOARD)},
      {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST,   CreateGroup(omnibox::SECTION_PERSONALIZED_ZERO_SUGGEST)},
      // clang-format on
  };
  return groups;
}

GroupId GroupIdForNumber(int value) {
  return GroupId_IsValid(value) ? static_cast<GroupId>(value) : GROUP_INVALID;
}

}  // namespace omnibox
