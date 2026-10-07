// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/one_time_passwords/otp_suggestion_generator.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/mock_otp_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_util.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace autofill {

using ::base::test::RunOnceCallback;
using ::one_time_tokens::OneTimeToken;
using ::one_time_tokens::OneTimeTokenType;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;

class OtpSuggestionGeneratorTest : public testing::Test {
 protected:
  OtpSuggestionGeneratorTest() = default;

  TestAutofillClient& client() { return autofill_client_; }
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
      .WillOnce(RunOnceCallback<2>(std::vector<OneTimeToken>{OneTimeToken(
          OneTimeTokenType::kSmsOtp, "123456", base::TimeTicks::Now())}));

  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  EXPECT_CALL(
      suggestions_generated_callback,
      Run(Pair(
          SuggestionGenerator::SuggestionDataSource::kOneTimePassword,
          ElementsAre(AllOf(Field(&Suggestion::main_text,
                                  Field(&Suggestion::Text::value, u"123456")),
                            Field(&Suggestion::type,
                                  SuggestionType::kOneTimePasswordEntry))))));

  generator().GenerateSuggestions(form, form.fields()[0], &form_structure,
                                  form_structure.field(0), client(),
                                  suggestions_generated_callback.Get());
}

TEST_F(OtpSuggestionGeneratorTest, GenerateOtpSuggestions_GmailOtp) {
  FormData form = test::GetFormData({.fields = {{.role = ONE_TIME_CODE}}});
  FormStructure form_structure(form);
  form_structure.field(0)->SetTypeTo(AutofillType(ONE_TIME_CODE), std::nullopt);

  EXPECT_CALL(otp_manager(), GetOtpSuggestions)
      .WillOnce(RunOnceCallback<2>(std::vector<OneTimeToken>{
          OneTimeToken(OneTimeTokenType::kGmail, "123456",
                       base::TimeTicks::Now(), "sender@example.com")}));

  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  EXPECT_CALL(
      suggestions_generated_callback,
      Run(Pair(
          SuggestionGenerator::SuggestionDataSource::kOneTimePassword,
          ElementsAre(
              AllOf(Field(&Suggestion::main_text,
                          Field(&Suggestion::Text::value, u"123456")),
                    Field(&Suggestion::type,
                          SuggestionType::kGmailOneTimePasswordEntry),
                    Field(&Suggestion::labels,
                          ElementsAre(ElementsAre(
                              Suggestion::Text(u"From sender@example.com"))))),
              Field(&Suggestion::type, SuggestionType::kSeparator),
              Field(&Suggestion::type, SuggestionType::kOpenGmailForOtps)))));

  generator().GenerateSuggestions(form, form.fields()[0], &form_structure,
                                  form_structure.field(0), client(),
                                  suggestions_generated_callback.Get());
}

TEST_F(OtpSuggestionGeneratorTest, GenerateOtpSuggestions_EmptyOtpList) {
  FormData form = test::GetFormData({.fields = {{.role = ONE_TIME_CODE}}});
  FormStructure form_structure(form);
  form_structure.field(0)->SetTypeTo(AutofillType(ONE_TIME_CODE), std::nullopt);

  EXPECT_CALL(otp_manager(), GetOtpSuggestions)
      .WillOnce(RunOnceCallback<2>(std::vector<OneTimeToken>{}));

  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  EXPECT_CALL(
      suggestions_generated_callback,
      Run(Pair(SuggestionGenerator::SuggestionDataSource::kOneTimePassword,
               IsEmpty())));

  generator().GenerateSuggestions(form, form.fields()[0], &form_structure,
                                  form_structure.field(0), client(),
                                  suggestions_generated_callback.Get());
}

TEST_F(OtpSuggestionGeneratorTest, EmptyInput) {
  EXPECT_TRUE(BuildOtpSuggestions(base::span<const OneTimeToken>{}).empty());
}

TEST_F(OtpSuggestionGeneratorTest, GmailOtps) {
  std::vector<OneTimeToken> otps = {
      OneTimeToken(OneTimeTokenType::kGmail, "123456", base::TimeTicks::Now(),
                   "user@gmail.com"),
      OneTimeToken(OneTimeTokenType::kGmail, "789012", base::TimeTicks::Now(),
                   "user@gmail.com"),
  };
  std::vector<Suggestion> suggestions = BuildOtpSuggestions(otps);

  ASSERT_EQ(suggestions.size(), 4U);
  EXPECT_EQ(suggestions[0].main_text.value, u"123456");
  EXPECT_EQ(suggestions[0].type, SuggestionType::kGmailOneTimePasswordEntry);
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kMailAsterisk);
  EXPECT_THAT(suggestions[0].minor_texts,
              ElementsAre(Suggestion::Text(u"Verification code")));
  EXPECT_THAT(
      suggestions[0].labels,
      ElementsAre(ElementsAre(Suggestion::Text(u"From user@gmail.com"))));
  EXPECT_EQ(suggestions[0].voice_over, u"Verification code: 123456");
  EXPECT_EQ(suggestions[0].acceptance_a11y_announcement, u"Autofilled code");

  EXPECT_EQ(suggestions[1].main_text.value, u"789012");
  EXPECT_EQ(suggestions[1].type, SuggestionType::kGmailOneTimePasswordEntry);
  EXPECT_EQ(suggestions[1].icon, Suggestion::Icon::kMailAsterisk);
  EXPECT_THAT(suggestions[1].minor_texts,
              ElementsAre(Suggestion::Text(u"Verification code")));
  EXPECT_THAT(
      suggestions[1].labels,
      ElementsAre(ElementsAre(Suggestion::Text(u"From user@gmail.com"))));
  EXPECT_EQ(suggestions[1].voice_over, u"Verification code: 789012");
  EXPECT_EQ(suggestions[1].acceptance_a11y_announcement, u"Autofilled code");

  EXPECT_EQ(suggestions[2].type, SuggestionType::kSeparator);

  EXPECT_EQ(suggestions[3].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_OPEN_GMAIL_FOR_OTP));
  EXPECT_EQ(suggestions[3].type, SuggestionType::kOpenGmailForOtps);
  EXPECT_EQ(suggestions[3].icon, Suggestion::Icon::kGmail);
  EXPECT_EQ(suggestions[3].trailing_icon, Suggestion::Icon::kOpenInNew);
}

