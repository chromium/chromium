// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/entity_instance.h"

#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/data_model/entity_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

TEST(AutofillEntityInstanceTest, Attributes) {
  const char16_t kName[] = u"Pippi";
  EntityInstance pp =
      test::GetPassportEntityInstance({.name = kName, .number = nullptr});
  using enum AttributeTypeName;
  EXPECT_EQ(pp.attributes().size(), 4u);
  EXPECT_EQ(pp.type().attributes().size(), 5u);
  EXPECT_FALSE(pp.attribute(AttributeType(kPassportNumber)));
  {
    base::optional_ref<const AttributeInstance> a =
        pp.attribute(AttributeType(kPassportName));
    ASSERT_TRUE(a);
    EXPECT_THAT(a->type(), AttributeType(kPassportName));
    EXPECT_THAT(a->value(), std::u16string_view(kName));
  }
}

TEST(AutofillEntityInstanceTest,
     GetEntityMergeability_IdentiticalEntities_NoMergeableAttribute_IsASubset) {
  EntityInstance::EntityMergeability result =
      test::GetPassportEntityInstance().GetEntityMergeability(
          test::GetPassportEntityInstance());
  EXPECT_TRUE(result.mergeable_attributes.empty());
  EXPECT_TRUE(result.is_subset);
}

TEST(
    AutofillEntityInstanceTest,
    GetEntityMergeability_NewEntityMissingAttribute_NoMergeableAttributes_IsASubset) {
  autofill::test::PassportEntityOptions passport_without_an_expiry_date;
  passport_without_an_expiry_date.expiry_date = nullptr;

  EntityInstance::EntityMergeability result =
      test::GetPassportEntityInstance().GetEntityMergeability(
          test::GetPassportEntityInstance(passport_without_an_expiry_date));

  EXPECT_TRUE(result.mergeable_attributes.empty());
  EXPECT_TRUE(result.is_subset);
}
TEST(
    AutofillEntityInstanceTest,
    GetEntityMergeability_NewEntityEmptyStringAttribute_NoMergeablesAttribute_IsASubset) {
  autofill::test::PassportEntityOptions passport_without_an_expiry_date;
  passport_without_an_expiry_date.expiry_date = u"";

  EntityInstance::EntityMergeability result =
      test::GetPassportEntityInstance().GetEntityMergeability(
          test::GetPassportEntityInstance(passport_without_an_expiry_date));

  EXPECT_TRUE(result.mergeable_attributes.empty());
  EXPECT_TRUE(result.is_subset);
}

TEST(
    AutofillEntityInstanceTest,
    GetEntityMergeability_NewEntityHasNewAttribute_MergeableAttributesExists_IsNotASubset) {
  autofill::test::PassportEntityOptions passport_without_an_expiry_date;
  passport_without_an_expiry_date.expiry_date = u"";
  EntityInstance new_entity = test::GetPassportEntityInstance();
  EntityInstance::EntityMergeability result =
      test::GetPassportEntityInstance(passport_without_an_expiry_date)
          .GetEntityMergeability(new_entity);

  EXPECT_EQ(result.mergeable_attributes.size(), 1u);
  EXPECT_EQ(result.mergeable_attributes[0].type().name(),
            AttributeTypeName::kPassportExpiryDate);
  EXPECT_EQ(
      result.mergeable_attributes[0].value(),
      new_entity
          .attribute(AttributeType(AttributeTypeName::kPassportExpiryDate))
          ->value());
  EXPECT_FALSE(result.is_subset);
}

// This test has two entities that have the same merge constraints (Passport
// number and expiry date). However, newer contains an update data for country,
// this should not lead to a fresh entity, rather an updated one.
TEST(
    AutofillEntityInstanceTest,
    GetEntityMergeability_MergeConstraintsMatch_AttributeWithDifferentValue_MergeableAttributesExists_IsNotASubset) {
  autofill::test::PassportEntityOptions passport_with_new_country;
  passport_with_new_country.country = u"Argentina";
  EntityInstance new_entity =
      test::GetPassportEntityInstance(passport_with_new_country);
  EntityInstance::EntityMergeability result =
      test::GetPassportEntityInstance().GetEntityMergeability(new_entity);

  EXPECT_EQ(result.mergeable_attributes.size(), 1u);
  EXPECT_EQ(result.mergeable_attributes[0].type().name(),
            AttributeTypeName::kPassportCountry);
  EXPECT_EQ(
      result.mergeable_attributes[0].value(),
      new_entity.attribute(AttributeType(AttributeTypeName::kPassportCountry))
          ->value());
  EXPECT_FALSE(result.is_subset);
}

TEST(
    AutofillEntityInstanceTest,
    GetEntityMergeability_NewEntityHasSameAttributeWithDifferentValue_MergeableAttributesDoNotExists_IsNotASubset) {
  autofill::test::PassportEntityOptions passport_without_different_expiry_date;
  passport_without_different_expiry_date.expiry_date = u"01/12/2034";
  EntityInstance new_entity = test::GetPassportEntityInstance();
  EntityInstance::EntityMergeability result =
      test::GetPassportEntityInstance(passport_without_different_expiry_date)
          .GetEntityMergeability(new_entity);

  EXPECT_TRUE(result.mergeable_attributes.empty());
  EXPECT_FALSE(result.is_subset);
}

TEST(
    AutofillEntityInstanceTest,
    GetEntityMergeability_NewEntityHasEquivalentAttribute_MergeableAttributesDoNotExists_IsASubset) {
  autofill::test::PassportEntityOptions pp_1;
  pp_1.number = u"1234 5";
  autofill::test::PassportEntityOptions pp_2;
  pp_2.number = u" 1234    5";

  EntityInstance::EntityMergeability result =
      test::GetPassportEntityInstance(pp_1).GetEntityMergeability(
          test::GetPassportEntityInstance(pp_2));

  EXPECT_TRUE(result.mergeable_attributes.empty());
  EXPECT_TRUE(result.is_subset);
}

}  // namespace
}  // namespace autofill
