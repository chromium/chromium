// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/profile_token_quality.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ObservationType = ProfileTokenQuality::ObservationType;
using testing::UnorderedElementsAre;

class ProfileTokenQualityTest : public testing::Test {
 public:
  ProfileTokenQualityTest() : bam_(&driver_, &client_) {}

  // Creates a form and registers it with the `bam_` as-if it had the given
  // `types` as predictions.
  FormData GetFormWithTypes(const std::vector<ServerFieldType>& types) {
    test::FormDescription form_description;
    for (ServerFieldType type : types) {
      form_description.fields.emplace_back(type);
    }
    FormData form_data = test::GetFormData(form_description);
    bam_.AddSeenForm(form_data, types);
    return form_data;
  }

  // Edits the value of field number `field_index` to `new_value` and notifies
  // the `bam_` about this change.
  void EditFieldValue(FormData& form,
                      size_t field_index,
                      std::u16string new_value) {
    FormFieldData& field = form.fields[field_index];
    field.value = std::move(new_value);
    bam_.OnTextFieldDidChange(form, field, gfx::RectF(),
                              AutofillTickClock::NowTicks());
  }

  // Fills the `form` with the `profile`, as-if autofilling was triggered from
  // the `triggering_field_index`-th field.
  void FillForm(const FormData& form,
                const AutofillProfile& profile,
                size_t triggering_field_index = 0) {
    bam_.FillProfileForm(profile, form, form.fields[triggering_field_index],
                         AutofillTriggerSource::kPopup);
  }

 protected:
  base::test::ScopedFeatureList feature_{
      features::kAutofillTrackProfileTokenQuality};
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillDriver driver_;
  TestAutofillClient client_;
  TestBrowserAutofillManager bam_;
  TestPersonalDataManager pdm_;
};

// Ensures that `ProfileTokenQualityTest` supports all supported types of
// `AutofillProfile`. In particular, this test ensures that whenever a new
// non-stored type is added, the map in `GetStoredTypeOf()` is updated
// accordingly. If the type is supposed to be stored, it should be added to
// `AutofillTable::GetStoredTypesForAutofillProfile()`.
TEST_F(ProfileTokenQualityTest, AllSupportedTypesHandled) {
  ServerFieldTypeSet supported_types;
  AutofillProfile profile;
  profile.GetSupportedTypes(&supported_types);
  ProfileTokenQuality quality(&profile);
  for (ServerFieldType type : supported_types) {
    // See comment above `GetStoredTypeOf()` why this type is special.
    if (type == ADDRESS_HOME_ADDRESS) {
      continue;
    }
    // `GetObservationTypesForFieldType()` will internally call
    // `GetStoredTypeOf()`. A `CHECK()` will fail if the mapping is incomplete.
    EXPECT_TRUE(quality.GetObservationTypesForFieldType(type).empty());
  }
}

TEST_F(ProfileTokenQualityTest, GetObservationTypesForFieldType) {
  AutofillProfile profile;
  ProfileTokenQuality quality(&profile);

  EXPECT_TRUE(quality.GetObservationTypesForFieldType(NAME_FIRST).empty());

  quality.AddObservationForTesting(NAME_FIRST, ObservationType::kAccepted);
  EXPECT_THAT(quality.GetObservationTypesForFieldType(NAME_FIRST),
              UnorderedElementsAre(ObservationType::kAccepted));
  EXPECT_TRUE(quality.GetObservationTypesForFieldType(NAME_LAST).empty());

  // Test that if more than `kMaxObservationsPerToken` observations are added,
  // only the first `kMaxObservationsPerToken` are returned.
  for (size_t i = 0; i < ProfileTokenQuality::kMaxObservationsPerToken; i++) {
    quality.AddObservationForTesting(NAME_FIRST,
                                     ObservationType::kEditedToSimilarValue);
  }
  EXPECT_THAT(quality.GetObservationTypesForFieldType(NAME_FIRST),
              testing::UnorderedElementsAreArray(std::vector<ObservationType>(
                  ProfileTokenQuality::kMaxObservationsPerToken,
                  ObservationType::kEditedToSimilarValue)));
}

