// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggestion_group.h"

// Value chosen based on SuggestionGroupIds::INVALID in suggestion_config.proto.
const int kInvalidSuggestionGroupId = -1;

void SuggestionGroup::MergeFrom(const SuggestionGroup& suggestion_group) {
  // Only update the header if not previously set.
  if (header.empty()) {
    header = suggestion_group.header;
  }
  hidden = suggestion_group.hidden;
}

void SuggestionGroup::Clear() {
  header.clear();
  hidden = false;
}
