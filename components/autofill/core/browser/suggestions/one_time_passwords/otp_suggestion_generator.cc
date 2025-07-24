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
  // TODO(crbug.com/415273270): Just passing the value string and attempting to
  // fill it on one field covers most of OTP flows, even for multi-field OTPs,
  // however not all of them. Pass the additional data to ensure multi-field
  // OTPs are always handled correctly.
  suggestion.payload = Suggestion::OneTimePasswordPayload(
      {{field_id, base::UTF8ToUTF16(otp_value)}});
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