// Tests that `AddObservationsForFilledForm()` derives the correct observation
// types when fields are not edited.
TEST_F(ProfileTokenQualityTest, AddObservationsForFilledForm_Accepted) {
  AutofillProfile profile = test::GetFullProfile();
  pdm_.AddProfile(profile);
  ProfileTokenQuality quality(&profile);
  quality.disable_randomization_for_testing();

  FormData form = GetFormWithTypes({NAME_FIRST, NAME_MIDDLE_INITIAL});
  FillForm(form, profile);
  // Accept field 0 as-is.
  // Accept field 1 as-is too. But since it has a derived type, it counts as
  // a partial accept for the middle name (its stored type).

  FormStructure* form_structure = bam_.FindCachedFormById(form.global_id());
  EXPECT_TRUE(
      quality.AddObservationsForFilledForm(*form_structure, form, pdm_));

  EXPECT_THAT(quality.GetObservationTypesForFieldType(NAME_FIRST),
              UnorderedElementsAre(ObservationType::kAccepted));
  EXPECT_THAT(quality.GetObservationTypesForFieldType(NAME_MIDDLE),
              UnorderedElementsAre(ObservationType::kPartiallyAccepted));
}

// Tests that `AddObservationsForFilledForm()` derives the correct observation
// types when fields are edited to values that don't occur in another profile.
TEST_F(ProfileTokenQualityTest, AddObservationsForFilledForm_Edited) {
  AutofillProfile profile = test::GetFullProfile();
  pdm_.AddProfile(profile);
  ProfileTokenQuality quality(&profile);
  quality.disable_randomization_for_testing();

  FormData form = GetFormWithTypes(
      {NAME_FIRST, NAME_LAST, ADDRESS_HOME_LINE1, ADDRESS_HOME_CITY});
  FillForm(form, profile);

  // Clear the value of field 0.
  EditFieldValue(form, 0, u"");
  // Edit field 1 to a different token of the same `profile`.
  EditFieldValue(form, 1, profile.GetInfo(NAME_MIDDLE, pdm_.app_locale()));
  // Edit field 2 to a value similar to the originally filled one.
  ASSERT_EQ(profile.GetInfo(ADDRESS_HOME_LINE1, pdm_.app_locale()),
            u"666 Erebus St.");
  EditFieldValue(form, 2, u"666 Erbus Str");
  // Edit field 3 to a completely different token.
  EditFieldValue(form, 3, u"different value");

  FormStructure* form_structure = bam_.FindCachedFormById(form.global_id());
  EXPECT_TRUE(
      quality.AddObservationsForFilledForm(*form_structure, form, pdm_));

  EXPECT_THAT(quality.GetObservationTypesForFieldType(NAME_FIRST),
              UnorderedElementsAre(ObservationType::kEditedValueCleared));
  EXPECT_THAT(quality.GetObservationTypesForFieldType(NAME_LAST),
              UnorderedElementsAre(
                  ObservationType::kEditedToDifferentTokenOfSameProfile));
  EXPECT_THAT(quality.GetObservationTypesForFieldType(ADDRESS_HOME_LINE1),
              UnorderedElementsAre(ObservationType::kEditedToSimilarValue));
  EXPECT_THAT(quality.GetObservationTypesForFieldType(ADDRESS_HOME_CITY),
              UnorderedElementsAre(ObservationType::kEditedFallback));
}