TEST_F(OtpSuggestionGeneratorTest, GmailOtps_EmptyAccountEmailReturnsEmpty) {
  std::vector<OneTimeToken> otps = {
      OneTimeToken(OneTimeTokenType::kGmail, "123456", base::TimeTicks::Now(),
                   /*sender_address=*/""),
  };
  std::vector<Suggestion> suggestions = BuildOtpSuggestions(otps);

  EXPECT_TRUE(suggestions.empty());
}

TEST_F(OtpSuggestionGeneratorTest, BuildOtpSuggestions_MixedOneTimeTokens) {
  std::vector<OneTimeToken> tokens = {
      OneTimeToken(OneTimeTokenType::kSmsOtp, "111111", base::TimeTicks::Now()),
      OneTimeToken(OneTimeTokenType::kGmail, "222222", base::TimeTicks::Now(),
                   "sender1@example.com"),
      OneTimeToken(OneTimeTokenType::kGmail, "333333", base::TimeTicks::Now(),
                   "sender2@example.com"),
  };
  std::vector<Suggestion> suggestions = BuildOtpSuggestions(tokens);

  ASSERT_EQ(suggestions.size(), 5U);
  EXPECT_EQ(suggestions[0].main_text.value, u"111111");
  EXPECT_EQ(suggestions[0].type, SuggestionType::kOneTimePasswordEntry);
  EXPECT_TRUE(suggestions[0].labels.empty());

  EXPECT_EQ(suggestions[1].main_text.value, u"222222");
  EXPECT_EQ(suggestions[1].type, SuggestionType::kGmailOneTimePasswordEntry);
  EXPECT_THAT(
      suggestions[1].labels,
      ElementsAre(ElementsAre(Suggestion::Text(u"From sender1@example.com"))));

  EXPECT_EQ(suggestions[2].main_text.value, u"333333");
  EXPECT_EQ(suggestions[2].type, SuggestionType::kGmailOneTimePasswordEntry);
  EXPECT_THAT(
      suggestions[2].labels,
      ElementsAre(ElementsAre(Suggestion::Text(u"From sender2@example.com"))));

  EXPECT_EQ(suggestions[3].type, SuggestionType::kSeparator);
  EXPECT_EQ(suggestions[4].type, SuggestionType::kOpenGmailForOtps);
}

TEST_F(OtpSuggestionGeneratorTest, SmsOtps) {
  std::vector<OneTimeToken> otps = {
      OneTimeToken(OneTimeTokenType::kSmsOtp, "123456", base::TimeTicks::Now()),
      OneTimeToken(OneTimeTokenType::kSmsOtp, "789012", base::TimeTicks::Now()),
  };
  std::vector<Suggestion> suggestions = BuildOtpSuggestions(otps);

  ASSERT_EQ(suggestions.size(), 2U);
  EXPECT_EQ(suggestions[0].main_text.value, u"123456");
  EXPECT_EQ(suggestions[0].type, SuggestionType::kOneTimePasswordEntry);
  EXPECT_EQ(suggestions[0].voice_over, u"Verification code: 123456");
  EXPECT_EQ(suggestions[0].acceptance_a11y_announcement, u"Autofilled code");
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kAndroidMessages);
#else
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kNoIcon);
#endif

  EXPECT_EQ(suggestions[1].main_text.value, u"789012");
  EXPECT_EQ(suggestions[1].type, SuggestionType::kOneTimePasswordEntry);
  EXPECT_EQ(suggestions[1].voice_over, u"Verification code: 789012");
  EXPECT_EQ(suggestions[1].acceptance_a11y_announcement, u"Autofilled code");
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(suggestions[1].icon, Suggestion::Icon::kAndroidMessages);
#else
  EXPECT_EQ(suggestions[1].icon, Suggestion::Icon::kNoIcon);
#endif
}

// Test that OTP suggestions are silently suppressed on insecure contexts.
TEST_F(OtpSuggestionGeneratorTest, GenerateOtpSuggestions_InsecureContext) {
  client().set_last_committed_primary_main_frame_url(
      GURL("http://example.com"));
  FormData form = test::GetFormData({.fields = {{.role = ONE_TIME_CODE}}});
  FormStructure form_structure(form);
  form_structure.field(0)->SetTypeTo(AutofillType(ONE_TIME_CODE), std::nullopt);

  EXPECT_CALL(otp_manager(), GetOtpSuggestions).Times(0);

  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  EXPECT_CALL(
      suggestions_generated_callback,
      Run(Pair(SuggestionGenerator::SuggestionDataSource::kOneTimePassword,
               IsEmpty())));

  generator().GenerateSuggestions(form, form.fields()[0], &form_structure,
                                  form_structure.field(0), client(),
                                  suggestions_generated_callback.Get());
}

}  // namespace autofill
