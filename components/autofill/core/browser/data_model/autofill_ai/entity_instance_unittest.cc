// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using enum AttributeTypeName;

constexpr char kAppLocaleUS[] = "en-US";

struct GetInfoParams {
  std::string app_locale = "";
  std::optional<AutofillFormatString> format_string;
};

std::u16string GetInfo(const AttributeInstance& a,
                       FieldType field_type,
                       GetInfoParams params = {}) {
  return a.GetInfo(field_type, params.app_locale, params.format_string);
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

// Tests that AttributeInstance::GetInfo() returns the value for the specified
// subtype (as per AttributeType::field_subtypes()) and defaults to the overall
// type (AttributeType::field_type()).
TEST(AutofillEntityInstanceTest, Attributes_NormalizedType) {
  AttributeInstance passport_name((AttributeType(kPassportName)));
  passport_name.SetInfo(NAME_FULL, u"John Doe",
                        /*app_locale=*/"", /*format_string=*/std::nullopt,
                        VerificationStatus::kObserved);
  passport_name.FinalizeInfo();

  AttributeInstance passport_number((AttributeType(kPassportNumber)));
  passport_number.SetInfo(PASSPORT_NUMBER, u"LR0123456",
                          /*app_locale=*/"", /*format_string=*/std::nullopt,
                          VerificationStatus::kObserved);

  EXPECT_EQ(GetInfo(passport_name, NAME_FULL), u"John Doe");
  EXPECT_EQ(GetInfo(passport_name, NAME_FIRST), u"John");
  EXPECT_EQ(GetInfo(passport_name, NAME_LAST), u"Doe");
  EXPECT_EQ(GetInfo(passport_name, ADDRESS_HOME_STREET_NAME), u"John Doe");
  EXPECT_EQ(GetInfo(passport_name, UNKNOWN_TYPE), u"John Doe");

  EXPECT_EQ(GetInfo(passport_number, PASSPORT_NUMBER), u"LR0123456");
  EXPECT_EQ(GetInfo(passport_number, ADDRESS_HOME_STREET_NAME), u"LR0123456");
  EXPECT_EQ(GetInfo(passport_number, UNKNOWN_TYPE), u"LR0123456");
}

// Tests that AttributeInstance localizes the country name.
TEST(AutofillEntityInstanceTest, Attributes_CountryLocalization) {
  AttributeInstance passport_country((AttributeType(kPassportCountry)));
  passport_country.SetInfo(PASSPORT_ISSUING_COUNTRY, u"SE",
                           /*app_locale=*/"", /*format_string=*/std::nullopt,
                           VerificationStatus::kObserved);

  EXPECT_EQ(GetInfo(passport_country, PASSPORT_ISSUING_COUNTRY,
                    {.app_locale = kAppLocaleUS}),
            u"Sweden");
  EXPECT_EQ(GetInfo(passport_country, ADDRESS_HOME_COUNTRY,
                    {.app_locale = kAppLocaleUS}),
            u"Sweden");
  EXPECT_EQ(
      GetInfo(passport_country, UNKNOWN_TYPE, {.app_locale = kAppLocaleUS}),
      u"Sweden");

  EXPECT_EQ(GetInfo(passport_country, PASSPORT_ISSUING_COUNTRY,
                    {.app_locale = "de-DE"}),
            u"Schweden");
  EXPECT_EQ(
      GetInfo(passport_country, ADDRESS_HOME_COUNTRY, {.app_locale = "de-DE"}),
      u"Schweden");
  EXPECT_EQ(GetInfo(passport_country, UNKNOWN_TYPE, {.app_locale = "de-DE"}),
            u"Schweden");
}

// Tests that AttributeInstance appropriately manages structured names.
TEST(AutofillEntityInstanceTest, Attributes_StructuredName) {
  AttributeInstance passport_name((AttributeType(kPassportName)));
  passport_name.SetInfo(NAME_FULL, u"Some Name",
                        /*app_locale=*/"", /*format_string=*/std::nullopt,
                        VerificationStatus::kObserved);
  passport_name.FinalizeInfo();

  // The value propagated correctly.
  EXPECT_EQ(GetInfo(passport_name, NAME_FULL), u"Some Name");
  EXPECT_EQ(GetInfo(passport_name, NAME_FIRST), u"Some");
  EXPECT_EQ(GetInfo(passport_name, NAME_LAST), u"Name");
}

// Tests that AttributeInstance honors the affix formats.
TEST(AutofillEntityInstanceTest, Attributes_IdentificationNumbers) {
  auto from_affix = [](std::u16string fs) {
    return AutofillFormatString(std::move(fs), FormatString_Type_AFFIX);
  };

  AttributeInstance passport_number((AttributeType(kPassportNumber)));
  passport_number.SetInfo(PASSPORT_NUMBER, u"LR0123456",
                          /*app_locale=*/"", /*format_string=*/std::nullopt,
                          VerificationStatus::kObserved);
  EXPECT_EQ(GetInfo(passport_number, PASSPORT_NUMBER), u"LR0123456");
  EXPECT_EQ(GetInfo(passport_number, PASSPORT_NUMBER,
                    {.format_string = from_affix(u"0")}),
            u"LR0123456");
  EXPECT_EQ(GetInfo(passport_number, PASSPORT_NUMBER,
                    {.format_string = from_affix(u"4")}),
            u"LR01");
  EXPECT_EQ(GetInfo(passport_number, PASSPORT_NUMBER,
                    {.format_string = from_affix(u"-4")}),
            u"3456");
}

// Tests that AttributeInstance appropriately manages dates.
TEST(AutofillEntityInstanceTest, Attributes_Date) {
  auto from_date = [](std::u16string fs) {
    return AutofillFormatString(std::move(fs), FormatString_Type_DATE);
  };

  AttributeInstance passport_name((AttributeType(kPassportIssueDate)));
  passport_name.SetInfo(PASSPORT_ISSUE_DATE, u"2001-02-03",
                        /*app_locale=*/"", from_date(u"YYYY-MM-DD"),
                        VerificationStatus::kNoStatus);

  EXPECT_EQ(GetInfo(passport_name, PASSPORT_ISSUE_DATE), u"2001-02-03");
  EXPECT_EQ(GetInfo(passport_name, PASSPORT_ISSUE_DATE,
                    {.format_string = from_date(u"DD/MM/YYYY")}),
            u"03/02/2001");
}

// Tests that formatting flight numbers works correctly.
TEST(AutofillEntityInstanceTest, AttributesFlightFormat) {
  auto from_flight_number = [](std::u16string fs) {
    return AutofillFormatString(std::move(fs), FormatString_Type_FLIGHT_NUMBER);
  };

  {
    AttributeInstance flight_number(
        (AttributeType(kFlightReservationFlightNumber)));
    flight_number.SetInfo(
        FLIGHT_RESERVATION_FLIGHT_NUMBER, u"LH89", /*app_locale*/ "",
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
    EXPECT_EQ(GetInfo(flight_number, FLIGHT_RESERVATION_FLIGHT_NUMBER),
              u"LH89");
    EXPECT_EQ(GetInfo(flight_number, FLIGHT_RESERVATION_FLIGHT_NUMBER,
                      {.format_string = from_flight_number(u"A")}),
              u"LH");
    EXPECT_EQ(GetInfo(flight_number, FLIGHT_RESERVATION_FLIGHT_NUMBER,
                      {.format_string = from_flight_number(u"N")}),
              u"89");
    EXPECT_EQ(GetInfo(flight_number, FLIGHT_RESERVATION_FLIGHT_NUMBER,
                      {.format_string = from_flight_number(u"F")}),
              u"LH89");
  }

  {
    // A mal-formed flight number.
    AttributeInstance flight_number(
        (AttributeType(kFlightReservationFlightNumber)));
    flight_number.SetInfo(
        FLIGHT_RESERVATION_FLIGHT_NUMBER, u"AA", /*app_locale*/ "",
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
    EXPECT_EQ(GetInfo(flight_number, FLIGHT_RESERVATION_FLIGHT_NUMBER), u"AA");
    EXPECT_EQ(GetInfo(flight_number, FLIGHT_RESERVATION_FLIGHT_NUMBER,
                      {.format_string = from_flight_number(u"A")}),
              u"AA");
    EXPECT_EQ(GetInfo(flight_number, FLIGHT_RESERVATION_FLIGHT_NUMBER,
                      {.format_string = from_flight_number(u"N")}),
              u"AA");
    EXPECT_EQ(GetInfo(flight_number, FLIGHT_RESERVATION_FLIGHT_NUMBER,
                      {.format_string = from_flight_number(u"F")}),
              u"AA");
    EXPECT_EQ(GetInfo(flight_number, FLIGHT_RESERVATION_FLIGHT_NUMBER,
                      {.format_string = from_flight_number(u"F")}),
              u"AA");
  }
}

// Tests that calling `SetInfo` with an ICU date format string causes a CHECK
// failure.
TEST(AutofillEntityInstanceTest, Attributes_SetInfoWithIcuDate_CheckFails) {
  AttributeType type(kFlightReservationDepartureDate);
  AttributeInstance attribute(type);
  EXPECT_DEATH_IF_SUPPORTED(
      attribute.SetInfo(
          FLIGHT_RESERVATION_DEPARTURE_DATE, u"2025-01-01",
          /*app_locale=*/"", /*format_string=*/
          AutofillFormatString(u"YYYY-MM-DD", FormatString_Type_ICU_DATE),
          VerificationStatus::kObserved),
      "");
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

TEST(AutofillEntityInstanceTest, IsSubsetOf_IdenticalEntities) {
  EntityInstance entity1 = test::GetPassportEntityInstance();
  EntityInstance entity2 = test::GetPassportEntityInstance();
  EXPECT_TRUE(entity1.IsSubsetOf(entity2));
}

TEST(AutofillEntityInstanceTest, IsSubsetOf_ProperSubset) {
  EntityInstance entity1 =
      test::GetPassportEntityInstance({.expiry_date = nullptr});
  EntityInstance entity2 = test::GetPassportEntityInstance();
  EXPECT_TRUE(entity1.IsSubsetOf(entity2));
}

TEST(AutofillEntityInstanceTest, IsSubsetOf_NotASubset) {
  EntityInstance entity1 = test::GetPassportEntityInstance();
  EntityInstance entity2 =
      test::GetPassportEntityInstance({.expiry_date = nullptr});
  EXPECT_FALSE(entity1.IsSubsetOf(entity2));
}

TEST(AutofillEntityInstanceTest, IsSubsetOf_DifferentEntityTypes) {
  EntityInstance entity1 = test::GetPassportEntityInstance();
  EntityInstance entity2 = test::GetVehicleEntityInstance();
  EXPECT_FALSE(entity1.IsSubsetOf(entity2));
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

TEST(AutofillEntityInstanceTest, FrecencyOrder_SortEntitiesByFrecency) {
  auto pp_with_random_guid = []() {
    return test::GetPassportEntityInstance(
        {.guid = base::Uuid::GenerateRandomV4().AsLowercaseString()});
  };
  std::vector<EntityInstance> entities = {pp_with_random_guid(),
                                          pp_with_random_guid()};
  auto sort_entities = [&]() {
    EntityInstance::FrecencyOrder comp(base::Time::Now());
    std::ranges::sort(entities, comp);
  };

  // Set first passport as have been used once.
  EntityInstance& top_entity = entities[0];
  top_entity.RecordEntityUsed(test::kJune2017);
  sort_entities();
  EXPECT_EQ(entities[0].guid(), top_entity.guid());

  // Now set second passport as have been used twice. Note that the second use
  // date is the same as the one for the first passport.
  top_entity = entities[1];
  top_entity.RecordEntityUsed(test::kJanuary2017);
  top_entity.RecordEntityUsed(test::kJune2017);
  sort_entities();
  EXPECT_EQ(entities[0].guid(), top_entity.guid());
}

// Tests that frecency override takes precedence over frecency.
TEST(AutofillEntityInstanceTest, FrecencyOrder_EntitiesWithFrecencyOverride) {
  EntityInstance first_flight = test::GetFlightReservationEntityInstance(
      {.departure_time = test::kJune2017});
  EntityInstance second_flight = test::GetFlightReservationEntityInstance(
      {.departure_time = test::kJune2017 + base::Days(1)});
  second_flight.RecordEntityUsed(test::kJune2017);
  std::vector<EntityInstance> entities = {first_flight, second_flight};

  EntityInstance::FrecencyOrder comp(test::kJune2017 + base::Days(2));
  std::ranges::sort(entities, comp);

  EXPECT_EQ(entities[0].guid(), first_flight.guid());
}

// Tests that if one entity has a non-empty frecency override, while other has
// empty override, the non-empty takes precedence.
TEST(AutofillEntityInstanceTest,
     FrecencyOrder_EntityWithFrecencyOverrideTakesPrecedence) {
  EntityInstance flight = test::GetFlightReservationEntityInstance(
      {.departure_time = test::kJune2017});
  EntityInstance passport = test::GetPassportEntityInstance();
  passport.RecordEntityUsed(test::kJune2017);
  std::vector<EntityInstance> entities = {flight, passport};

  EntityInstance::FrecencyOrder comp(test::kJune2017 + base::Days(2));
  std::ranges::sort(entities, comp);

  EXPECT_EQ(entities[0].guid(), flight.guid());
}

TEST(AutofillEntityInstanceTest, AreAttributesReadOnly_ForReadOnlyEntity) {
  EntityInstance entity = test::GetPassportEntityInstance(
      {.are_attributes_read_only =
           EntityInstance::AreAttributesReadOnly{true}});

  EXPECT_TRUE(entity.are_attributes_read_only());
}

TEST(AutofillEntityInstanceTest, AreAttributesReadOnly_ForMutableEntity) {
  EntityInstance entity = test::GetPassportEntityInstance(
      {.are_attributes_read_only =
           EntityInstance::AreAttributesReadOnly{false}});

  EXPECT_FALSE(entity.are_attributes_read_only());
}

TEST(AutofillEntityInstanceTest, FormatFlightDepartureDate) {
  AttributeType type(kFlightReservationDepartureDate);
  AttributeInstance attribute(type);
  attribute.SetInfo(FLIGHT_RESERVATION_DEPARTURE_DATE, u"2025-01-01",
                    /*app_locale=*/"", /*format_string=*/
                    AutofillFormatString(u"YYYY-MM-DD", FormatString_Type_DATE),
                    VerificationStatus::kObserved);
  EXPECT_EQ(GetInfo(attribute, FLIGHT_RESERVATION_DEPARTURE_DATE,
                    {.app_locale = "en_US",
                     .format_string = AutofillFormatString(
                         u"MMM d", FormatString_Type_ICU_DATE)}),
            u"Jan 1");
}

// Tests that the metadata of an entity instance can be updated correctly.
TEST(AutofillEntityInstanceTest, SetMetadata) {
  EntityInstance entity = test::GetPassportEntityInstance();
  EntityInstance::EntityId original_guid = entity.guid();
  // Create new metadata with different values but the same GUID.
  const EntityInstance::EntityMetadata new_metadata{
      .guid = entity.guid(),
      .date_modified = base::Time::Now() + base::Days(1),
      .use_count = entity.use_count() + 1,
      .use_date = base::Time::Now() + base::Days(2)};
  entity.set_metadata(new_metadata);
  EXPECT_EQ(entity.metadata(), new_metadata);
}

// Tests that calling `set_metadata` with a different GUID causes a CHECK
// failure.
TEST(AutofillEntityInstanceTest, SetMetadata_DifferentGuid_CheckFails) {
  EntityInstance entity = test::GetPassportEntityInstance();
  EntityInstance::EntityMetadata new_metadata = entity.metadata();
  new_metadata.guid = EntityInstance::EntityId(base::Uuid::GenerateRandomV4());

  EXPECT_DEATH_IF_SUPPORTED(entity.set_metadata(new_metadata), "");
}

}  // namespace
}  // namespace autofill