// Tests that `AddObservationsForFilledForm()` derives the correct observation
// types when fields are edited to values occurring in another profile.
TEST_F(ProfileTokenQualityTest,
       AddObservationsForFilledForm_Edited_DifferentProfile) {
  AutofillProfile profile = test::GetFullProfile();
  AutofillProfile other_profile = test::GetFullProfile2();
  pdm_.AddProfile(profile);
  pdm_.AddProfile(other_profile);
  ProfileTokenQuality quality(&profile);
  quality.disable_randomization_for_testing();

  FormData form = GetFormWithTypes({EMAIL_ADDRESS, ADDRESS_HOME_ZIP});
  FillForm(form, profile);

  // Edit field 0 to the same token of a another profile.
  EditFieldValue(form, 0,
                 other_profile.GetInfo(EMAIL_ADDRESS, pdm_.app_locale()));
  // Edit field 1 to a different token of another profile.
  EditFieldValue(form, 1,
                 other_profile.GetInfo(ADDRESS_HOME_STATE, pdm_.app_locale()));

  FormStructure* form_structure = bam_.FindCachedFormById(form.global_id());
  EXPECT_TRUE(
      quality.AddObservationsForFilledForm(*form_structure, form, pdm_));

  EXPECT_THAT(
      quality.GetObservationTypesForFieldType(EMAIL_ADDRESS),
      UnorderedElementsAre(ObservationType::kEditedToSameTokenOfOtherProfile));
  EXPECT_THAT(quality.GetObservationTypesForFieldType(ADDRESS_HOME_ZIP),
              UnorderedElementsAre(
                  ObservationType::kEditedToDifferentTokenOfOtherProfile));
}

// Tests that only a single observation is collected per field.
TEST_F(ProfileTokenQualityTest, AddObservationsForFilledForm_SameField) {
  AutofillProfile profile = test::GetFullProfile();
  pdm_.AddProfile(profile);
  ProfileTokenQuality quality(&profile);

  FormData form = GetFormWithTypes({NAME_FIRST});
  FillForm(form, profile);

  FormStructure* form_structure = bam_.FindCachedFormById(form.global_id());
  EXPECT_TRUE(
      quality.AddObservationsForFilledForm(*form_structure, form, pdm_));
  EXPECT_THAT(quality.GetObservationTypesForFieldType(NAME_FIRST),
              UnorderedElementsAre(ObservationType::kAccepted));
  EXPECT_FALSE(
      quality.AddObservationsForFilledForm(*form_structure, form, pdm_));
  EXPECT_THAT(quality.GetObservationTypesForFieldType(NAME_FIRST),
              UnorderedElementsAre(ObservationType::kAccepted));
}

TEST_F(ProfileTokenQualityTest, IsWithinLevenshteinDistance) {
  // Checks if the Levenshtein distance between `a` and `b` is exactly `k`, by
  // checking that it is <= `k` but not <= `k-1`.
  auto has_levenshtein_distance = [](std::u16string_view a,
                                     std::u16string_view b, size_t k) {
    return ProfileTokenQuality::IsWithinLevenshteinDistanceForTesting(a, b,
                                                                      k) &&
           (k == 0 ||
            !ProfileTokenQuality::IsWithinLevenshteinDistanceForTesting(a, b,
                                                                        k - 1));
  };

  EXPECT_TRUE(has_levenshtein_distance(u"aa", u"aa", 0));
  EXPECT_TRUE(has_levenshtein_distance(u"a", u"aa", 1));
  EXPECT_TRUE(has_levenshtein_distance(u"ab", u"aa", 1));
  EXPECT_TRUE(has_levenshtein_distance(u"aba", u"aa", 1));
  EXPECT_TRUE(has_levenshtein_distance(u"", u"12", 2));
  EXPECT_TRUE(has_levenshtein_distance(u"street", u"str.", 3));
  EXPECT_TRUE(has_levenshtein_distance(u"asdf", u"fdsa", 4));
  EXPECT_TRUE(has_levenshtein_distance(std::u16string(100, 'a'),
                                       std::u16string(200, 'a'), 100));
}

