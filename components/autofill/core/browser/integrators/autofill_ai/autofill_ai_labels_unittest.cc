// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_labels.h"

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;

using enum AttributeTypeName;

constexpr char16_t kSeparator[] = u" - ";
constexpr std::u16string kDots = u"\u2022\u2060\u2006\u2060";

class AutofillAiLabelsTest : public testing::Test {
 public:
  std::vector<std::u16string> GetLabels(
      base::span<const EntityInstance* const> entities,
      DenseSet<AttributeTypeName> attribute_type_names_to_ignore,
      bool only_disambiguating_types,
      bool obfuscate_sensitive_types = false,
      const std::string& app_locale = "en-US") {
    DenseSet<AttributeType> attribute_types_to_ignore(
        attribute_type_names_to_ignore,
        [](AttributeTypeName name) { return AttributeType(name); });
    return base::ToVector(
        GetLabelsForEntities(entities, attribute_types_to_ignore,
                             only_disambiguating_types,
                             obfuscate_sensitive_types, app_locale),
        [&](const EntityLabel& label) {
          return base::JoinString(label, kSeparator);
        });
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillAiWalletPrivatePasses};
};

// Tests that just enough labels are shown to disambiguate suggestions, meaning
// that if more labels can be shown but aren't necessary, they won't be shown.
TEST_F(AutofillAiLabelsTest, Suggestions_OnlyEnoughLabels) {
  EntityInstance vehicle_1 =
      test::GetVehicleEntityInstance({.name = u"John Doe",
                                      .plate = u"",
                                      .number = u"",
                                      .make = u"BMW",
                                      .model = u"Series 5",
                                      .year = u"",
                                      .state = u""});
  EntityInstance vehicle_2 =
      test::GetVehicleEntityInstance({.name = u"Jane Doe",
                                      .plate = u"",
                                      .number = u"",
                                      .make = u"Toyota",
                                      .model = u"Camry",
                                      .year = u"",
                                      .state = u""});

  // Even though the vehicle owner would be a valid label and that showing two
  // disambiguating labels is allowed, this information is not shown, because
  // showing the vehicle make (which has higher priority than owner) is already
  // enough to fully disambiguate the two entities.
  EXPECT_THAT(GetLabels({&vehicle_1, &vehicle_2}, {kVehicleModel},
                        /*only_disambiguating_types=*/true),
              ElementsAre(u"BMW", u"Toyota"));
}

// Tests that non-disambiguating types are never used for generating labels for
// suggestions, even if that means generated labels will be empty or non-unique.
TEST_F(AutofillAiLabelsTest, Suggestions_NoNonDisambiguatingTypes) {
  EntityInstance vehicle_1 =
      test::GetVehicleEntityInstance({.name = u"John Doe",
                                      .plate = u"",
                                      .number = u"1234",
                                      .make = u"BMW",
                                      .model = u"",
                                      .year = u"",
                                      .state = u""});
  EntityInstance vehicle_2 =
      test::GetVehicleEntityInstance({.name = u"John Doe",
                                      .plate = u"",
                                      .number = u"5678",
                                      .make = u"Toyota",
                                      .model = u"",
                                      .year = u"",
                                      .state = u""});

  // Even though the VIN would disambiguate the entities, it is not used as a
  // label for suggestions because the VIN is not a disambiguation type. Since
  // no other considered type has a value, the resulting labels are empty.
  EXPECT_THAT(GetLabels({&vehicle_1, &vehicle_2}, {kVehicleMake, kVehicleOwner},
                        /*only_disambiguating_types=*/true),
              ElementsAre(u"", u""));

  // Even though the VIN would disambiguate the entities, it is not used as a
  // label for suggestions because the VIN is not a disambiguation type. Since
  // no all other considered types are identical in value, the resulting labels
  // are identical.
  EXPECT_THAT(GetLabels({&vehicle_1, &vehicle_2}, {kVehicleMake},
                        /*only_disambiguating_types=*/true),
              ElementsAre(u"John Doe", u"John Doe"));
}

