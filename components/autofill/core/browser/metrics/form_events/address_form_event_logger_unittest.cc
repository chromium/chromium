// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

class CategoryResolvedKeyMetricsTest : public AutofillMetricsBaseTest,
                                       public testing::Test {
 public:
  CategoryResolvedKeyMetricsTest() = default;

  void SetUp() override {
    SetUpHelper();
    personal_data().test_address_data_manager().ClearProfiles();
  }
  void TearDown() override { TearDownHelper(); }

  // Creates a full profile of the given `category` and adds it to the PDM.
  AutofillProfile CreateProfileOfCategory(
      AutofillProfileRecordTypeCategory category) {
    AutofillProfile profile = test::GetFullProfile();
    test::SetProfileCategory(profile, category);
    personal_data().address_data_manager().AddProfile(profile);
    return profile;
  }

  // Creates an arbitrary address form and triggers AutofillManager's
  // OnFormSeen() event.
  // TODO(crbug.com/40100455): Replace this with a modern form creation
  // function.
  FormData CreateAndSeeForm() {
    std::vector<FormFieldData> fields(3);
    for (FormFieldData& field : fields) {
      field.set_renderer_id(autofill_test_environment_.NextFieldRendererId());
    }
    FormData form = CreateEmptyForm();
    form.set_fields(std::move(fields));
    autofill_manager().AddSeenForm(
        form, {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, EMAIL_ADDRESS});
    SeeForm(form);
    return form;
  }

  // Fills the `form` using the `profile` by clicking into the first field. Only
  // profiles stored in the PDM can be used for filling.
  void FillFormWithProfile(const FormData& form,
                           const AutofillProfile& profile) {
    ASSERT_TRUE(personal_data().address_data_manager().GetProfileByGUID(
        profile.guid()));
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().front().global_id());
    autofill_manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, form,
                                         form.fields().front().global_id(),
                                         &profile,
                                         AutofillTriggerSource::kPopup);
  }

 protected:
  base::HistogramTester histogram_tester_;
};

// Tests that when Autofill is not used, the assistance metric is emitted as
// kNone, but not the correctness metric is not.
TEST_F(CategoryResolvedKeyMetricsTest, NoAutofill) {
  FormData form = CreateAndSeeForm();
  autofill_manager().OnAskForValuesToFillTest(
      form, form.fields().front().global_id());
  SubmitForm(form);

  ResetDriverToCommitMetrics();
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Leipzig.FillingAssistanceCategory",
      CategoryResolvedKeyMetricBucket::kNone, 1);
  // FillingCorrectness is only emitted when Autofill was used.
  histogram_tester_.ExpectTotalCount(
      "Autofill.Leipzig.FillingCorrectness.Legacy", 0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Leipzig.FillingCorrectness.AccountChrome", 0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Leipzig.FillingCorrectness.AccountNonChrome", 0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Leipzig.FillingCorrectness.Mixed", 0);
}

// Parameterized CategoryResolvedKeyMetricsTest that edits a field depending on
// the parameter. This is used to test the correctness metric, which depends on
// whether autofilled fields have been edited.
// Additionally, these tests verify that the category-resolved assistance and
// readiness metrics are correctly emitted.
class CategoryResolvedKeyMetricsEditTest
    : public CategoryResolvedKeyMetricsTest,
      public testing::WithParamInterface<bool> {
 public:
  bool ShouldEditField() const { return GetParam(); }
};
INSTANTIATE_TEST_SUITE_P(CategoryResolvedKeyMetricsTest,
                         CategoryResolvedKeyMetricsEditTest,
                         testing::Bool());

TEST_P(CategoryResolvedKeyMetricsEditTest, kLocalOrSyncable) {
  FormData form = CreateAndSeeForm();
  FillFormWithProfile(form,
                      CreateProfileOfCategory(
                          AutofillProfileRecordTypeCategory::kLocalOrSyncable));
  if (ShouldEditField()) {
    SimulateUserChangedField(form, form.fields().front());
  }
  SubmitForm(form);

  ResetDriverToCommitMetrics();
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Leipzig.FillingReadinessCategory",
      CategoryResolvedKeyMetricBucket::kLocalOrSyncable, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Leipzig.FillingAssistanceCategory",
      CategoryResolvedKeyMetricBucket::kLocalOrSyncable, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Leipzig.FillingCorrectness.Legacy", !ShouldEditField(), 1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Leipzig.FillingCorrectness.AccountChrome", 0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Leipzig.FillingCorrectness.AccountNonChrome", 0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Leipzig.FillingCorrectness.Mixed", 0);
}

