// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/entity_attribute_update_details.h"

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;

TEST(EntityAttributeUpdateDetailsTest, AttributeUnchanged) {
  EntityInstance new_number = test::GetKnownTravelerNumberInstance(
      {.name = u"Name", .number = u"1234", .expiration_date = u"2030-01-01"});
  EntityInstance old_number = test::GetKnownTravelerNumberInstance(
      {.name = u"Name", .number = u"1234", .expiration_date = u"2030-01-01"});

  ASSERT_THAT(
      EntityAttributeUpdateDetails::GetUpdatedAttributesDetails(
          new_number, old_number, "en-US"),
      ElementsAre(
          EntityAttributeUpdateDetails(
              u"Name", u"Name", std::nullopt,
              EntityAttributeUpdateType::kNewEntityAttributeUnchanged),
          EntityAttributeUpdateDetails(
              u"Number", u"1234", std::nullopt,
              EntityAttributeUpdateType::kNewEntityAttributeUnchanged),
          EntityAttributeUpdateDetails(
              u"Expiration date", u"Jan 1, 2030", std::nullopt,
              EntityAttributeUpdateType::kNewEntityAttributeUnchanged)));
}

TEST(EntityAttributeUpdateDetailsTest, AttributeUpdated) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kAutofillAiNewUpdatePrompt);

  EntityInstance new_ktn = test::GetKnownTravelerNumberInstance(
      {.name = u"Name", .number = u"1234", .expiration_date = u"2030-01-01"});
  EntityInstance old_ktn = test::GetKnownTravelerNumberInstance(
      {.name = u"Name", .number = u"1234", .expiration_date = u"2025-01-01"});

  ASSERT_THAT(
      EntityAttributeUpdateDetails::GetUpdatedAttributesDetails(
          new_ktn, old_ktn, "en-US"),
      ElementsAre(
          EntityAttributeUpdateDetails(
              u"Expiration date", u"Jan 1, 2030", u"Jan 1, 2025",
              EntityAttributeUpdateType::kNewEntityAttributeUpdated),
          EntityAttributeUpdateDetails(
              u"Name", u"Name", std::nullopt,
              EntityAttributeUpdateType::kNewEntityAttributeUnchanged),
          EntityAttributeUpdateDetails(
              u"Number", u"1234", std::nullopt,
              EntityAttributeUpdateType::kNewEntityAttributeUnchanged)));
}

// Tests that updated entities are no longer always listed first when
// `kAutofillAiNewUpdatePrompt` is enabled.
TEST(EntityAttributeUpdateDetailsTest, AttributeUpdatedRevampedUI) {
  base::test::ScopedFeatureList features{features::kAutofillAiNewUpdatePrompt};
  EntityInstance new_ktn = test::GetKnownTravelerNumberInstance(
      {.name = u"Name", .number = u"1234", .expiration_date = u"2030-01-01"});
  EntityInstance old_ktn = test::GetKnownTravelerNumberInstance(
      {.name = u"Name", .number = u"1234", .expiration_date = u"2025-01-01"});

  ASSERT_THAT(
      EntityAttributeUpdateDetails::GetUpdatedAttributesDetails(
          new_ktn, old_ktn, "en-US"),
      ElementsAre(EntityAttributeUpdateDetails(
                      u"Name", u"Name", std::nullopt,
                      EntityAttributeUpdateType::kNewEntityAttributeUnchanged),
                  EntityAttributeUpdateDetails(
                      u"Number", u"1234", std::nullopt,
                      EntityAttributeUpdateType::kNewEntityAttributeUnchanged),
                  EntityAttributeUpdateDetails(
                      u"Expiration date", u"Jan 1, 2030", u"Jan 1, 2025",
                      EntityAttributeUpdateType::kNewEntityAttributeUpdated)));
}

TEST(EntityAttributeUpdateDetailsTest, AttributeAdded) {
  EntityInstance new_number = test::GetKnownTravelerNumberInstance(
      {.name = u"Name", .number = u"4321", .expiration_date = u"2030-01-01"});
  EntityInstance old_number = test::GetKnownTravelerNumberInstance(
      {.name = u"Name", .number = u"1234", .expiration_date = nullptr});

  ASSERT_THAT(
      EntityAttributeUpdateDetails::GetUpdatedAttributesDetails(
          new_number, old_number, "en-US"),
      ElementsAre(
          EntityAttributeUpdateDetails(
              u"Number", u"4321", u"1234",
              EntityAttributeUpdateType::kNewEntityAttributeUpdated),
          EntityAttributeUpdateDetails(
              u"Expiration date", u"Jan 1, 2030", std::nullopt,
              EntityAttributeUpdateType::kNewEntityAttributeAdded),
          EntityAttributeUpdateDetails(
              u"Name", u"Name", std::nullopt,
              EntityAttributeUpdateType::kNewEntityAttributeUnchanged)));
}

TEST(EntityAttributeUpdateDetailsTest, AttributeRemoved) {
  EntityInstance new_number = test::GetKnownTravelerNumberInstance(
      {.name = u"Name", .number = u"4321", .expiration_date = nullptr});
  EntityInstance old_number = test::GetKnownTravelerNumberInstance(
      {.name = u"Name", .number = u"1234", .expiration_date = u"2030-01-01"});

  // Only the attributes in the new entity are taken into account.
  ASSERT_THAT(
      EntityAttributeUpdateDetails::GetUpdatedAttributesDetails(
          new_number, old_number, "en-US"),
      ElementsAre(
          EntityAttributeUpdateDetails(
              u"Number", u"4321", u"1234",
              EntityAttributeUpdateType::kNewEntityAttributeUpdated),
          EntityAttributeUpdateDetails(
              u"Name", u"Name", std::nullopt,
              EntityAttributeUpdateType::kNewEntityAttributeUnchanged)));
}

}  // namespace

}  // namespace autofill
