// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/one_time_passwords/otp_suggestion_generator.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/mock_otp_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_util.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace autofill {
namespace {

using ::base::test::RunOnceCallback;
using ::one_time_tokens::OneTimeToken;
using ::one_time_tokens::OneTimeTokenType;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Property;
using ::testing::TestWithParam;
using ::testing::Values;

Matcher<Suggestion> IsGmailOtpSuggestion(const std::u16string& code,
                                         const std::u16string& sender) {
  return AllOf(
      EqualsSuggestion(SuggestionType::kGmailOneTimePasswordEntry, code,
                       Suggestion::Icon::kMailAsterisk,
                       {{Suggestion::Text(sender)}}),
      Field(&Suggestion::minor_texts,
            ElementsAre(Suggestion::Text(u"Verification code"))),
      Field(&Suggestion::voice_over,
            u"Verification code: " + code + u" from " + sender),
      Field(&Suggestion::acceptance_a11y_announcement, u"Autofilled code"));
}

Matcher<Suggestion> IsSmsOtpSuggestion(const std::u16string& code) {
  return AllOf(
      EqualsSuggestion(SuggestionType::kOneTimePasswordEntry, code),
      Field(&Suggestion::labels, IsEmpty()),
      Field(&Suggestion::voice_over, u"Verification code: " + code),
      Field(&Suggestion::acceptance_a11y_announcement, u"Autofilled code"),
#if BUILDFLAG(IS_ANDROID)
      HasIcon(Suggestion::Icon::kAndroidMessages)
#else
      HasIcon(Suggestion::Icon::kNoIcon)
#endif
  );
}

Matcher<Suggestion> IsOpenGmailForOtpsSuggestion() {
  return AllOf(EqualsSuggestion(
                   SuggestionType::kOpenGmailForOtps,
                   l10n_util::GetStringUTF16(IDS_AUTOFILL_OPEN_GMAIL_FOR_OTP),
                   Suggestion::Icon::kGmail),
               HasTrailingIcon(Suggestion::Icon::kOpenInNew));
}

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
  NiceMock<MockOtpManager> otp_manager_;
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
      Run(Pair(SuggestionGenerator::SuggestionDataSource::kOneTimePassword,
               ElementsAre(IsSmsOtpSuggestion(u"123456")))));

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
          ElementsAre(IsGmailOtpSuggestion(u"123456", u"sender@example.com"),
                      EqualsSuggestion(SuggestionType::kSeparator),
                      IsOpenGmailForOtpsSuggestion()))));

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
  EXPECT_THAT(BuildOtpSuggestions(base::span<const OneTimeToken>{}), IsEmpty());
}

TEST_F(OtpSuggestionGeneratorTest, GmailOtps) {
  std::vector<OneTimeToken> otps = {
      OneTimeToken(OneTimeTokenType::kGmail, "123456", base::TimeTicks::Now(),
                   "user@gmail.com"),
      OneTimeToken(OneTimeTokenType::kGmail, "789012", base::TimeTicks::Now(),
                   "user@gmail.com"),
  };

  EXPECT_THAT(BuildOtpSuggestions(otps),
              ElementsAre(IsGmailOtpSuggestion(u"123456", u"user@gmail.com"),
                          IsGmailOtpSuggestion(u"789012", u"user@gmail.com"),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          IsOpenGmailForOtpsSuggestion()));
}

TEST_F(OtpSuggestionGeneratorTest, GmailOtps_EmptyAccountEmailReturnsEmpty) {
  std::vector<OneTimeToken> otps = {
      OneTimeToken(OneTimeTokenType::kGmail, "123456", base::TimeTicks::Now(),
                   /*sender_address=*/""),
  };

  EXPECT_THAT(BuildOtpSuggestions(otps), IsEmpty());
}

