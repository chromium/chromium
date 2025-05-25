// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using enum AttributeTypeName;

constexpr char kAppLocaleUS[] = "en-US";

struct GetInfoParams {
  std::string app_locale = "";
  const char16_t* format_string = nullptr;
};

std::u16string GetInfo(const AttributeInstance& a,
                       FieldType field_type,
                       GetInfoParams params = {}) {
  return a.GetInfo(field_type, params.app_locale,
                   params.format_string
                       ? std::optional(std::u16string(params.format_string))
                       : std::nullopt);
}

TEST(AutofillEntityInstanceTest, Attributes) {
  const char16_t kName[] = u"Pippi";
  EntityInstance pp =
      test::GetPassportEntityInstance({.name = kName, .number = nullptr});
  EXPECT_EQ(pp.attributes().size(), 4u);
  EXPECT_EQ(pp.type().attributes().size(), 5u);
  EXPECT_FALSE(pp.attribute(AttributeType(kPassportNumber)));
  {
    base::optional_ref<const AttributeInstance> a =
        pp.attribute(AttributeType(kPassportName));
    ASSERT_TRUE(a);
    EXPECT_THAT(a->type(), AttributeType(kPassportName));
    EXPECT_EQ(GetInfo(*a, NAME_FULL), kName);
  }
}

// Tests that AttributeInstance appropriately handles various types in its
// getters.
TEST(AutofillEntityInstanceTest, Attributes_NormalizedType) {
  AttributeInstance passport_name((AttributeType(kPassportName)));
  passport_name.SetInfo(NAME_FULL, u"Some Name",
                        /*app_locale=*/"", /*format_string=*/u"",
                        VerificationStatus::kObserved);
  passport_name.FinalizeInfo();

  AttributeInstance passport_number((AttributeType(kPassportNumber)));
  passport_number.SetInfo(PASSPORT_NUMBER, u"LR0123456",
                          /*app_locale=*/"", /*format_string=*/u"",
                          VerificationStatus::kObserved);

  // We can retrieve info from structured attributes when the provided type
  // gives us the information that is missing from the attribute's generic type
  // (In that case `PASSPORT_NAME_TAG`). Otherwise we do not
  EXPECT_EQ(GetInfo(passport_name, NAME_FULL), u"Some Name");
  EXPECT_TRUE(GetInfo(passport_name, ADDRESS_HOME_STREET_NAME).empty());

  // Non-structured attributes, on the other hand, have the complete information
  // needed to fetch the value from the attribute type. Hence we just ignore the
  // type given to the getter.
  EXPECT_EQ(GetInfo(passport_number, PASSPORT_NUMBER), u"LR0123456");
  EXPECT_EQ(GetInfo(passport_number, ADDRESS_HOME_STREET_NAME), u"LR0123456");
}

// Tests that AttributeInstance localizes the country name.
TEST(AutofillEntityInstanceTest, Attributes_CountryLocalization) {
  AttributeInstance passport_country((AttributeType(kPassportCountry)));
  passport_country.SetInfo(PASSPORT_ISSUING_COUNTRY, u"SE",
                           /*app_locale=*/"", /*format_string=*/u"",
                           VerificationStatus::kObserved);

  EXPECT_EQ(GetInfo(passport_country, PASSPORT_ISSUING_COUNTRY,
                    {.app_locale = kAppLocaleUS}),
            u"Sweden");
  EXPECT_EQ(GetInfo(passport_country, ADDRESS_HOME_COUNTRY,
                    {.app_locale = kAppLocaleUS}),
            u"Sweden");

  EXPECT_EQ(GetInfo(passport_country, PASSPORT_ISSUING_COUNTRY,
                    {.app_locale = "de-DE"}),
            u"Schweden");
  EXPECT_EQ(
      GetInfo(passport_country, ADDRESS_HOME_COUNTRY, {.app_locale = "de-DE"}),
      u"Schweden");
}

// Tests that AttributeInstance appropriately manages structured names.
TEST(AutofillEntityInstanceTest, Attributes_StructuredName) {
  AttributeInstance passport_name((AttributeType(kPassportName)));
  passport_name.SetInfo(NAME_FULL, u"Some Name",
                        /*app_locale=*/"", /*format_string=*/u"",
                        VerificationStatus::kObserved);
  passport_name.FinalizeInfo();

  // The value propagated correctly.
  EXPECT_EQ(GetInfo(passport_name, NAME_FULL), u"Some Name");
  EXPECT_EQ(GetInfo(passport_name, NAME_FIRST), u"Some");
  EXPECT_EQ(GetInfo(passport_name, NAME_LAST), u"Name");
}

// Tests that AttributeInstance appropriately manages dates.
TEST(AutofillEntityInstanceTest, Attributes_Date) {
  AttributeInstance passport_name((AttributeType(kPassportIssueDate)));
  passport_name.SetInfo(PASSPORT_ISSUE_DATE, u"2001-02-03",
                        /*app_locale=*/"", /*format_string=*/u"YYYY-MM-DD",
                        VerificationStatus::kNoStatus);

  EXPECT_EQ(GetInfo(passport_name, PASSPORT_ISSUE_DATE), u"2001-02-03");
  EXPECT_EQ(GetInfo(passport_name, PASSPORT_ISSUE_DATE,
                    {.format_string = u"DD/MM/YYYY"}),
            u"03/02/2001");
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
  EntityInstance::EntityMergeability result =
      test::GetPassportEntityInstance().GetEntityMergeability(
          test::GetPassportEntityInstance({.expiry_date = nullptr}));

  EXPECT_TRUE(result.mergeable_attributes.empty());
  EXPECT_TRUE(result.is_subset);
}
TEST(
    AutofillEntityInstanceTest,
    GetEntityMergeability_NewEntityEmptyStringAttribute_NoMergeablesAttribute_IsASubset) {
  EntityInstance::EntityMergeability result =
      test::GetPassportEntityInstance().GetEntityMergeability(
          test::GetPassportEntityInstance({.expiry_date = nullptr}));

  EXPECT_TRUE(result.mergeable_attributes.empty());
  EXPECT_TRUE(result.is_subset);
}

