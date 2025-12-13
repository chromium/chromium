// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/one_time_passwords/otp_suggestion_generator.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/mock_otp_manager.h"
#include "components/autofill/core/browser/suggestions/one_time_passwords/one_time_password_suggestion_data.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::Field;

class OtpSuggestionGeneratorTest : public testing::Test {
 protected:
  OtpSuggestionGeneratorTest() = default;

  AutofillClient& client() { return autofill_client_; }
  OtpSuggestionGenerator& generator() { return generator_; }
  MockOtpManager& otp_manager() { return otp_manager_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  testing::NiceMock<MockOtpManager> otp_manager_;
  OtpSuggestionGenerator generator_{otp_manager_};
};

TEST_F(OtpSuggestionGeneratorTest, GenerateOtpSuggestions) {
  FormData form = test::GetFormData({.fields = {{.role = ONE_TIME_CODE}}});
  FormStructure form_structure(form);
  form_structure.field(0)->SetTypeTo(AutofillType(ONE_TIME_CODE), std::nullopt);

  EXPECT_CALL(otp_manager(), GetOtpSuggestions)
      .WillOnce([](OtpManager::GetOtpSuggestionsCallback callback) {
        std::move(callback).Run(std::vector<std::string>{"123456"});
      });

  base::MockCallback<base::OnceCallback<void(
      std::pair<SuggestionGenerator::SuggestionDataSource,
                std::vector<SuggestionGenerator::SuggestionData>>)>>
      suggestion_data_callback;
  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  EXPECT_CALL(
      suggestions_generated_callback,
      Run(testing::Pair(FillingProduct::kOneTimePassword,
                        testing::UnorderedElementsAre(Field(
                            &Suggestion::main_text,
                            Field(&Suggestion::Text::value, u"123456"))))));
  EXPECT_CALL(suggestion_data_callback,
              Run(testing::Pair(
                  SuggestionGenerator::SuggestionDataSource::kOneTimePassword,
                  testing::UnorderedElementsAre(
                      OneTimePasswordSuggestionData("123456")))))
      .WillOnce([&](std::pair<SuggestionGenerator::SuggestionDataSource,
                              std::vector<SuggestionGenerator::SuggestionData>>
                        suggestion_data) {
        generator().GenerateSuggestions(
            form, form.fields()[0], &form_structure, form_structure.field(0),
            client(), {suggestion_data}, suggestions_generated_callback.Get());
      });

  generator().FetchSuggestionData(form, form.fields()[0], &form_structure,
                                  form_structure.field(0), client(),
                                  suggestion_data_callback.Get());
}

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
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kAndroidMessages);
  EXPECT_EQ(suggestions[0].voice_over, u"Verification Code: 123456");
  EXPECT_EQ(suggestions[0].acceptance_a11y_announcement, u"Autofilled code");
#endif

  EXPECT_EQ(suggestions[1].main_text.value, base::UTF8ToUTF16(otps[1]));
  EXPECT_EQ(suggestions[1].type, SuggestionType::kOneTimePasswordEntry);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(suggestions[1].icon, Suggestion::Icon::kAndroidMessages);
  EXPECT_EQ(suggestions[1].voice_over, u"Verification Code: 789012");
  EXPECT_EQ(suggestions[1].acceptance_a11y_announcement, u"Autofilled code");
#endif
}

}  // namespace autofill
