// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category_info.h"

namespace ntp_snippets {

CategoryInfo::CategoryInfo(const std::u16string& title,
                           ContentSuggestionsCardLayout card_layout,
                           ContentSuggestionsAdditionalAction additional_action,
                           bool show_if_empty,
                           const std::u16string& no_suggestions_message)
    : title_(title),
      card_layout_(card_layout),
      additional_action_(additional_action),
      show_if_empty_(show_if_empty),
      no_suggestions_message_(no_suggestions_message) {}

CategoryInfo::CategoryInfo(CategoryInfo&&) = default;
CategoryInfo::CategoryInfo(const CategoryInfo&) = default;

CategoryInfo& CategoryInfo::operator=(CategoryInfo&&) = default;
CategoryInfo& CategoryInfo::operator=(const CategoryInfo&) = default;

CategoryInfo::~CategoryInfo() = default;

}  // namespace ntp_snippets
