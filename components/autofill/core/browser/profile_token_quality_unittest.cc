// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/profile_token_quality.h"

#include <vector>

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ObservationType = ProfileTokenQuality::ObservationType;

// Ensures that `ProfileTokenQualityTest` supports all supported types of
// `AutofillProfile`. In particular, this test ensures that whenever a new
// non-stored type is added, the map in `GetStoredTypeOf()` is updated
// accordingly. If the type is supposed to be stored, it should be added to
// `AutofillTable::GetStoredTypesForAutofillProfile()`.
TEST(ProfileTokenQualityTest, AllSupportedTypesHandled) {
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

TEST(ProfileTokenQualityTest, GetObservationTypesForFieldType) {
  AutofillProfile profile;
  ProfileTokenQuality quality(&profile);

  EXPECT_TRUE(quality.GetObservationTypesForFieldType(NAME_FIRST).empty());

  quality.AddObservationForTesting(NAME_FIRST, ObservationType::kAccepted);
  EXPECT_THAT(quality.GetObservationTypesForFieldType(NAME_FIRST),
              testing::UnorderedElementsAre(ObservationType::kAccepted));
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

}  // namespace autofill
