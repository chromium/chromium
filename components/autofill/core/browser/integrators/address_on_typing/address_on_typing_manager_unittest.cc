// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/address_on_typing/address_on_typing_manager.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/address_on_typing/address_on_typing_manager_test_api.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/strike_databases/addresses/address_on_typing_suggestion_strike_database.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/strike_database/test_inmemory_strike_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

const std::string kDefaultProfileGuid =
    base::Uuid::GenerateRandomV4().AsLowercaseString();

AutofillProfile CreateTestProfile() {
  AutofillProfile profile = test::GetFullProfile();
  profile.set_guid(kDefaultProfileGuid);
  return profile;
}

class AddressOnTypingManagerTest : public testing::Test {
 public:
  AddressOnTypingManagerTest() {
    std::unique_ptr<TestStrikeDatabase> test_strike_database =
        std::make_unique<TestStrikeDatabase>();
    test_strike_database_ = test_strike_database.get();
    test_autofill_client_.set_test_strike_database(
        std::move(test_strike_database));

    manager_ = std::make_unique<AddressOnTypingManager>(test_autofill_client_);
  }

  AddressOnTypingManager& manager() { return *manager_; }

  AddressDataManager& address_data_manager() {
    return client().GetPersonalDataManager().address_data_manager();
  }

  std::vector<Suggestion> CreateAutofillOnTypingSuggestions(
      std::vector<FieldType> field_types_used) {
    std::vector<Suggestion> suggestions;
    for (FieldType type : field_types_used) {
      Suggestion suggestion(SuggestionType::kAddressEntryOnTyping);
      suggestion.field_by_field_filling_type_used = type;
      suggestion.payload = Suggestion::AutofillProfilePayload(
          Suggestion::Guid(kDefaultProfileGuid));
      suggestions.push_back(std::move(suggestion));
    }
    return suggestions;
  }

  TestAutofillClient& client() { return test_autofill_client_; }

  void KillAndRecreateManager() {
    manager_.reset();
    manager_ = std::make_unique<AddressOnTypingManager>(test_autofill_client_);
  }

 private:
  base::test::ScopedFeatureList features_{
      features::kAutofillAddressSuggestionsOnTypingHasStrikeDatabase};
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_env_;
  // Owned by `test_autofill_client_`.
  TestAutofillClient test_autofill_client_;
  raw_ptr<TestStrikeDatabase> test_strike_database_;
  std::unique_ptr<AddressOnTypingManager> manager_;
};

// Tests that suggestions that were not accepted have their types included to
// the strike database, conversely accepting a suggestion removes them.
TEST_F(AddressOnTypingManagerTest,
       StrikeDatabase_AcceptingSuggestion_ClearRespectiveTypes) {
  client().GetPersonalDataManager().test_address_data_manager().AddProfile(
      CreateTestProfile());
  client().SetAutofillSuggestions(CreateAutofillOnTypingSuggestions(
      {EMAIL_ADDRESS, NAME_FULL, ADDRESS_HOME_LINE1}));

  AutofillField autofill_field;
  FieldGlobalId field_id = test::MakeFieldGlobalId();
  manager().OnDidShowAddressOnTyping(field_id, &autofill_field);
  // Accept the suggestion built using `ADDRESS_HOME_LINE1`.
  manager().OnDidAcceptAddressOnTyping(field_id, u"P sherman",
                                       ADDRESS_HOME_LINE1, kDefaultProfileGuid);

  // Strikes are added at destruction time.
  KillAndRecreateManager();
  EXPECT_EQ(
      test_api(manager()).GetAddressOnTypingFieldTypeStrikes(EMAIL_ADDRESS), 1);
  EXPECT_EQ(test_api(manager()).GetAddressOnTypingFieldTypeStrikes(NAME_FULL),
            1);
  // Note the suggestion built using `ADDRESS_HOME_LINE1` was accepted.
  EXPECT_EQ(test_api(manager()).GetAddressOnTypingFieldTypeStrikes(
                ADDRESS_HOME_LINE1),
            0);

  // Now accept an `EMAIL_ADDRESS` suggestion, which should clear the strike
  // database for it.
  manager().OnDidShowAddressOnTyping(field_id, &autofill_field);
  manager().OnDidAcceptAddressOnTyping(field_id, u"dory@gmail.com",
                                       EMAIL_ADDRESS, kDefaultProfileGuid);
  EXPECT_EQ(
      test_api(manager()).GetAddressOnTypingFieldTypeStrikes(EMAIL_ADDRESS), 0);
}

TEST_F(AddressOnTypingManagerTest, StrikeLimitReached_MetricEmitted) {
  base::HistogramTester histogram_tester;

  client().GetPersonalDataManager().test_address_data_manager().AddProfile(
      CreateTestProfile());
  client().SetAutofillSuggestions(
      CreateAutofillOnTypingSuggestions({EMAIL_ADDRESS}));
  AutofillField autofill_field;
  FieldGlobalId field_id = test::MakeFieldGlobalId();

  // Show and decline suggestions enough times.
  for (int i = 0;
       i < *test_api(manager()).GetAddressOnTypingMaxStrikesLimit() - 1; i++) {
    // Show a suggestion for EMAIL_ADDRESS, but don't accept it.
    manager().OnDidShowAddressOnTyping(field_id, &autofill_field);
    KillAndRecreateManager();
    histogram_tester.ExpectBucketCount(
        "Autofill.AddressSuggestionOnTypingFieldTypeAddedToStrikeDatabase",
        EMAIL_ADDRESS, 0);
  }

  manager().OnDidShowAddressOnTyping(field_id, &autofill_field);
  KillAndRecreateManager();
  histogram_tester.ExpectBucketCount(
      "Autofill.AddressSuggestionOnTypingFieldTypeAddedToStrikeDatabase",
      EMAIL_ADDRESS, 1);
}

