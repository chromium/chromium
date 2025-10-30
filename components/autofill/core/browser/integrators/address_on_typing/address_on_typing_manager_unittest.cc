// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/address_on_typing/address_on_typing_manager.h"

#include <memory>

#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/strike_databases/addresses/address_on_typing_suggestion_strike_database.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/strike_database/test_inmemory_strike_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// ID of the dummy profile used for filling in tests.
const std::string kGuid = base::Uuid::GenerateRandomV4().AsLowercaseString();

class AddressOnTypingManagerTest : public testing::Test {
 public:
  AddressOnTypingManagerTest() {
    test_strike_database_ =
        std::make_unique<strike_database::TestInMemoryStrikeDatabase>();
    strike_database_ =
        std::make_unique<AddressOnTypingSuggestionStrikeDatabase>(
            test_strike_database_.get());
    manager_ = std::make_unique<AddressOnTypingManager>(strike_database_.get());
  }

  AddressOnTypingManager& manager() { return *manager_; }

  AddressOnTypingSuggestionStrikeDatabase& strike_database() {
    return *strike_database_;
  }

  void KillAndRecreateManager() {
    manager_.reset();
    manager_ = std::make_unique<AddressOnTypingManager>(strike_database_.get());
  }

 private:
  test::AutofillUnitTestEnvironment autofill_test_env_;
  std::unique_ptr<strike_database::TestInMemoryStrikeDatabase>
      test_strike_database_;
  std::unique_ptr<AddressOnTypingSuggestionStrikeDatabase> strike_database_;
  std::unique_ptr<AddressOnTypingManager> manager_;
};

// Tests that suggestions that were not accepted have their types included to
// the strike database, conversely accepting a suggestion removes them.
TEST_F(AddressOnTypingManagerTest,
       StrikeDatabase_AcceptingSuggestion_ClearRespectiveTypes) {
  FieldGlobalId field = test::MakeFieldGlobalId();
  manager().OnDidShowAddressOnTyping(
      field, {EMAIL_ADDRESS, NAME_FULL, ADDRESS_HOME_LINE1},
      /*triggering_field_types=*/{},
      /*profile_last_used_time_per_guid=*/{{kGuid, base::Days(1)}});
  // Accept the suggestion built using `ADDRESS_HOME_LINE1`.
  manager().OnDidAcceptAddressOnTyping(field, u"P sherman", ADDRESS_HOME_LINE1,
                                       kGuid);

  // Strikes are added at destruction time.
  KillAndRecreateManager();
  EXPECT_EQ(strike_database().GetStrikes(base::NumberToString(EMAIL_ADDRESS)),
            1);
  EXPECT_EQ(strike_database().GetStrikes(base::NumberToString(NAME_FULL)), 1);
  // Note the suggestion built using `ADDRESS_HOME_LINE1` was accepted.
  EXPECT_EQ(
      strike_database().GetStrikes(base::NumberToString(ADDRESS_HOME_LINE1)),
      0);

  // Now accept an `EMAIL_ADDRESS` suggestion, which should clear the strike
  // database for it.
  manager().OnDidShowAddressOnTyping(
      field, {EMAIL_ADDRESS}, /*triggering_field_types=*/{},
      /*profile_last_used_time_per_guid=*/{{kGuid, base::Days(1)}});
  manager().OnDidAcceptAddressOnTyping(field, u"dory@gmail.com", EMAIL_ADDRESS,
                                       kGuid);
  EXPECT_EQ(strike_database().GetStrikes(base::NumberToString(EMAIL_ADDRESS)),
            0);
}

TEST_F(AddressOnTypingManagerTest, StrikeLimitReached_MetricEmitted) {
  base::HistogramTester histogram_tester;
  FieldGlobalId field = test::MakeFieldGlobalId();

  // Show and decline suggestions enough times.
  for (int i = 0; i < strike_database().GetMaxStrikesLimit() - 1; i++) {
    // Show a suggestion for EMAIL_ADDRESS, but don't accept it.
    manager().OnDidShowAddressOnTyping(
        field, {EMAIL_ADDRESS}, /*triggering_field_types=*/{},
        /*profile_last_used_time_per_guid=*/{{kGuid, base::Days(1)}});
    KillAndRecreateManager();
    histogram_tester.ExpectBucketCount(
        "Autofill.AddressSuggestionOnTypingFieldTypeAddedToStrikeDatabase",
        EMAIL_ADDRESS, 0);
  }

  manager().OnDidShowAddressOnTyping(
      field, {EMAIL_ADDRESS}, /*triggering_field_types=*/{},
      /*profile_last_used_time_per_guid=*/{{kGuid, base::Days(1)}});
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
  constexpr size_t kProfileLastUsedInDays = 2u;
  // See, accept and do not correct the first suggestion.
  manager().OnDidShowAddressOnTyping(
      form.fields()[0].global_id(), {NAME_FULL}, {ADDRESS_HOME_LINE1},
      {{kGuid, base::Days(kProfileLastUsedInDays)}});
  const std::u16string filled_value = u"Jon snow";
  manager().OnDidAcceptAddressOnTyping(form.fields()[0].global_id(),
                                       filled_value, NAME_FULL, kGuid);
  std::vector<FormFieldData> form_fields = form.ExtractFields();
  form_fields[0].set_value(filled_value);
  form.set_fields(std::move(form_fields));

  // Only see second suggestion.
  manager().OnDidShowAddressOnTyping(
      form.fields()[1].global_id(), {NAME_FIRST}, {ADDRESS_HOME_LINE1},
      {{kGuid, base::Days(kProfileLastUsedInDays)}});

  // See, accept and edit the third suggestion. Note the triggering field is
  // unclassified.
  manager().OnDidShowAddressOnTyping(
      form.fields()[2].global_id(), {NAME_FULL}, {},
      {{kGuid, base::Days(kProfileLastUsedInDays)}});
  manager().OnDidAcceptAddressOnTyping(form.fields()[2].global_id(),
                                       filled_value, NAME_FULL, kGuid);
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
