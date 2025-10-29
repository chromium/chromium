// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/passkeys/passkey_suggestion_generator.h"

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/password_manager/mock_password_manager_delegate.h"
#include "components/autofill/core/browser/suggestions/passkeys/hybrid_passkey_availability.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/password_manager/core/browser/features/password_features.h"
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
using SuggestionData = SuggestionGenerator::SuggestionData;
using ReturnedSuggestions = SuggestionGenerator::ReturnedSuggestions;
using ProductSuggestionDataPair =
    std::pair<SuggestionGenerator::SuggestionDataSource,
              std::vector<SuggestionData>>;

class PasskeySuggestionGeneratorTest : public testing::Test {
 public:
  void SetUp() override {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    features_.InitAndEnableFeature(
        ::password_manager::features::
            kAutofillReintroduceHybridPasskeyDropdownItem);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    generator_ =
        std::make_unique<PasskeySuggestionGenerator>(password_delegate());
    form_ = test::CreateTestHybridSignUpFormData();
  }

  void DisableHybridEntryPoint() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    features_.Reset();
    features_.InitAndDisableFeature(
        password_manager::features::
            kAutofillReintroduceHybridPasskeyDropdownItem);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  }

  MockPasswordManagerDelegate& password_delegate() {
    return password_manager_delegate_;
  }

  PasskeySuggestionGenerator& generator() { return *generator_; }

  const AutofillClient& client() { return test_autofill_client_; }

  FormData& form() { return form_; }

  const FormFieldData& field() { return form_.fields()[0]; }

 private:
  base::test::ScopedFeatureList features_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment test_environment_;
  TestAutofillClient test_autofill_client_;
  MockPasswordManagerDelegate password_manager_delegate_;
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

  base::MockOnceCallback<void(ProductSuggestionDataPair)> fetch_cb;
  ProductSuggestionDataPair fetched_suggestions;
  EXPECT_CALL(fetch_cb, Run).WillOnce(SaveArg<0>(&fetched_suggestions));
  generator().FetchSuggestionData(form(), field(), nullptr, nullptr, client(),
                                  fetch_cb.Get());

  EXPECT_EQ(fetched_suggestions.first,
            SuggestionGenerator::SuggestionDataSource::kPasskey);
  ASSERT_EQ(fetched_suggestions.second.size(), 1u);
  const SuggestionData& data = fetched_suggestions.second[0];
  ASSERT_TRUE(std::holds_alternative<HybridPasskeyAvailability>(data));
  EXPECT_TRUE(std::get<HybridPasskeyAvailability>(data).value());

  base::MockOnceCallback<void(ReturnedSuggestions)> generate_cb;
  EXPECT_CALL(generate_cb,
              Run(Pair(FillingProduct::kPasskey, ElementsAre(suggestion))));
  generator().GenerateSuggestions(form(), field(), nullptr, nullptr, client(),
                                  {{fetched_suggestions}}, generate_cb.Get());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(PasskeySuggestionGeneratorTest, NoHybridPasskeyAvailability) {
  DisableHybridEntryPoint();

  // Neither fetch nor generate should call into the delegate now.
  EXPECT_CALL(password_delegate(), GetWebauthnSignInWithAnotherDeviceSuggestion)
      .Times(0);

  base::MockOnceCallback<void(ProductSuggestionDataPair)> fetch_cb;
  ProductSuggestionDataPair fetched_suggestions;
  EXPECT_CALL(fetch_cb, Run).WillOnce(SaveArg<0>(&fetched_suggestions));
  generator().FetchSuggestionData(form(), field(), nullptr, nullptr, client(),
                                  fetch_cb.Get());

  // Fetch still calls the callback to report no suggestion will be generated.
  EXPECT_EQ(fetched_suggestions.first,
            SuggestionGenerator::SuggestionDataSource::kPasskey);
  EXPECT_EQ(fetched_suggestions.second.size(), 0u);

  base::MockOnceCallback<void(ReturnedSuggestions)> generate_cb;
  EXPECT_CALL(generate_cb, Run(Pair(FillingProduct::kPasskey, IsEmpty())));
  generator().GenerateSuggestions(form(), field(), nullptr, nullptr, client(),
                                  {{fetched_suggestions}}, generate_cb.Get());
}

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

  base::MockOnceCallback<void(ProductSuggestionDataPair)> fetch_cb;
  ProductSuggestionDataPair fetched_suggestions;
  EXPECT_CALL(fetch_cb, Run).WillOnce(SaveArg<0>(&fetched_suggestions));
  generator().FetchSuggestionData(form(), field(), nullptr, nullptr, client(),
                                  fetch_cb.Get());

  // Fetch still calls the callback to report no suggestion will be generated.
  EXPECT_EQ(fetched_suggestions.first,
            SuggestionGenerator::SuggestionDataSource::kPasskey);
  EXPECT_EQ(fetched_suggestions.second.size(), 0u);

  base::MockOnceCallback<void(ReturnedSuggestions)> generate_cb;
  EXPECT_CALL(generate_cb, Run(Pair(FillingProduct::kPasskey, IsEmpty())));
  generator().GenerateSuggestions(form(), field(), nullptr, nullptr, client(),
                                  {{fetched_suggestions}}, generate_cb.Get());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Ensure if no hybrid suggestion is provided by the password delegate, no
