// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/autofill_ai/autofill_ai_suggestion_generator.h"

#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {
using enum SuggestionType;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;

auto HasType(SuggestionType expected_type) {
  return Field("Suggestion::type", &Suggestion::type, Eq(expected_type));
}

class AutofillAiSuggestionGeneratorTest : public testing::Test {
 protected:
  AutofillAiSuggestionGeneratorTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillAiWithDataSchema,
                              features::kAutofillAiNoTagTypes,
                              features::kAutofillAiServerModel},
        /*disabled_features=*/{});
    autofill_client_.GetPersonalDataManager().SetPrefService(
        autofill_client_.GetPrefs());
    autofill_client_.set_entity_data_manager(
        std::make_unique<EntityDataManager>(
            webdata_helper_.autofill_webdata_service(),
            /*history_service=*/nullptr,
            /*strike_database=*/nullptr));

    generator_ = std::make_unique<AutofillAiSuggestionGenerator>();
  }

  TestAutofillClient& client() { return autofill_client_; }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  AutofillAiSuggestionGenerator& generator() { return *generator_; }
  EntityDataManager& edm() { return *autofill_client_.GetEntityDataManager(); }
  AutofillWebDataServiceTestHelper& webdata_helper() { return webdata_helper_; }

  void SetForm(const std::vector<FieldType>& field_types) {
    test::FormDescription form_description;
    for (FieldType type : field_types) {
      form_description.fields.emplace_back(
          test::FieldDescription({.role = type}));
    }
    form_structure_.emplace(test::GetFormData(form_description));
    CHECK_EQ(field_types.size(), form_structure_->field_count());
    for (size_t i = 0; i < form_structure_->field_count(); i++) {
      form_structure_->field(i)->set_server_predictions({[&] {
        FieldPrediction prediction;
        prediction.set_type(field_types[i]);
        return prediction;
      }()});
    }
  }

  FormData form() { return form_structure_->ToFormData(); }
  FormFieldData& field_data() { return *form_structure_->fields()[0]; }
  FormStructure& form_structure() { return *form_structure_; }
  AutofillField& field() { return *form_structure_->fields()[0]; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AutofillAiSuggestionGenerator> generator_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  TestAutofillClient autofill_client_;
  std::optional<FormStructure> form_structure_;
};

TEST_F(AutofillAiSuggestionGeneratorTest, GeneratesAutofillAiSuggestions) {
  client().SetUpPrefsAndIdentityForAutofillAi();
  SetForm({PASSPORT_NUMBER});
  edm().AddOrUpdateEntityInstance(test::GetPassportEntityInstance());
  webdata_helper().WaitUntilIdle();

  base::MockCallback<base::OnceCallback<void(
      std::pair<FillingProduct,
                std::vector<SuggestionGenerator::SuggestionData>>)>>
      suggestion_data_callback;
  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  std::pair<FillingProduct, std::vector<SuggestionGenerator::SuggestionData>>
      saved_on_suggestion_data_returned_argument;
  SuggestionGenerator::ReturnedSuggestions
      saved_on_suggestions_generated_argument;

  EXPECT_CALL(
      suggestion_data_callback,
      Run(testing::Pair(FillingProduct::kAutofillAi, testing::SizeIs(1))))
      .WillOnce(
          testing::SaveArg<0>(&saved_on_suggestion_data_returned_argument));
  generator().FetchSuggestionData(form(), field_data(), &form_structure(),
                                  &field(), client(),
                                  suggestion_data_callback.Get());
  EXPECT_TRUE(
      base::test::RunUntil([&saved_on_suggestion_data_returned_argument]() {
        return !saved_on_suggestion_data_returned_argument.second.empty();
      }));

  EXPECT_CALL(suggestions_generated_callback,
              Run(testing::Pair(
                  FillingProduct::kAutofillAi,
                  ElementsAre(HasType(kFillAutofillAi), HasType(kSeparator),
                              HasType(kManageAutofillAi)))))
      .WillOnce(testing::SaveArg<0>(&saved_on_suggestions_generated_argument));
  generator().GenerateSuggestions(form(), field_data(), &form_structure(),
                                  &field(),
                                  {saved_on_suggestion_data_returned_argument},
                                  suggestions_generated_callback.Get());
  EXPECT_TRUE(
      base::test::RunUntil([&saved_on_suggestions_generated_argument]() {
        return !saved_on_suggestions_generated_argument.second.empty();
      }));
}

TEST_F(AutofillAiSuggestionGeneratorTest, NoSuggestionsIfNoEntities) {
  client().SetUpPrefsAndIdentityForAutofillAi();
  SetForm({PASSPORT_NUMBER});

  base::MockCallback<base::OnceCallback<void(
      std::pair<FillingProduct,
                std::vector<SuggestionGenerator::SuggestionData>>)>>
      suggestion_data_callback;
  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  std::pair<FillingProduct, std::vector<SuggestionGenerator::SuggestionData>>
      saved_on_suggestion_data_returned_argument;
  SuggestionGenerator::ReturnedSuggestions
      saved_on_suggestions_generated_argument;

  EXPECT_CALL(suggestion_data_callback,
              Run(testing::Pair(FillingProduct::kAutofillAi, IsEmpty())))
      .WillOnce(
          testing::SaveArg<0>(&saved_on_suggestion_data_returned_argument));
  generator().FetchSuggestionData(form(), field_data(), &form_structure(),
                                  &field(), client(),
                                  suggestion_data_callback.Get());
  EXPECT_TRUE(
      base::test::RunUntil([&saved_on_suggestion_data_returned_argument]() {
        return saved_on_suggestion_data_returned_argument.first ==
               FillingProduct::kAutofillAi;
      }));

  EXPECT_CALL(suggestions_generated_callback,
              Run(testing::Pair(FillingProduct::kAutofillAi, IsEmpty())))
      .WillOnce(testing::SaveArg<0>(&saved_on_suggestions_generated_argument));
  generator().GenerateSuggestions(form(), field_data(), &form_structure(),
                                  &field(),
                                  {saved_on_suggestion_data_returned_argument},
                                  suggestions_generated_callback.Get());
  EXPECT_TRUE(
      base::test::RunUntil([&saved_on_suggestions_generated_argument]() {
        return saved_on_suggestions_generated_argument.first ==
               FillingProduct::kAutofillAi;
      }));
}

}  // namespace
}  // namespace autofill