TEST_F(OtpSuggestionGeneratorTest, BuildOtpSuggestions_MixedOneTimeTokens) {
  std::vector<OneTimeToken> tokens = {
      OneTimeToken(OneTimeTokenType::kSmsOtp, "111111", base::TimeTicks::Now()),
      OneTimeToken(OneTimeTokenType::kGmail, "222222", base::TimeTicks::Now(),
                   "sender1@example.com"),
      OneTimeToken(OneTimeTokenType::kGmail, "333333", base::TimeTicks::Now(),
                   "sender2@example.com"),
  };

  EXPECT_THAT(
      BuildOtpSuggestions(tokens),
      ElementsAre(IsSmsOtpSuggestion(u"111111"),
                  IsGmailOtpSuggestion(u"222222", u"sender1@example.com"),
                  IsGmailOtpSuggestion(u"333333", u"sender2@example.com"),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  IsOpenGmailForOtpsSuggestion()));
}

TEST_F(OtpSuggestionGeneratorTest, SmsOtps) {
  std::vector<OneTimeToken> otps = {
      OneTimeToken(OneTimeTokenType::kSmsOtp, "123456", base::TimeTicks::Now()),
      OneTimeToken(OneTimeTokenType::kSmsOtp, "789012", base::TimeTicks::Now()),
  };

  EXPECT_THAT(BuildOtpSuggestions(otps),
              ElementsAre(IsSmsOtpSuggestion(u"123456"),
                          IsSmsOtpSuggestion(u"789012")));
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

// Tests that an OTP that completes the value the user has typed into the
// trigger field is suggested.
TEST_F(OtpSuggestionGeneratorTest, GenerateOtpSuggestions_FieldValueIsPrefix) {
  FormData form =
      test::GetFormData({.fields = {{.role = ONE_TIME_CODE, .value = u"08"}}});
  FormStructure form_structure(form);
  form_structure.field(0)->SetTypeTo(AutofillType(ONE_TIME_CODE),
                                     /*source=*/std::nullopt);

  EXPECT_CALL(otp_manager(), GetOtpSuggestions)
      .WillOnce(RunOnceCallback<2>(std::vector<OneTimeToken>{
          OneTimeToken(OneTimeTokenType::kGmail, "083203",
                       base::TimeTicks::Now(), "sender@example.com")}));

  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  EXPECT_CALL(
      suggestions_generated_callback,
      Run(Pair(
          SuggestionGenerator::SuggestionDataSource::kOneTimePassword,
          ElementsAre(IsGmailOtpSuggestion(u"083203", u"sender@example.com"),
                      EqualsSuggestion(SuggestionType::kSeparator),
                      IsOpenGmailForOtpsSuggestion()))));

  generator().GenerateSuggestions(form, form.fields()[0], &form_structure,
                                  form_structure.field(0), client(),
                                  suggestions_generated_callback.Get());
}

// Tests that no OTP suggestions (including the "Open Gmail" footer) are
// generated if the available OTP contradicts the value the user is typing into
// the trigger field.
TEST_F(OtpSuggestionGeneratorTest,
       GenerateOtpSuggestions_FieldValueContradictsOtp) {
  FormData form =
      test::GetFormData({.fields = {{.role = ONE_TIME_CODE, .value = u"12"}}});
  FormStructure form_structure(form);
  form_structure.field(0)->SetTypeTo(AutofillType(ONE_TIME_CODE),
                                     /*source=*/std::nullopt);

  EXPECT_CALL(otp_manager(), GetOtpSuggestions)
      .WillOnce(RunOnceCallback<2>(std::vector<OneTimeToken>{
          OneTimeToken(OneTimeTokenType::kGmail, "083203",
                       base::TimeTicks::Now(), "sender@example.com")}));

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

// Tests that `GetOtpsToSuggest()` matches every OTP independently against the
// value of the trigger field.
TEST(GetOtpsToSuggestTest, MatchesEachOtpAgainstFieldValue) {
  FormFieldData field;
  field.set_value(u"2");
  EXPECT_THAT(GetOtpsToSuggest(
                  {OneTimeToken(OneTimeTokenType::kSmsOtp, "111111",
                                base::TimeTicks::Now()),
                   OneTimeToken(OneTimeTokenType::kGmail, "222222",
                                base::TimeTicks::Now(), "sender@example.com"),
                   OneTimeToken(OneTimeTokenType::kSmsOtp, "233333",
                                base::TimeTicks::Now())},
                  field),
              ElementsAre(Property(&OneTimeToken::value, "222222"),
                          Property(&OneTimeToken::value, "233333")));
}

struct GetOtpsToSuggestTestCase {
  std::u16string field_value;
  bool is_autofilled = false;
  std::string otp_value;
  bool expect_suggested = false;
};

class GetOtpsToSuggestParamTest
    : public TestWithParam<GetOtpsToSuggestTestCase> {};

// Tests whether `GetOtpsToSuggest()` keeps an OTP given the value and the
// autofill state of the trigger field.
TEST_P(GetOtpsToSuggestParamTest, MatchesOtpAgainstFieldValue) {
  const GetOtpsToSuggestTestCase& test_case = GetParam();
  FormFieldData field;
  field.set_value(test_case.field_value);
  field.set_is_autofilled_according_to_renderer(test_case.is_autofilled);

  std::vector<OneTimeToken> otps_to_suggest = GetOtpsToSuggest(
      {OneTimeToken(OneTimeTokenType::kSmsOtp, test_case.otp_value,
                    base::TimeTicks::Now())},
      field);

  if (test_case.expect_suggested) {
    EXPECT_THAT(otps_to_suggest, ElementsAre(Property(&OneTimeToken::value,
                                                      test_case.otp_value)));
  } else {
    EXPECT_THAT(otps_to_suggest, IsEmpty());
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GetOtpsToSuggestParamTest,
    Values(
        // An empty field is compatible with every OTP.
        GetOtpsToSuggestTestCase{.field_value = u"",
                                 .otp_value = "083203",
                                 .expect_suggested = true},
        // OTPs that complete the typed value are kept.
        GetOtpsToSuggestTestCase{.field_value = u"08",
                                 .otp_value = "083203",
                                 .expect_suggested = true},
        // OTPs that contradict the typed value are dropped.
        GetOtpsToSuggestTestCase{.field_value = u"12", .otp_value = "083203"},
        GetOtpsToSuggestTestCase{.field_value = u"0832031",
                                 .otp_value = "083203"},
        // OTPs equal to the field value are dropped.
        GetOtpsToSuggestTestCase{.field_value = u"083203",
                                 .otp_value = "083203"},
        GetOtpsToSuggestTestCase{.field_value = u"083-203",
                                 .otp_value = "083203"},
        // Whitespace, ASCII punctuation (e.g. input mask placeholders) and the
        // case of ASCII letters are ignored.
        GetOtpsToSuggestTestCase{.field_value = u" 083 2-0",
                                 .otp_value = "083203",
                                 .expect_suggested = true},
        GetOtpsToSuggestTestCase{.field_value = u"08__-___",
                                 .otp_value = "083203",
                                 .expect_suggested = true},
        GetOtpsToSuggestTestCase{.field_value = u"___-___",
                                 .otp_value = "083203",
                                 .expect_suggested = true},
        GetOtpsToSuggestTestCase{.field_value = u"ab1",
                                 .otp_value = "AB12CD",
                                 .expect_suggested = true},
        // Non-ASCII digits (here Arabic-Indic "12") are not ignored.
        GetOtpsToSuggestTestCase{.field_value = u"\u0661\u0662",
                                 .otp_value = "083203"},
        // On autofilled fields, OTPs other than the filled value are kept
        // regardless of the field value, so that e.g. a resent OTP can replace
        // the filled one.
        GetOtpsToSuggestTestCase{.field_value = u"111111",
                                 .is_autofilled = true,
                                 .otp_value = "222222",
                                 .expect_suggested = true},
        GetOtpsToSuggestTestCase{.field_value = u"222222",
                                 .is_autofilled = true,
                                 .otp_value = "222222"}));

}  // namespace
}  // namespace autofill