// no suggestion is fetched or generated.
TEST_F(PasskeySuggestionGeneratorTest,
       NoSuggestionFromPasswordManagerDelegate) {
  EXPECT_CALL(password_delegate(), GetWebauthnSignInWithAnotherDeviceSuggestion)
      .WillRepeatedly(Return(std::nullopt));

  base::MockOnceCallback<void(ProductSuggestionDataPair)> fetch_cb;
  ProductSuggestionDataPair fetched_suggestions;
  EXPECT_CALL(fetch_cb, Run).WillOnce(SaveArg<0>(&fetched_suggestions));
  generator().FetchSuggestionData(form(), field(), nullptr, nullptr, client(),
                                  fetch_cb.Get());

  // Fetch still calls the callback to report no suggestion will be generated.
  EXPECT_EQ(fetched_suggestions.first,
            SuggestionGenerator::SuggestionDataSource::kPasskey);
  EXPECT_EQ(fetched_suggestions.second.size(), 0u);

  base::MockOnceCallback<void(ReturnedSuggestions)> generate_cb;
  EXPECT_CALL(generate_cb, Run(Pair(FillingProduct::kPasskey, IsEmpty())));
  generator().GenerateSuggestions(form(), field(), nullptr, nullptr, client(),
                                  {{fetched_suggestions}}, generate_cb.Get());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Ensure that a fetch step always reports correctly whether a suggestion is
// generated. This ensures that other generators can reliably tell whether a
// hybrid entry point will be present or not.
TEST_F(PasskeySuggestionGeneratorTest,
       NoUpdateInGenerateIfFetchCreatesNoSuggestion) {
  // Simulate that a suggestion *could* be generated now ...
  Suggestion suggestion(SuggestionType::kWebauthnSignInWithAnotherDevice);
  EXPECT_CALL(password_delegate(), GetWebauthnSignInWithAnotherDeviceSuggestion)
      .WillRepeatedly(Return(suggestion));

  // ... but ensure it's not used because the fetch step didn't have access yet.
  base::MockOnceCallback<void(ReturnedSuggestions)> generate_cb;
  EXPECT_CALL(generate_cb, Run(Pair(FillingProduct::kPasskey, IsEmpty())));
  generator().GenerateSuggestions(
      form(), field(), nullptr, nullptr, client(),
      {{SuggestionGenerator::SuggestionDataSource::kPasskey, {}}},
      generate_cb.Get());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace
}  // namespace autofill
