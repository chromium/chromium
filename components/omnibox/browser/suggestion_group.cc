// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggestion_group.h"

omnibox::GroupId GroupIdForNumber(int value) {
  if (!omnibox::GroupId_IsValid(value)) {
    return omnibox::GroupId::INVALID;
  }
  return static_cast<omnibox::GroupId>(value);
}

void SuggestionGroup::MergeFrom(const SuggestionGroup& other) {
  if (priority == SuggestionGroupPriority::kDefault &&
      other.priority != SuggestionGroupPriority::kDefault) {
    priority = other.priority;
  }
  if (!original_group_id.has_value() && other.original_group_id.has_value()) {
    original_group_id = *other.original_group_id;
  }
  group_config_info.MergeFrom(other.group_config_info);
}

void SuggestionGroup::Clear() {
  priority = SuggestionGroupPriority::kDefault;
  original_group_id.reset();
  group_config_info.Clear();
}