// Tests that non-disambiguating types could be used for generating labels for
// settings, but that only happens if nothing else is available.
TEST_F(AutofillAiLabelsTest, Settings_NonDisambiguatingTypesAsLastResort) {
  EntityInstance vehicle_1 =
      test::GetVehicleEntityInstance({.name = u"John Doe",
                                      .plate = u"",
                                      .number = u"1234",
                                      .make = u"BMW",
                                      .model = u"",
                                      .year = u"",
                                      .state = u""});
  EntityInstance vehicle_2 =
      test::GetVehicleEntityInstance({.name = u"John Doe",
                                      .plate = u"",
                                      .number = u"5678",
                                      .make = u"Toyota",
                                      .model = u"",
                                      .year = u"",
                                      .state = u""});

  // VINs are used since otherwise an empty label would be generated and no
  // other considered attribute had values to present.
  EXPECT_THAT(GetLabels({&vehicle_1, &vehicle_2}, {kVehicleMake, kVehicleOwner},
                        /*only_disambiguating_types=*/false),
              ElementsAre(u"1234", u"5678"));

  // Even though the VIN would disambiguate the entities, it is not used as a
  // label if a disambiguating type has a value available to present, even if
  // that value doesn't yield unique labels. It is intentionally preferred to
  // show non-unique labels than to show a non-disambiguating type as a label.
  EXPECT_THAT(GetLabels({&vehicle_1, &vehicle_2}, {kVehicleMake},
                        /*only_disambiguating_types=*/false),
              ElementsAre(u"John Doe", u"John Doe"));
}

TEST_F(AutofillAiLabelsTest, FlightDepartureDate_IsLocaleDependent) {
  base::Time departure_time;
  ASSERT_TRUE(base::Time::FromUTCString("2025-01-01", &departure_time));
  EntityInstance flight_1 = test::GetFlightReservationEntityInstance(
      {.departure_time = departure_time});
  EntityInstance flight_2 = test::GetFlightReservationEntityInstance(
      {.departure_time = departure_time + base::Days(1)});

  EXPECT_THAT(GetLabels({&flight_1, &flight_2}, {},
                        /*only_disambiguating_types=*/true),
              ElementsAre(u"Jan 1", u"Jan 2"));
  EXPECT_THAT(GetLabels({&flight_1, &flight_2}, {},
                        /*only_disambiguating_types=*/true,
                        /*obfuscate_sensitive_types=*/false,
                        /*app_locale=*/"de-DE"),
              ElementsAre(u"1. Jan.", u"2. Jan."));
  EXPECT_THAT(GetLabels({&flight_1, &flight_2}, {},
                        /*only_disambiguating_types=*/true,
                        /*obfuscate_sensitive_types=*/false,
                        /*app_locale=*/"pl-PL"),
              ElementsAre(u"1 sty", u"2 sty"));
}

TEST_F(AutofillAiLabelsTest, ObfuscateSensitiveTypes) {
  // Note that number is an obfuscated field.
  EntityInstance passport_1 =
      test::GetDriversLicenseEntityInstanceWithRandomGuid({
          .name = u"",
          .region = u"",
          .number = u"1234567890",
          .expiration_date = u"",
          .issue_date = u"",

      });
  EntityInstance passport_2 =
      test::GetDriversLicenseEntityInstanceWithRandomGuid({
          .name = u"",
          .region = u"",
          .number = u"0987654321",
          .expiration_date = u"",
          .issue_date = u"",
      });

  // Use only_disambiguating_types=false to force usage of drivers license
  // number.
  std::vector<std::u16string> labels = GetLabels(
      {&passport_1, &passport_2}, /*attribute_type_names_to_ignore=*/{},
      /*only_disambiguating_types=*/false, /*obfuscate_sensitive_types=*/true);

  EXPECT_THAT(labels,
              ElementsAre(base::StrCat({kDots, kDots, kDots, kDots, u"7890"}),
                          base::StrCat({kDots, kDots, kDots, kDots, u"4321"})));
}

