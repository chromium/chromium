// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggestion_group.h"

void SuggestionGroup::MergeFrom(const SuggestionGroup& suggestion_group) {
  // Only update the priority if not previously set.
  if (priority == SuggestionGroupPriority::kDefault) {
    priority = suggestion_group.priority;
  }
  // Only update the header if not previously set.
  if (header.empty()) {
    header = suggestion_group.header;
  }
  // Only update the server group ID if not previously set and given group has
  // a value.
  if (!original_group_id.has_value() &&
      suggestion_group.original_group_id.has_value()) {
    original_group_id = *suggestion_group.original_group_id;
  }
  hidden = suggestion_group.hidden;
}

void SuggestionGroup::Clear() {
  priority = SuggestionGroupPriority::kDefault;
  header.clear();
  original_group_id.reset();
  hidden = false;
}