// Tests that `SaveObservationsForFilledFormForAllSubmittedProfiles()` collects
// observations for all profiles that were used to fill the form.
TEST_F(ProfileTokenQualityTest,
       SaveObservationsForFilledFormForAllSubmittedProfiles) {
  // Create two profiles, one without an `EMAIL_ADDRESS` and one without an
  // `ADDRESS_HOME_CITY`.
  AutofillProfile profile1 = test::GetFullProfile();
  profile1.ClearFields({EMAIL_ADDRESS});
  AutofillProfile profile2 = test::GetFullProfile2();
  profile2.ClearFields({ADDRESS_HOME_CITY});
  pdm_.AddProfile(profile1);
  pdm_.AddProfile(profile2);

  // No profile contains sufficient data to fill both fields.
  FormData form = GetFormWithTypes({ADDRESS_HOME_CITY, EMAIL_ADDRESS});
  FillForm(form, profile1, /*triggering_field_index=*/0);
  FillForm(form, profile2, /*triggering_field_index=*/1);

  ProfileTokenQuality::SaveObservationsForFilledFormForAllSubmittedProfiles(
      *bam_.FindCachedFormById(form.global_id()), form, pdm_);

  // Expect that observations for both profiles were collected. Since
  // `SaveObservationsForFilledFormForAllSubmittedProfiles()` operates on the
  // profiles owned by the `pdm_`, the profiles need to be accessed through the
  // `pdm_`. `profile1` and `profile2` haven't changed.
  const ProfileTokenQuality& quality1 =
      pdm_.GetProfileByGUID(profile1.guid())->token_quality();
  EXPECT_THAT(quality1.GetObservationTypesForFieldType(ADDRESS_HOME_CITY),
              UnorderedElementsAre(ObservationType::kAccepted));
  EXPECT_TRUE(quality1.GetObservationTypesForFieldType(EMAIL_ADDRESS).empty());
  const ProfileTokenQuality& quality2 =
      pdm_.GetProfileByGUID(profile2.guid())->token_quality();
  EXPECT_THAT(quality2.GetObservationTypesForFieldType(EMAIL_ADDRESS),
              UnorderedElementsAre(ObservationType::kAccepted));
  EXPECT_TRUE(
      quality2.GetObservationTypesForFieldType(ADDRESS_HOME_CITY).empty());
}

// Tests the dropping of random observations during
// `AddObservationsForFilledForm()`. In particular, tests that for a form
// containing fields of the the given `form_types`, the
// `expected_number_of_observations` are collected.
struct DropObservationTest {
  std::vector<ServerFieldType> form_types;
  int expected_number_of_observations;
};

class ProfileTokenQualityObservationDroppingTest
    : public ProfileTokenQualityTest,
      public testing::WithParamInterface<DropObservationTest> {};

TEST_P(ProfileTokenQualityObservationDroppingTest,
       AddObservationsForFilledForm_DropObservations) {
  const DropObservationTest& test = GetParam();
  AutofillProfile profile = test::GetFullProfile();
  pdm_.AddProfile(profile);
  ProfileTokenQuality quality(&profile);

  FormData form = GetFormWithTypes(test.form_types);
  FillForm(form, profile);

  EXPECT_TRUE(quality.AddObservationsForFilledForm(
      *bam_.FindCachedFormById(form.global_id()), form, pdm_));
  EXPECT_EQ(test.expected_number_of_observations,
            base::ranges::count_if(test.form_types, [&](ServerFieldType type) {
              return !quality.GetObservationTypesForFieldType(type).empty();
            }));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ProfileTokenQualityObservationDroppingTest,
    testing::Values(
        // Average size form: Expect that three observations are dropped, such
        // that 2 observations are collected.
        DropObservationTest{{NAME_FIRST, NAME_LAST, ADDRESS_HOME_STREET_ADDRESS,
                             ADDRESS_HOME_CITY, ADDRESS_HOME_ZIP},
                            2},
        // Small form: Expect that one observation is dropped, such that only
        // a single observation is collected.
        DropObservationTest{{NAME_FIRST, NAME_LAST}, 1},
        // Large form: Expect that four observations are dropped, such that
        // the limit of eight observations are collected.
        DropObservationTest{
            {NAME_FIRST, NAME_LAST_FIRST, NAME_LAST_SECOND, COMPANY_NAME,
             ADDRESS_HOME_STREET_NAME, ADDRESS_HOME_HOUSE_NUMBER,
             ADDRESS_HOME_CITY, ADDRESS_HOME_ZIP, ADDRESS_HOME_STATE,
             ADDRESS_HOME_COUNTRY, EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER},
            8}));

}  // namespace autofill
