// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/one_time_passwords/otp_suggestion_generator.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class OtpSuggestionGeneratorTest : public testing::Test {};

TEST_F(OtpSuggestionGeneratorTest, EmptyInput) {
  std::vector<Suggestion> suggestions =
      BuildOtpSuggestions(/*one_time_passwords=*/{});
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(OtpSuggestionGeneratorTest, Otps) {
  std::vector<std::string> otps = {"123456", "789012"};
  std::vector<Suggestion> suggestions = BuildOtpSuggestions(otps);

  ASSERT_EQ(suggestions.size(), 2U);
  EXPECT_EQ(suggestions[0].main_text.value, base::UTF8ToUTF16(otps[0]));
  EXPECT_EQ(suggestions[0].type, SuggestionType::kOneTimePasswordEntry);

  EXPECT_EQ(suggestions[1].main_text.value, base::UTF8ToUTF16(otps[1]));
  EXPECT_EQ(suggestions[1].type, SuggestionType::kOneTimePasswordEntry);
}

}  // namespace autofill