TEST_F(AddressOnTypingManagerTest, EmitMetrics) {
  base::HistogramTester histogram_tester;
  FormData form =
      test::GetFormData({.fields = {{.role = ADDRESS_HOME_LINE1,
                                     .autocomplete_attribute = "address-line1"},
                                    {.role = ADDRESS_HOME_LINE1,
                                     .autocomplete_attribute = "address-line1"},
                                    {}}});
  AutofillProfile profile = CreateTestProfile();
  constexpr size_t kProfileLastUsedInDays = 2u;
  profile.usage_history().set_use_date(base::Time::Now() -
                                       base::Days(kProfileLastUsedInDays));
  client().GetPersonalDataManager().test_address_data_manager().AddProfile(
      profile);
  // See, accept and do not correct the first suggestion.
  client().SetAutofillSuggestions(
      CreateAutofillOnTypingSuggestions({NAME_FULL}));
  AutofillField autofill_field_1 = AutofillField(form.fields()[0]);
  autofill_field_1.SetTypeTo(AutofillType(ADDRESS_HOME_LINE1),
                             AutofillPredictionSource::kHeuristics);
  manager().OnDidShowAddressOnTyping(form.fields()[0].global_id(),
                                     &autofill_field_1);
  const std::u16string filled_value = u"Jon snow";
  manager().OnDidAcceptAddressOnTyping(form.fields()[0].global_id(),
                                       filled_value, NAME_FULL,
                                       kDefaultProfileGuid);

  std::vector<FormFieldData> form_fields = form.ExtractFields();
  form_fields[0].set_value(filled_value);
  form.set_fields(std::move(form_fields));

  // Only see second suggestion.
  client().SetAutofillSuggestions(
      CreateAutofillOnTypingSuggestions({NAME_FIRST}));
  AutofillField autofill_field_2 = AutofillField(form.fields()[1]);
  autofill_field_2.SetTypeTo(AutofillType(ADDRESS_HOME_LINE1),
                             AutofillPredictionSource::kHeuristics);
  manager().OnDidShowAddressOnTyping(form.fields()[1].global_id(),
                                     &autofill_field_2);

  // See, accept and edit the third suggestion. Note the triggering field is
  // unclassified.
  client().SetAutofillSuggestions(
      CreateAutofillOnTypingSuggestions({NAME_FULL}));
  AutofillField autofill_field_3 = AutofillField(form.fields()[2]);
  manager().OnDidShowAddressOnTyping(form.fields()[2].global_id(),
                                     &autofill_field_3);
  manager().OnDidAcceptAddressOnTyping(form.fields()[2].global_id(),
                                       filled_value, NAME_FULL,
                                       kDefaultProfileGuid);
  form_fields = form.ExtractFields();
  // Set the third field value as something different from what was autofilled,
  // simulating a correction.
  form_fields[2].set_value(u"Jon snowy");
  form.set_fields(std::move(form_fields));

  manager().LogAddressOnTypingCorrectnessMetrics(FormStructure(form));
  // Some metrics are emitted during destruction.
  KillAndRecreateManager();

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.AddressSuggestionOnTypingAcceptance.Any"),
              BucketsAre(base::Bucket(false, 1), base::Bucket(true, 2)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.AddressSuggestionOnTypingAcceptance.Classified"),
              BucketsAre(base::Bucket(false, 1), base::Bucket(true, 1)));
  // Note that the third field in `form` is unclassified.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.AddressSuggestionOnTypingAcceptance.Unclassified"),
              BucketsAre(base::Bucket(false, 0), base::Bucket(true, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.AddressSuggestionOnTypingAcceptance.PerFieldType"),
      BucketsAre(
          base::Bucket(
              autofill_metrics::GetBucketForAcceptanceMetricsGroupedByFieldType(
                  NAME_FIRST, /*suggestion_accepted=*/false),
              1),
          base::Bucket(
              autofill_metrics::GetBucketForAcceptanceMetricsGroupedByFieldType(
                  NAME_FULL, /*suggestion_accepted=*/true),
              2)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.EditedAutofilledFieldAtSubmission.AddressOnTyping"),
              BucketsAre(base::Bucket(false, 1), base::Bucket(true, 1)));
  // One field was accepted without correction (first bucket), another field was
  // edited to a string that has 1 character distance. "Jon snow" vs "Jonsnowy"
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.EditedDistanceAutofilledFieldAtSubmission.AddressOnTyping"),
      BucketsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
  // Similar to the metric above, however measuring percentage values.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.EditedPercentageAutofilledFieldAtSubmission."
                  "AddressOnTyping"),
              BucketsAre(base::Bucket(0, 1), base::Bucket(12, 1)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.AddressSuggestionOnTypingShown.DaysSinceLastUse.Profile",
      kProfileLastUsedInDays, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.AddressSuggestionOnTypingAccepted.DaysSinceLastUse.Profile",
      kProfileLastUsedInDays, 1);
}

}  // namespace

}  // namespace autofill
