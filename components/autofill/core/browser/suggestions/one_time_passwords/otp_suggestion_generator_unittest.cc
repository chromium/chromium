// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/one_time_passwords/otp_suggestion_generator.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class OtpSuggestionGeneratorTest : public testing::Test {
 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

TEST_F(OtpSuggestionGeneratorTest, EmptyInput) {
  std::vector<Suggestion> suggestions =
      BuildOtpSuggestions(/*one_time_passwords=*/{}, test::MakeFieldGlobalId());
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(OtpSuggestionGeneratorTest, Otps) {
  FieldGlobalId field_id = test::MakeFieldGlobalId();
  std::vector<std::string> otps = {"123456", "789012"};
  std::vector<Suggestion> suggestions = BuildOtpSuggestions(otps, field_id);

  ASSERT_EQ(suggestions.size(), 2U);
  EXPECT_EQ(suggestions[0].main_text.value, base::UTF8ToUTF16(otps[0]));
  EXPECT_EQ(suggestions[0].type, SuggestionType::kOneTimePasswordEntry);
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kAndroidMessages);
  ASSERT_TRUE(std::holds_alternative<Suggestion::OneTimePasswordPayload>(
      suggestions[0].payload));
  EXPECT_EQ(
      std::get<Suggestion::OneTimePasswordPayload>(suggestions[0].payload),
      Suggestion::OneTimePasswordPayload(
          {{field_id, base::UTF8ToUTF16(otps[0])}}));

  EXPECT_EQ(suggestions[1].main_text.value, base::UTF8ToUTF16(otps[1]));
  EXPECT_EQ(suggestions[1].type, SuggestionType::kOneTimePasswordEntry);
  EXPECT_EQ(suggestions[1].icon, Suggestion::Icon::kAndroidMessages);
  ASSERT_TRUE(std::holds_alternative<Suggestion::OneTimePasswordPayload>(
      suggestions[1].payload));
  EXPECT_EQ(
      std::get<Suggestion::OneTimePasswordPayload>(suggestions[1].payload),
      Suggestion::OneTimePasswordPayload(
          {{field_id, base::UTF8ToUTF16(otps[1])}}));
}

}  // namespace autofill
