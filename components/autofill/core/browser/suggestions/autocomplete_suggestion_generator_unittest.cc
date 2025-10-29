// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/autocomplete_suggestion_generator.h"

#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_service.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {
constexpr int kDbQueryId = 100;

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::IsTrue;
using DbCallback = base::OnceCallback<void(WebDataServiceBase::Handle,
                                           std::unique_ptr<WDTypedResult>)>;

auto HasSingleSuggestionWithMainText(std::u16string text) {
  return Field(&Suggestion::main_text,
               AllOf(Field(&Suggestion::Text::value, text),
                     Field(&Suggestion::Text::is_primary, IsTrue())));
}

class AutocompleteSuggestionGeneratorTest : public testing::Test {
 protected:
  AutocompleteSuggestionGeneratorTest() {
    web_data_service_ = base::MakeRefCounted<MockAutofillWebDataService>();
    generator_ =
        std::make_unique<AutocompleteSuggestionGenerator>(web_data_service_);
  }

  AutofillClient& client() { return autofill_client_; }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  AutocompleteEntry GetAutocompleteEntry(
      const std::u16string& name,
      const std::u16string& value,
      base::Time date_created = base::Time::Now(),
      base::Time date_last_used = base::Time::Now()) {
    return AutocompleteEntry(AutocompleteKey(name, value), date_created,
                             date_last_used);
  }
  scoped_refptr<MockAutofillWebDataService> web_data_service() {
    return web_data_service_;
  }
  AutocompleteSuggestionGenerator& generator() { return *generator_; }

 private:
  std::unique_ptr<AutocompleteSuggestionGenerator> generator_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  scoped_refptr<MockAutofillWebDataService> web_data_service_;
  TestAutofillClient autofill_client_;
};

TEST_F(AutocompleteSuggestionGeneratorTest, GenerateAutocompleteSuggestions) {
  FormFieldData field_data =
      test::CreateTestFormField(/*label=*/"", "Some Field Name", "SomePrefix",
                                FormControlType::kInputText);
  FormData form_data;
  form_data.set_url(GURL("https://www.foo.com"));
  form_data.set_fields({field_data});

  std::vector<AutocompleteEntry> expected_values = {
      GetAutocompleteEntry(field_data.name(), u"SomePrefixOne"),
      GetAutocompleteEntry(field_data.name(), u"SomePrefixTwo")};
  std::unique_ptr<WDTypedResult> mocked_results =
      std::make_unique<WDResult<std::vector<AutocompleteEntry>>>(
          AUTOFILL_VALUE_RESULT, expected_values);

  base::MockCallback<base::OnceCallback<void(
      std::pair<SuggestionGenerator::SuggestionDataSource,
                std::vector<SuggestionGenerator::SuggestionData>>)>>
      suggestion_data_callback;
  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  std::pair<SuggestionGenerator::SuggestionDataSource,
            std::vector<SuggestionGenerator::SuggestionData>>
      saved_on_suggestion_data_returned_argument;
  SuggestionGenerator::ReturnedSuggestions
      saved_on_suggestions_generated_argument;

  EXPECT_CALL(
      *web_data_service(),
      GetFormValuesForElementName(field_data.name(), field_data.value(), _, _))
      .WillOnce([&](auto, auto, int, DbCallback callback) {
        task_environment().GetMainThreadTaskRunner()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), kDbQueryId,
                                      std::move(mocked_results)));
        return kDbQueryId;
      });

  EXPECT_CALL(suggestion_data_callback,
              Run(testing::Pair(
                  SuggestionGenerator::SuggestionDataSource::kAutocomplete,
                  testing::SizeIs(2))))
      .WillOnce(
          testing::SaveArg<0>(&saved_on_suggestion_data_returned_argument));
  generator().FetchSuggestionData(form_data, field_data,
                                  /*form_structure=*/nullptr,
                                  /*trigger_autofill_field=*/nullptr, client(),
                                  suggestion_data_callback.Get());
  EXPECT_TRUE(
      base::test::RunUntil([&saved_on_suggestion_data_returned_argument]() {
        return saved_on_suggestion_data_returned_argument.second.size() == 2;
      }));

  EXPECT_CALL(suggestions_generated_callback,
              Run(testing::Pair(
                  FillingProduct::kAutocomplete,
                  testing::UnorderedElementsAre(
                      HasSingleSuggestionWithMainText(u"SomePrefixOne"),
                      HasSingleSuggestionWithMainText(u"SomePrefixTwo")))))
      .WillOnce(testing::SaveArg<0>(&saved_on_suggestions_generated_argument));
  generator().GenerateSuggestions(form_data, field_data,
                                  /*form_structure=*/nullptr,
                                  /*trigger_autofill_field=*/nullptr, client(),
                                  {saved_on_suggestion_data_returned_argument},
                                  suggestions_generated_callback.Get());
  EXPECT_TRUE(
      base::test::RunUntil([&saved_on_suggestions_generated_argument]() {
        return saved_on_suggestions_generated_argument.second.size() == 2;
      }));
}

