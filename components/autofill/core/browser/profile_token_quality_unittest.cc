// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/profile_token_quality.h"

#include <memory>
#include <string>
#include <vector>

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
  // the first field.
  void FillForm(const FormData& form, const AutofillProfile& profile) {
    bam_.FillProfileForm(profile, form, form.fields[0],
                         AutofillTriggerSource::kPopup);
  }

 protected:
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

  FormData form = GetFormWithTypes({NAME_FIRST, NAME_LAST, ADDRESS_HOME_CITY});
  FillForm(form, profile);

  // Clear the value of field 0.
  EditFieldValue(form, 0, u"");
  // Edit field 1 to a different token of the same `profile`.
  EditFieldValue(form, 1, profile.GetInfo(NAME_MIDDLE, pdm_.app_locale()));
  // Edit field 2 to a completely different token.
  EditFieldValue(form, 2, u"different value");

  FormStructure* form_structure = bam_.FindCachedFormById(form.global_id());
  EXPECT_TRUE(
      quality.AddObservationsForFilledForm(*form_structure, form, pdm_));

  EXPECT_THAT(quality.GetObservationTypesForFieldType(NAME_FIRST),
              UnorderedElementsAre(ObservationType::kEditedValueCleared));
  EXPECT_THAT(quality.GetObservationTypesForFieldType(NAME_LAST),
              UnorderedElementsAre(
                  ObservationType::kEditedToDifferentTokenOfSameProfile));
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

}  // namespace autofill
