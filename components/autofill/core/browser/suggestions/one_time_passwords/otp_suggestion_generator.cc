// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/suggestions/one_time_passwords/otp_suggestion_generator.h"

#include "base/strings/utf_string_conversions.h"

namespace autofill {
namespace {

// Builds Suggestion for given `otp_value`.
Suggestion BuildOtpSuggestion(const std::string& otp_value,
                              const FieldGlobalId& field_id) {
  Suggestion suggestion = Suggestion(base::UTF8ToUTF16(otp_value),
                                     SuggestionType::kOneTimePasswordEntry);
  suggestion.icon = Suggestion::Icon::kAndroidMessages;
  return suggestion;
}

}  // namespace

std::vector<Suggestion> BuildOtpSuggestions(
    std::vector<std::string> one_time_passwords,
    const FieldGlobalId& field_id) {
  std::vector<Suggestion> suggestions;
  for (const std::string& otp_value : one_time_passwords) {
    suggestions.push_back(BuildOtpSuggestion(otp_value, field_id));
  }
  return suggestions;
}

}  // namespace autofill
