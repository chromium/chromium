// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/compose_suggestion_generator.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/compose/mock_autofill_compose_delegate.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::Field;

class ComposeSuggestionGeneratorTest : public testing::Test {
 protected:
  ComposeSuggestionGeneratorTest() {
    FormData form_data;
    form_data.set_fields(
        {test::CreateTestFormField(/*label=*/"", "Some Field Name",
                                   "SomePrefix", FormControlType::kTextArea)});
    form_data.set_main_frame_origin(
        url::Origin::Create(GURL("https://www.example.com")));
    form_structure_ = std::make_unique<FormStructure>(form_data);
    autofill_field_ = form_structure_->field(0);
  }

  TestAutofillClient& client() { return autofill_client_; }
  AutofillField& field() { return *autofill_field_; }
  FormStructure& form() { return *form_structure_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<FormStructure> form_structure_;
  // Owned by `form_structure_`.
  raw_ptr<AutofillField> autofill_field_ = nullptr;
};

// Checks that compose suggestion is generated.
TEST_F(ComposeSuggestionGeneratorTest, GeneratesComposeSuggestion) {
  base::MockCallback<base::OnceCallback<void(
      std::pair<SuggestionGenerator::SuggestionDataSource,
                std::vector<SuggestionGenerator::SuggestionData>>)>>
      suggestion_data_callback;
  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  MockAutofillComposeDelegate compose_delegate;
  EXPECT_CALL(compose_delegate, GetSuggestion)
      .WillOnce(
          testing::Return(Suggestion(SuggestionType::kComposeProactiveNudge)));

  ComposeSuggestionGenerator generator(
      &compose_delegate,
      AutofillSuggestionTriggerSource::kTextFieldValueChanged);
  std::pair<SuggestionGenerator::SuggestionDataSource,
            std::vector<SuggestionGenerator::SuggestionData>>
      savedCallbackArgument;

  EXPECT_CALL(
      suggestion_data_callback,
      Run(testing::Pair(SuggestionGenerator::SuggestionDataSource::kCompose,
                        testing::IsEmpty())))
      .WillOnce(testing::SaveArg<0>(&savedCallbackArgument));
  generator.FetchSuggestionData(form().ToFormData(), field(), &form(), &field(),
                                client(), suggestion_data_callback.Get());

  EXPECT_CALL(suggestions_generated_callback,
              Run(testing::Pair(FillingProduct::kCompose, testing::SizeIs(1))));
  generator.GenerateSuggestions(form().ToFormData(), field(), &form(), &field(),
                                {savedCallbackArgument},
                                suggestions_generated_callback.Get());
}

// Checks that no compose suggestion is generated, if other products have some
// suggestions to offer.
TEST_F(ComposeSuggestionGeneratorTest,
       NoComposeSuggestionIfOtherSuggestionsAvailable) {
  base::MockCallback<base::OnceCallback<void(
      std::pair<SuggestionGenerator::SuggestionDataSource,
                std::vector<SuggestionGenerator::SuggestionData>>)>>
      suggestion_data_callback;
  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  MockAutofillComposeDelegate compose_delegate;
  EXPECT_CALL(compose_delegate, GetSuggestion).Times(0);

  ComposeSuggestionGenerator generator(
      &compose_delegate,
      AutofillSuggestionTriggerSource::kTextFieldValueChanged);
  std::pair<SuggestionGenerator::SuggestionDataSource,
            std::vector<SuggestionGenerator::SuggestionData>>
      other_generated_suggestion_data{
          SuggestionGenerator::SuggestionDataSource::kAutocomplete,
          {SuggestionGenerator::SuggestionData(AutocompleteEntry())}};

  EXPECT_CALL(
      suggestion_data_callback,
      Run(testing::Pair(SuggestionGenerator::SuggestionDataSource::kCompose,
                        testing::IsEmpty())));
  generator.FetchSuggestionData(form().ToFormData(), field(), &form(), &field(),
                                client(), suggestion_data_callback.Get());

  EXPECT_CALL(suggestions_generated_callback,
              Run(testing::Pair(FillingProduct::kCompose, testing::IsEmpty())));
  generator.GenerateSuggestions(form().ToFormData(), field(), &form(), &field(),
                                {other_generated_suggestion_data},
                                suggestions_generated_callback.Get());
}

}  // namespace
}  // namespace autofill