TEST(
    AutofillEntityInstanceTest,
    GetEntityMergeability_NewEntityHasNewAttribute_MergeableAttributesExists_IsNotASubset) {
  EntityInstance old_entity =
      test::GetPassportEntityInstance({.expiry_date = nullptr});
  EntityInstance new_entity = test::GetPassportEntityInstance();
  EntityInstance::EntityMergeability result =
      old_entity.GetEntityMergeability(new_entity);

  ASSERT_EQ(result.mergeable_attributes.size(), 1u);
  EXPECT_EQ(result.mergeable_attributes[0].type().name(),
            kPassportExpirationDate);

  const AttributeInstance& old_attribute = result.mergeable_attributes[0];
  base::optional_ref<const AttributeInstance> new_attribute =
      new_entity.attribute(AttributeType(kPassportExpirationDate));
  EXPECT_EQ(old_attribute.GetCompleteInfo(/*app_locale=*/""),
            new_attribute->GetCompleteInfo(/*app_locale=*/""));
  EXPECT_FALSE(result.is_subset);
}

// This test has two entities that have the same merge constraints (Passport
// number and expiry date). However, newer contains an update data for country,
// this should not lead to a fresh entity, rather an updated one.
TEST(
    AutofillEntityInstanceTest,
    GetEntityMergeability_MergeConstraintsMatch_AttributeWithDifferentValue_MergeableAttributesExists_IsNotASubset) {
  EntityInstance new_entity =
      test::GetPassportEntityInstance({.country = u"Argentina"});
  EntityInstance::EntityMergeability result =
      test::GetPassportEntityInstance().GetEntityMergeability(new_entity);

  ASSERT_EQ(result.mergeable_attributes.size(), 1u);
  EXPECT_EQ(result.mergeable_attributes[0].type().name(), kPassportCountry);

  const AttributeInstance& old_attribute = result.mergeable_attributes[0];
  base::optional_ref<const AttributeInstance> new_attribute =
      new_entity.attribute(AttributeType(kPassportCountry));
  EXPECT_EQ(old_attribute.GetCompleteInfo(kAppLocaleUS),
            new_attribute->GetCompleteInfo(kAppLocaleUS));
  EXPECT_FALSE(result.is_subset);
}

TEST(
    AutofillEntityInstanceTest,
    GetEntityMergeability_NewEntityHasSameAttributeWithDifferentValue_MergeableAttributesDoNotExists_IsNotASubset) {
  EntityInstance old_entity =
      test::GetPassportEntityInstance({.number = u"123"});
  EntityInstance new_entity =
      test::GetPassportEntityInstance({.number = u"456"});
  EntityInstance::EntityMergeability result =
      old_entity.GetEntityMergeability(new_entity);
  EXPECT_TRUE(result.mergeable_attributes.empty());
  EXPECT_FALSE(result.is_subset);
}

TEST(
    AutofillEntityInstanceTest,
    GetEntityMergeability_NewEntityHasEquivalentAttribute_MergeableAttributesDoNotExists_IsASubset) {
  EntityInstance::EntityMergeability result =
      test::GetPassportEntityInstance({.number = u"1234 5"})
          .GetEntityMergeability(
              test::GetPassportEntityInstance({.number = u"1234    5"}));

  EXPECT_TRUE(result.mergeable_attributes.empty());
  EXPECT_TRUE(result.is_subset);
}

// Tests that valid entities that don't share any attribute in common are not
// merged, as we cannot verify that they represent the same real-world object.
TEST(AutofillEntityInstanceTest, GetEntityMergeability_EntitiesAreDisjoint) {
  EntityInstance::EntityMergeability result =
      test::GetVehicleEntityInstance({.number = u"12345"})
          .GetEntityMergeability(
              test::GetVehicleEntityInstance({.plate = u"6789"}));

  EXPECT_TRUE(result.mergeable_attributes.empty());
  EXPECT_FALSE(result.is_subset);
}

TEST(AutofillEntityInstanceTest, RankingOrder_SortEntitiesByFrecency) {
  auto pp_with_random_guid = []() {
    return test::GetPassportEntityInstance(
        {.guid = base::Uuid::GenerateRandomV4().AsLowercaseString()});
  };
  std::vector<EntityInstance> entities = {pp_with_random_guid(),
                                          pp_with_random_guid()};
  auto sort_entities = [&]() {
    EntityInstance::RankingOrder comp(base::Time::Now());
    std::ranges::sort(entities, comp);
  };

  // Set first passport as have been used once.
  EntityInstance& top_entity = entities[0];
  base::Uuid first_top_entity_guid = top_entity.guid();
  top_entity.RecordEntityUsed(test::kJune2017);
  sort_entities();
  EXPECT_EQ(entities[0].guid(), top_entity.guid());

  // Now set second passport as have been used twice. Note that the second use
  // date is the same as the one for the first passport.
  top_entity = entities[1];
  base::Uuid second_top_entity_guid = top_entity.guid();
  top_entity.RecordEntityUsed(test::kJanuary2017);
  top_entity.RecordEntityUsed(test::kJune2017);
  sort_entities();
  EXPECT_EQ(entities[0].guid(), top_entity.guid());
}

}  // namespace
}  // namespace autofill