struct DisambiguationTestCase {
  const char16_t* make_1;
  const char16_t* make_2;
};

TEST_F(AutofillAiLabelsTest, NormalizationUsesAppLocale) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnableGermanTransliteration);

  EntityInstance vehicle_1 =
      test::GetVehicleEntityInstance({.name = u"John Doe",
                                      .plate = u"",
                                      .number = u"",
                                      .make = u"Müller",
                                      .model = u"Series 5",
                                      .year = u"",
                                      .state = u""});
  EntityInstance vehicle_2 =
      test::GetVehicleEntityInstance({.name = u"Jane Doe",
                                      .plate = u"",
                                      .number = u"",
                                      .make = u"Mueller",
                                      .model = u"Series 5",
                                      .year = u"",
                                      .state = u""});

  // In "de-DE", they are normalized to the same.
  EXPECT_THAT(GetLabels({&vehicle_1, &vehicle_2}, {kVehicleModel},
                        /*only_disambiguating_types=*/true,
                        /*obfuscate_sensitive_types=*/false,
                        /*app_locale=*/"de-DE"),
              ElementsAre(u"John Doe", u"Jane Doe"));

  // In "en-US", they are normalized differently.
  EXPECT_THAT(GetLabels({&vehicle_1, &vehicle_2}, {kVehicleModel},
                        /*only_disambiguating_types=*/true,
                        /*obfuscate_sensitive_types=*/false,
                        /*app_locale=*/"en-US"),
              ElementsAre(u"Müller", u"Mueller"));
}

class AutofillAiLabelsParamTest
    : public AutofillAiLabelsTest,
      public ::testing::WithParamInterface<DisambiguationTestCase> {};

TEST_P(AutofillAiLabelsParamTest, Normalization) {
  // Assert that `Make` has higher priority than `Owner` (Name) for the vehicle
  // entity type. If this order were to change, this test would become
  // meaningless because `Owner` (which is different: "John Doe" vs "Jane Doe")
  // would be chosen first regardless of whether `Make` was normalized or not.
  ASSERT_TRUE(AttributeType::DisambiguationOrder(AttributeType(kVehicleMake),
                                                 AttributeType(kVehicleOwner)));
  const auto& [make_1, make_2] = GetParam();
  EntityInstance vehicle_1 =
      test::GetVehicleEntityInstance({.name = u"John Doe",
                                      .plate = u"",
                                      .number = u"",
                                      .make = make_1,
                                      .model = u"Series 5",
                                      .year = u"",
                                      .state = u""});
  EntityInstance vehicle_2 =
      test::GetVehicleEntityInstance({.name = u"Jane Doe",
                                      .plate = u"",
                                      .number = u"",
                                      .make = make_2,
                                      .model = u"Series 5",
                                      .year = u"",
                                      .state = u""});

  // If normalization works, make_1 and make_2 are considered identical.
  // Since model is also identical (Series 5) and other fields are empty,
  // Make and Model are not prioritized.
  // Owner (Name) is different ("John Doe" vs "Jane Doe") and is prioritized.
  // So we should get Owner labels.
  EXPECT_THAT(GetLabels({&vehicle_1, &vehicle_2}, {kVehicleModel},
                        /*only_disambiguating_types=*/true),
              ElementsAre(u"John Doe", u"Jane Doe"));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillAiLabels,
    AutofillAiLabelsParamTest,
    ::testing::Values(
        // Case insensitivity
        DisambiguationTestCase{.make_1 = u"BMW", .make_2 = u"bmw"},
        // Trimming
        DisambiguationTestCase{.make_1 = u" BMW ", .make_2 = u"BMW"},
        // Collapsing
        DisambiguationTestCase{.make_1 = u"B  MW", .make_2 = u"B MW"}));

}  // namespace
}  // namespace autofill