// Tests that AutocompleteSuggestionGenerator::OnAutofillValuesReturned does not
// crash on empty results.
TEST_F(AutocompleteSuggestionGeneratorTest, EmptyResult) {
  FormFieldData field_data =
      test::CreateTestFormField(/*label=*/"", "Some Field Name", "SomePrefix",
                                FormControlType::kInputText);
  FormData form_data;
  form_data.set_url(GURL("https://www.foo.com"));
  form_data.set_fields({field_data});

  base::MockCallback<base::OnceCallback<void(
      std::pair<SuggestionGenerator::SuggestionDataSource,
                std::vector<SuggestionGenerator::SuggestionData>>)>>
      suggestion_data_callback;
  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  std::pair<SuggestionGenerator::SuggestionDataSource,
            std::vector<SuggestionGenerator::SuggestionData>>
      saved_on_suggestion_data_returned_argument;
  SuggestionGenerator::ReturnedSuggestions
      saved_on_suggestions_generated_argument;

  EXPECT_CALL(
      *web_data_service(),
      GetFormValuesForElementName(field_data.name(), field_data.value(), _, _))
      .WillOnce([&](auto, auto, int, DbCallback callback) {
        task_environment().GetMainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), kDbQueryId, nullptr));
        return kDbQueryId;
      });

  EXPECT_CALL(suggestion_data_callback,
              Run(testing::Pair(
                  SuggestionGenerator::SuggestionDataSource::kAutocomplete,
                  testing::IsEmpty())))
      .WillOnce(
          testing::SaveArg<0>(&saved_on_suggestion_data_returned_argument));
  generator().FetchSuggestionData(form_data, field_data,
                                  /*form_structure=*/nullptr,
                                  /*trigger_autofill_field=*/nullptr, client(),
                                  suggestion_data_callback.Get());
  EXPECT_TRUE(
      base::test::RunUntil([&saved_on_suggestion_data_returned_argument]() {
        return saved_on_suggestion_data_returned_argument.first ==
               SuggestionGenerator::SuggestionDataSource::kAutocomplete;
      }));

  EXPECT_CALL(
      suggestions_generated_callback,
      Run(testing::Pair(FillingProduct::kAutocomplete, testing::IsEmpty())))
      .WillOnce(testing::SaveArg<0>(&saved_on_suggestions_generated_argument));
  generator().GenerateSuggestions(form_data, field_data,
                                  /*form_structure=*/nullptr,
                                  /*trigger_autofill_field=*/nullptr, client(),
                                  {saved_on_suggestion_data_returned_argument},
                                  suggestions_generated_callback.Get());
  EXPECT_TRUE(
      base::test::RunUntil([&saved_on_suggestions_generated_argument]() {
        return saved_on_suggestions_generated_argument.first ==
               FillingProduct::kAutocomplete;
      }));
}

// Tests that AutocompleteSuggestionGenerator does not generate suggestions for
// CreditCard-classified fields.
TEST_F(AutocompleteSuggestionGeneratorTest,
       CreditCardNumberField_NoSuggestions) {
  FormData form = test::CreateTestCreditCardFormData(/*is_https=*/false,
                                                     /*use_month_type=*/false);
  test_api(form).field(1).set_should_autocomplete(true);
  FormStructure form_structure(form);
  form_structure.field(1)->SetTypeTo(AutofillType(CREDIT_CARD_NUMBER),
                                     /*source=*/std::nullopt);

  base::MockCallback<base::OnceCallback<void(
      std::pair<SuggestionGenerator::SuggestionDataSource,
                std::vector<SuggestionGenerator::SuggestionData>>)>>
      suggestion_data_callback;

  EXPECT_CALL(*web_data_service(), GetFormValuesForElementName).Times(0);
  EXPECT_CALL(suggestion_data_callback,
              Run(testing::Pair(
                  SuggestionGenerator::SuggestionDataSource::kAutocomplete,
                  testing::IsEmpty())));
  generator().FetchSuggestionData(form, form.fields()[1], &form_structure,
                                  form_structure.field(1), client(),
                                  suggestion_data_callback.Get());
}

}  // namespace
}  // namespace autofill
