// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/passkeys/passkey_suggestion_generator.h"

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/password_manager/mock_password_manager_delegate.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Return;
using ::testing::SaveArg;
using ReturnedSuggestions = SuggestionGenerator::ReturnedSuggestions;
using SuggestionDataSource = SuggestionGenerator::SuggestionDataSource;

class PasskeySuggestionGeneratorTest : public testing::Test {
 public:
  void SetUp() override {
    generator_ = std::make_unique<PasskeySuggestionGenerator>();
    form_ = test::CreateTestHybridSignUpFormData();
    test_autofill_client_.set_password_manager_delegate(
        std::make_unique<testing::NiceMock<MockPasswordManagerDelegate>>());
  }

  MockPasswordManagerDelegate& password_delegate() {
    return static_cast<MockPasswordManagerDelegate&>(
        *client().GetPasswordManagerDelegate(FieldGlobalId()));
  }

  PasskeySuggestionGenerator& generator() { return *generator_; }

  AutofillClient& client() { return test_autofill_client_; }

  FormData& form() { return form_; }

  const FormFieldData& field() { return form_.fields()[0]; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment test_environment_;
  TestAutofillClient test_autofill_client_;
  std::unique_ptr<PasskeySuggestionGenerator> generator_;
  FormData form_;
};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Ensure the hybrid suggestion provided by the password delegate is fetched
// and displayed on valid webauthn fields.
TEST_F(PasskeySuggestionGeneratorTest, FetchCreatesValidSuggestionForGenerate) {
  Suggestion suggestion(SuggestionType::kWebauthnSignInWithAnotherDevice);
  EXPECT_CALL(password_delegate(), GetWebauthnSignInWithAnotherDeviceSuggestion)
      .WillRepeatedly(Return(suggestion));

  base::MockOnceCallback<void(ReturnedSuggestions)> generate_cb;
  EXPECT_CALL(generate_cb, Run(Pair(SuggestionDataSource::kPasskey,
                                    ElementsAre(suggestion))));
  generator().GenerateSuggestions(form(), field(), /*form_structure=*/nullptr,
                                  /*trigger_autofill_field=*/nullptr, client(),
                                  generate_cb.Get());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Ensure the hybrid suggestion provided by the password delegate is fetched
// and not displayed on fields without webauthn annotation.
TEST_F(PasskeySuggestionGeneratorTest,
       NoSuggestionsOnFieldsWithoutWebauthnAnnotation) {
  form().set_fields({test::CreateTestFormField(
      "Email", "email", "", FormControlType::kInputEmail, "username")});

  // The delegate could be called but it's not important.
  ON_CALL(password_delegate(), GetWebauthnSignInWithAnotherDeviceSuggestion)
      .WillByDefault(
          Return(Suggestion(SuggestionType::kWebauthnSignInWithAnotherDevice)));

  base::MockOnceCallback<void(ReturnedSuggestions)> generate_cb;
  EXPECT_CALL(generate_cb,
              Run(Pair(SuggestionDataSource::kPasskey, IsEmpty())));
  generator().GenerateSuggestions(form(), field(), /*form_structure=*/nullptr,
                                  /*trigger_autofill_field=*/nullptr, client(),
                                  generate_cb.Get());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Ensure if no hybrid suggestion is provided by the password delegate, no
// no suggestion is fetched or generated.
TEST_F(PasskeySuggestionGeneratorTest,
       NoSuggestionFromPasswordManagerDelegate) {
  EXPECT_CALL(password_delegate(), GetWebauthnSignInWithAnotherDeviceSuggestion)
      .WillRepeatedly(Return(std::nullopt));

  base::MockOnceCallback<void(ReturnedSuggestions)> generate_cb;
  EXPECT_CALL(generate_cb,
              Run(Pair(SuggestionDataSource::kPasskey, IsEmpty())));
  generator().GenerateSuggestions(form(), field(), /*form_structure=*/nullptr,
                                  /*trigger_autofill_field=*/nullptr, client(),
                                  generate_cb.Get());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace
}  // namespace autofill