TEST_P(CategoryResolvedKeyMetricsEditTest, kAccountChrome) {
  FormData form = CreateAndSeeForm();
  FillFormWithProfile(form,
                      CreateProfileOfCategory(
                          AutofillProfileRecordTypeCategory::kAccountChrome));
  if (ShouldEditField()) {
    SimulateUserChangedField(form, form.fields().front());
  }
  SubmitForm(form);

  ResetDriverToCommitMetrics();
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Leipzig.FillingReadinessCategory",
      CategoryResolvedKeyMetricBucket::kAccountChrome, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Leipzig.FillingAssistanceCategory",
      CategoryResolvedKeyMetricBucket::kAccountChrome, 1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Leipzig.FillingCorrectness.Legacy", 0);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Leipzig.FillingCorrectness.AccountChrome", !ShouldEditField(),
      1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Leipzig.FillingCorrectness.AccountNonChrome", 0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Leipzig.FillingCorrectness.Mixed", 0);
}

TEST_P(CategoryResolvedKeyMetricsEditTest, kAccountNonChrome) {
  FormData form = CreateAndSeeForm();
  FillFormWithProfile(
      form, CreateProfileOfCategory(
                AutofillProfileRecordTypeCategory::kAccountNonChrome));
  if (ShouldEditField()) {
    SimulateUserChangedField(form, form.fields().front());
  }
  SubmitForm(form);

  ResetDriverToCommitMetrics();
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Leipzig.FillingReadinessCategory",
      CategoryResolvedKeyMetricBucket::kAccountNonChrome, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Leipzig.FillingAssistanceCategory",
      CategoryResolvedKeyMetricBucket::kAccountNonChrome, 1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Leipzig.FillingCorrectness.Legacy", 0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Leipzig.FillingCorrectness.AccountChrome", 0);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Leipzig.FillingCorrectness.AccountNonChrome",
      !ShouldEditField(), 1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Leipzig.FillingCorrectness.Mixed", 0);
}

// Tests that when profiles of different categories are used for filling,
// assistance is attributed to the kMixed bucket. Similarly, correctness is
// attributed to the mixed bucket, even if only profiles of a single category
// are edited.
// Note that key metrics are not emitted on form submit, but on navigation. From
// the loggers point of view, all fields are lumped together. For this reason
// two independent forms are can be used. This simplifies the filling logic in
// the unit tests.
TEST_P(CategoryResolvedKeyMetricsEditTest, Mixed) {
  FormData form1 = CreateAndSeeForm();
  FillFormWithProfile(form1,
                      CreateProfileOfCategory(
                          AutofillProfileRecordTypeCategory::kLocalOrSyncable));
  SubmitForm(form1);

  FormData form2 = CreateAndSeeForm();
  FillFormWithProfile(form2,
                      CreateProfileOfCategory(
                          AutofillProfileRecordTypeCategory::kAccountChrome));
  SubmitForm(form2);
  if (ShouldEditField()) {
    SimulateUserChangedField(form2, form2.fields().front());
  }

  ResetDriverToCommitMetrics();
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Leipzig.FillingReadinessCategory",
      CategoryResolvedKeyMetricBucket::kMixed, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Leipzig.FillingAssistanceCategory",
      CategoryResolvedKeyMetricBucket::kMixed, 1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Leipzig.FillingCorrectness.Legacy", 0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Leipzig.FillingCorrectness.AccountChrome", 0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Leipzig.FillingCorrectness.AccountNonChrome", 0);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Leipzig.FillingCorrectness.Mixed", !ShouldEditField(), 1);
}

class AutofillAddressOnTypingMetricsTest : public AutofillMetricsBaseTest,
                                           public testing::Test {
 public:
  AutofillAddressOnTypingMetricsTest() = default;

  void SetUp() override { SetUpHelper(); }
  void TearDown() override { TearDownHelper(); }

  // Builds a vectors of `SuggestionType::kAddressEntryOnTyping` suggestions.
  // `field_types_used` is used to set the
  // `Suggestion::field_by_field_filling_type_used` property for each output
  // suggestion.
  std::vector<Suggestion> BuildAutofillOnTypingSuggestions(
      FieldTypeSet field_types_used,
      std::string profile_guid) {
    std::vector<Suggestion> suggestions;
    for (FieldType field_type : field_types_used) {
      suggestions.emplace_back(SuggestionType::kAddressEntryOnTyping);
      suggestions.back().field_by_field_filling_type_used = field_type;
      suggestions.back().payload =
          Suggestion::AutofillProfilePayload(Suggestion::Guid(profile_guid));
    }
    return suggestions;
  }
};

