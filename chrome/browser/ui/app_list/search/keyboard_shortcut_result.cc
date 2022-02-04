// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/keyboard_shortcut_result.h"

#include "chromeos/components/string_matching/tokenized_string_match.h"

namespace app_list {

namespace {

using chromeos::string_matching::TokenizedString;
using chromeos::string_matching::TokenizedStringMatch;

}  // namespace

// TODO(crbug.com/1290682): Complete implementation.
KeyboardShortcutResult::KeyboardShortcutResult(const KeyboardShortcutData& data,
                                               double relevance)
    : description_(data.description_message) {
  set_relevance(relevance);
}

// TODO(crbug.com/1290682): Implement.
KeyboardShortcutResult::~KeyboardShortcutResult() = default;

// TODO(crbug.com/1290682): Implement.
void KeyboardShortcutResult::Open(int event_flags) {}

double KeyboardShortcutResult::CalculateRelevance(
    const TokenizedString& query_tokenized,
    const std::u16string& target) {
  const TokenizedString target_tokenized(target, TokenizedString::Mode::kWords);

  const bool use_default_relevance =
      query_tokenized.text().empty() || target_tokenized.text().empty();

  if (use_default_relevance) {
    static constexpr double kDefaultRelevance = 0.0;
    return kDefaultRelevance;
  }

  TokenizedStringMatch match;
  match.Calculate(query_tokenized, target_tokenized);
  return match.relevance();
}

}  // namespace app_list