TEST_F(AutofillAddressOnTypingMetricsTest, EmitMetrics) {
  base::HistogramTester histogram_tester_;
  FormData form = test::GetFormData({.fields = {{}, {}, {}}});
  AutofillProfile profile = test::GetFullProfile();
  const base::Time now = base::Time::Now();
  constexpr size_t kProfileLastUsedInDays = 2u;
  profile.usage_history().set_use_date(now -
                                       base::Days(kProfileLastUsedInDays));
  personal_data().address_data_manager().AddProfile(profile);

  // Simulate that the autofill manager has seen this form on page load.
  SeeForm(form);
  std::vector<Suggestion> shown_suggestions = {
      Suggestion(SuggestionType::kAddressEntryOnTyping),
      Suggestion(SuggestionType::kSeparator),
      Suggestion(SuggestionType::kManageAddress)};

  // See, accept and do not correct the first suggestion.
  autofill_client().SetAutofillSuggestions(BuildAutofillOnTypingSuggestions(
      {NAME_FULL}, /*profile_guid=*/profile.guid()));
  autofill_manager().DidShowSuggestions(shown_suggestions, form,
                                        form.fields()[0].global_id(), {});
  const std::u16string filled_value = u"Jon snow";
  autofill_manager().OnDidFillAddressOnTypingSuggestion(
      form.fields()[0].global_id(), filled_value, NAME_FULL, profile.guid());
  std::vector<FormFieldData> form_fields = form.ExtractFields();
  // Note that the first field value has the same as the one from the first
  // suggestion.
  form_fields[0].set_value(filled_value);
  form.set_fields(std::move(form_fields));

  // Only see second suggestion.
  autofill_client().SetAutofillSuggestions(
      BuildAutofillOnTypingSuggestions({NAME_FIRST}, profile.guid()));
  autofill_manager().DidShowSuggestions(shown_suggestions, form,
                                        form.fields()[1].global_id(), {});

  // See, accept and edit the third suggestion.
  autofill_client().SetAutofillSuggestions(
      BuildAutofillOnTypingSuggestions({NAME_FULL}, profile.guid()));
  autofill_manager().DidShowSuggestions(shown_suggestions, form,
                                        form.fields()[2].global_id(), {});
  autofill_manager().OnDidFillAddressOnTypingSuggestion(
      form.fields()[2].global_id(), filled_value, NAME_FULL, profile.guid());
  form_fields = form.ExtractFields();
  // Set the third field value as something different from what was autofilled,
  // simulating a correction.
  form_fields[2].set_value(u"Jon snowy");
  form.set_fields(std::move(form_fields));

  SubmitForm(form);
  ResetDriverToCommitMetrics();
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Autofill.AddressSuggestionOnTypingAcceptance"),
              BucketsAre(base::Bucket(false, 1), base::Bucket(true, 2)));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Autofill.AddressSuggestionOnTypingAcceptance.PerFieldType"),
      BucketsAre(base::Bucket(GetBucketForAcceptanceMetricsGroupedByFieldType(
                                  NAME_FIRST, /*suggestion_accepted=*/false),
                              1),
                 base::Bucket(GetBucketForAcceptanceMetricsGroupedByFieldType(
                                  NAME_FULL, /*suggestion_accepted=*/true),
                              2)));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Autofill.EditedAutofilledFieldAtSubmission.AddressOnTyping"),
              BucketsAre(base::Bucket(false, 1), base::Bucket(true, 1)));
  // One field was accepted without correction (first bucket), another field was
  // edited to a string that has 1 character distance. "Jon snow" vs "Jon snowy"
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Autofill.EditedDistanceAutofilledFieldAtSubmission.AddressOnTyping"),
      BucketsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
  // Similar to the method above, however measuring percentage values.
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Autofill.EditedPercentageAutofilledFieldAtSubmission."
                  "AddressOnTyping"),
              BucketsAre(base::Bucket(0, 1), base::Bucket(12, 1)));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.AddressSuggestionOnTypingShown.DaysSinceLastUse.Profile",
      kProfileLastUsedInDays, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.AddressSuggestionOnTypingAccepted.DaysSinceLastUse.Profile",
      kProfileLastUsedInDays, 1);
}

}  // namespace autofill::autofill_metrics
