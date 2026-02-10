// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance_test_api.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
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

class AutofillEntityInstanceTest : public base::test::WithFeatureOverride,
                                   public testing::Test {
 public:
  AutofillEntityInstanceTest()
      : base::test::WithFeatureOverride(
            features::kAutofillAiWalletPrivatePasses) {}
};

TEST_P(AutofillEntityInstanceTest, MaskedAttribute) {
  AttributeInstance attribute((AttributeType(kPassportNumber)));
  EXPECT_FALSE(attribute.masked());

  test_api(attribute).mark_as_masked();
  EXPECT_TRUE(attribute.masked());
}

TEST_P(AutofillEntityInstanceTest, MaskedServerEntityWithMaskedAttributes) {
  AttributeInstance attribute((AttributeType(kPassportNumber)));
  ASSERT_TRUE(attribute.type().is_obfuscated());
  test_api(attribute).mark_as_masked();

  EntityInstance entity = test::GetEntityInstance(
      {attribute}, {.record_type = EntityInstance::RecordType::kServerWallet});
  EXPECT_TRUE(entity.IsMaskedServerEntity());
  EXPECT_FALSE(entity.IsUnmaskedServerEntity());

  // Local entities must never have masked attributes.
  EntityInstance invalid_entity = test::GetEntityInstance(
      {attribute}, {.record_type = EntityInstance::RecordType::kLocal});
  EXPECT_CHECK_DEATH(invalid_entity.IsMaskedServerEntity());
}

TEST_P(AutofillEntityInstanceTest, NeitherMaskedNorUnmaskedServerEntity) {
  AttributeInstance attribute((AttributeType(kPassportNumber)));
  ASSERT_TRUE(attribute.type().is_obfuscated());

  EntityInstance entity = test::GetEntityInstance(
      {attribute}, {.record_type = EntityInstance::RecordType::kLocal});
  EXPECT_FALSE(entity.IsMaskedServerEntity());
  EXPECT_FALSE(entity.IsUnmaskedServerEntity());
}

TEST_P(AutofillEntityInstanceTest, ServerEntityWithoutObfuscatedAttributes) {
  AttributeInstance attribute((AttributeType(kPassportName)));
  ASSERT_FALSE(attribute.type().is_obfuscated());

  EntityInstance entity = test::GetEntityInstance(
      {attribute}, {.record_type = EntityInstance::RecordType::kServerWallet});
  EXPECT_FALSE(entity.IsMaskedServerEntity());
  EXPECT_FALSE(entity.IsUnmaskedServerEntity());
}

TEST_P(AutofillEntityInstanceTest, ServerEntityWithUnmaskedAttributes) {
  AttributeInstance attribute((AttributeType(kPassportNumber)));
  ASSERT_TRUE(attribute.type().is_obfuscated());

  EntityInstance entity = test::GetEntityInstance(
      {attribute}, {.record_type = EntityInstance::RecordType::kServerWallet});
  EXPECT_FALSE(entity.IsMaskedServerEntity());
  EXPECT_TRUE(entity.IsUnmaskedServerEntity());
}

TEST_P(AutofillEntityInstanceTest, Attributes) {
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
TEST_P(AutofillEntityInstanceTest, Attributes_NormalizedType) {
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
TEST_P(AutofillEntityInstanceTest, Attributes_CountryLocalization) {
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
TEST_P(AutofillEntityInstanceTest, Attributes_StructuredName) {
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
TEST_P(AutofillEntityInstanceTest, Attributes_IdentificationNumbers) {
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
TEST_P(AutofillEntityInstanceTest, Attributes_Date) {
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
TEST_P(AutofillEntityInstanceTest, AttributesFlightFormat) {
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
TEST_P(AutofillEntityInstanceTest, Attributes_SetInfoWithIcuDate_CheckFails) {
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

TEST_P(
    AutofillEntityInstanceTest,
    GetEntityMergeability_IdentiticalEntities_NoMergeableAttribute_IsASubset) {
  EntityInstance::EntityMergeability result =
      test::GetPassportEntityInstance().GetEntityMergeability(
          test::GetPassportEntityInstance());
  EXPECT_TRUE(result.mergeable_attributes.empty());
  EXPECT_TRUE(result.is_subset);
}

TEST_P(
    AutofillEntityInstanceTest,
    GetEntityMergeability_NewEntityMissingAttribute_NoMergeableAttributes_IsASubset) {
  EntityInstance::EntityMergeability result =
      test::GetPassportEntityInstance().GetEntityMergeability(
          test::GetPassportEntityInstance({.expiry_date = nullptr}));

  EXPECT_TRUE(result.mergeable_attributes.empty());
  EXPECT_TRUE(result.is_subset);
}
TEST_P(
    AutofillEntityInstanceTest,
    GetEntityMergeability_NewEntityEmptyStringAttribute_NoMergeablesAttribute_IsASubset) {
  EntityInstance::EntityMergeability result =
      test::GetPassportEntityInstance().GetEntityMergeability(
          test::GetPassportEntityInstance({.expiry_date = nullptr}));

  EXPECT_TRUE(result.mergeable_attributes.empty());
  EXPECT_TRUE(result.is_subset);
}

TEST_P(
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
TEST_P(
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

TEST_P(
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

TEST_P(
    AutofillEntityInstanceTest,
    GetEntityMergeability_NewEntityHasEquivalentAttribute_MergeableAttributesDoNotExists_IsASubset) {
  EntityInstance::EntityMergeability result =
      test::GetPassportEntityInstance({.number = u"1234 5"})
          .GetEntityMergeability(
              test::GetPassportEntityInstance({.number = u"1234    5"}));

  EXPECT_TRUE(result.mergeable_attributes.empty());
  EXPECT_TRUE(result.is_subset);
}

TEST_P(AutofillEntityInstanceTest, IsSubsetOf_IdenticalEntities) {
  EntityInstance entity1 = test::GetPassportEntityInstance();
  EntityInstance entity2 = test::GetPassportEntityInstance();
  EXPECT_TRUE(entity1.IsSubsetOf(entity2));
}

TEST_P(AutofillEntityInstanceTest, IsSubsetOf_ProperSubset) {
  EntityInstance entity1 =
      test::GetPassportEntityInstance({.expiry_date = nullptr});
  EntityInstance entity2 = test::GetPassportEntityInstance();
  EXPECT_TRUE(entity1.IsSubsetOf(entity2));
}

TEST_P(AutofillEntityInstanceTest, IsSubsetOf_NotASubset) {
  EntityInstance entity1 = test::GetPassportEntityInstance();
  EntityInstance entity2 =
      test::GetPassportEntityInstance({.expiry_date = nullptr});
  EXPECT_FALSE(entity1.IsSubsetOf(entity2));
}

TEST_P(AutofillEntityInstanceTest, IsSubsetOf_DifferentEntityTypes) {
  EntityInstance entity1 = test::GetPassportEntityInstance();
  EntityInstance entity2 = test::GetVehicleEntityInstance();
  EXPECT_FALSE(entity1.IsSubsetOf(entity2));
}

// Tests that valid entities that don't share any attribute in common are not
// merged, as we cannot verify that they represent the same real-world object.
TEST_P(AutofillEntityInstanceTest, GetEntityMergeability_EntitiesAreDisjoint) {
  EntityInstance::EntityMergeability result =
      test::GetVehicleEntityInstance({.number = u"12345"})
          .GetEntityMergeability(
              test::GetVehicleEntityInstance({.plate = u"6789"}));

  EXPECT_TRUE(result.mergeable_attributes.empty());
  EXPECT_FALSE(result.is_subset);
}

TEST_P(AutofillEntityInstanceTest, FrecencyOrder_SortEntitiesByFrecency) {
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
TEST_P(AutofillEntityInstanceTest, FrecencyOrder_EntitiesWithFrecencyOverride) {
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
TEST_P(AutofillEntityInstanceTest,
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

TEST_P(AutofillEntityInstanceTest, AreAttributesReadOnly_ForReadOnlyEntity) {
  EntityInstance entity = test::GetPassportEntityInstance(
      {.are_attributes_read_only =
           EntityInstance::AreAttributesReadOnly{true}});

  EXPECT_TRUE(entity.are_attributes_read_only());
}

TEST_P(AutofillEntityInstanceTest, AreAttributesReadOnly_ForMutableEntity) {
  EntityInstance entity = test::GetPassportEntityInstance(
      {.are_attributes_read_only =
           EntityInstance::AreAttributesReadOnly{false}});

  EXPECT_FALSE(entity.are_attributes_read_only());
}

TEST_P(AutofillEntityInstanceTest, FormatFlightDepartureDate) {
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
TEST_P(AutofillEntityInstanceTest, SetMetadata) {
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
TEST_P(AutofillEntityInstanceTest, SetMetadata_DifferentGuid_CheckFails) {
  EntityInstance entity = test::GetPassportEntityInstance();
  EntityInstance::EntityMetadata new_metadata = entity.metadata();
  new_metadata.guid = EntityInstance::EntityId(base::Uuid::GenerateRandomV4());

  EXPECT_DEATH_IF_SUPPORTED(entity.set_metadata(new_metadata), "");
}

// Tests that masked attributes only require a suffix match if the feature is
// enabled.
TEST_P(AutofillEntityInstanceTest, IsSubsetOf_MaskedAttributes) {
  AttributeInstance a1((AttributeType(kPassportNumber)));
  a1.SetRawInfo(PASSPORT_NUMBER, u"LR0123456", VerificationStatus::kNoStatus);

  AttributeInstance a2((AttributeType(kPassportNumber)));
  a2.SetRawInfo(PASSPORT_NUMBER, u"3456", VerificationStatus::kNoStatus);
  test_api(a2).mark_as_masked();

  EntityInstance entity1 = test::GetEntityInstance(
      {a1}, {.record_type = EntityInstance::RecordType::kServerWallet});
  EntityInstance entity2 = test::GetEntityInstance(
      {a2}, {.record_type = EntityInstance::RecordType::kServerWallet});

  EXPECT_EQ(entity2.IsSubsetOf(entity1), IsParamFeatureEnabled());
}

// Tests that GetEntityMergeability also honors the suffix match for masked
// attributes if the feature is enabled.
TEST_P(AutofillEntityInstanceTest, GetEntityMergeability_MaskedAttributes) {
  // Passport merge constraints include the passport number.
  AttributeInstance a1_num((AttributeType(kPassportNumber)));
  a1_num.SetRawInfo(PASSPORT_NUMBER, u"LR0123456",
                    VerificationStatus::kNoStatus);
  AttributeInstance a1_exp((AttributeType(kPassportExpirationDate)));
  a1_exp.SetRawInfo(PASSPORT_EXPIRATION_DATE, u"2030-01-01",
                    VerificationStatus::kNoStatus);

  AttributeInstance a2_num((AttributeType(kPassportNumber)));
  a2_num.SetRawInfo(PASSPORT_NUMBER, u"3456", VerificationStatus::kNoStatus);
  test_api(a2_num).mark_as_masked();
  AttributeInstance a2_exp((AttributeType(kPassportExpirationDate)));
  a2_exp.SetRawInfo(PASSPORT_EXPIRATION_DATE, u"2030-01-01",
                    VerificationStatus::kNoStatus);

  EntityInstance entity1 = test::GetEntityInstance(
      {a1_num, a1_exp},
      {.record_type = EntityInstance::RecordType::kServerWallet});
  EntityInstance entity2 = test::GetEntityInstance(
      {a2_num, a2_exp},
      {.record_type = EntityInstance::RecordType::kServerWallet});

  // They should be considered the same entity because their merge constraints
  // (number and expiry) match (via suffix) if the feature is enabled.
  EntityInstance::EntityMergeability result =
      entity1.GetEntityMergeability(entity2);
  EXPECT_EQ(result.is_subset, IsParamFeatureEnabled());
}

// Tests that if both attributes are masked, they compare as unequal if they
// are not the same.
TEST_P(AutofillEntityInstanceTest, IsSubsetOf_BothMasked_OneIsSuffixOfOther) {
  AttributeInstance a1((AttributeType(kPassportNumber)));
  a1.SetRawInfo(PASSPORT_NUMBER, u"LR0123456", VerificationStatus::kNoStatus);
  test_api(a1).mark_as_masked();

  AttributeInstance a2((AttributeType(kPassportNumber)));
  a2.SetRawInfo(PASSPORT_NUMBER, u"3456", VerificationStatus::kNoStatus);
  test_api(a2).mark_as_masked();

  EntityInstance entity1 = test::GetEntityInstance(
      {a1}, {.record_type = EntityInstance::RecordType::kServerWallet});
  EntityInstance entity2 = test::GetEntityInstance(
      {a2}, {.record_type = EntityInstance::RecordType::kServerWallet});

  EXPECT_FALSE(entity1.IsSubsetOf(entity2));
  EXPECT_FALSE(entity2.IsSubsetOf(entity1));
}

// Tests that entity types that support masked storage have at least one
// obfuscated attribute. Masked storage only makes sense for entities that have
// obfuscated attributes since all unobfuscated attributes are already
// transmitted via sync and therefore stored locally.
TEST_P(AutofillEntityInstanceTest, IsMaskedStorageSupported) {
  for (EntityType t : DenseSet<EntityType>::all()) {
    EXPECT_TRUE(
        !IsMaskedStorageSupported(t,
                                  EntityInstance::RecordType::kServerWallet) ||
        std::ranges::any_of(t.attributes(),
                            [](AttributeType a) { return a.is_obfuscated(); }))
        << t;
    EXPECT_FALSE(
        IsMaskedStorageSupported(t, EntityInstance::RecordType::kLocal))
        << t;
  }
}

// Tests explicitly for some entity types that they support masked storage.
TEST_P(AutofillEntityInstanceTest, IsMaskedStorageSupportedSelectTypes) {
  using enum EntityTypeName;
  EXPECT_TRUE(IsMaskedStorageSupported(
      EntityType(kDriversLicense), EntityInstance::RecordType::kServerWallet));
  EXPECT_TRUE(
      IsMaskedStorageSupported(EntityType(kKnownTravelerNumber),
                               EntityInstance::RecordType::kServerWallet));
  EXPECT_TRUE(IsMaskedStorageSupported(
      EntityType(kNationalIdCard), EntityInstance::RecordType::kServerWallet));
  EXPECT_TRUE(IsMaskedStorageSupported(
      EntityType(kPassport), EntityInstance::RecordType::kServerWallet));
  EXPECT_TRUE(IsMaskedStorageSupported(
      EntityType(kRedressNumber), EntityInstance::RecordType::kServerWallet));
}

// Tests that all obfuscated attributes of entity types that can be stored in
// Wallet are part of every import constraint.
//
// The reason for this test is as follows:
// - When importing data from a form submission, we assemble the entity that we
//   send to Wallet by augmenting the observed submission data with data stored
//   locally for that entity.
// - However, we never want to send a masked attribute to Wallet: If we did,
//   then Wallet would overwrite the attribute's value with the masked value.
// - We can guarantee that this never happens if we only offer to update if all
//   of the obfuscated attributes were present in the submitted form.
//
// Should this test start to fail, then the form import logic must be updated.
// For example, you might need to fetch the unmasked entity from the Wallet
// server before sending the update request.
TEST_P(AutofillEntityInstanceTest, ObfuscatedAttributesAreImportonstraints) {
  for (const EntityType entity_type : DenseSet<EntityType>::all()) {
    if (!IsMaskedStorageSupported(entity_type,
                                  EntityInstance::RecordType::kServerWallet)) {
      continue;
    }
    for (const AttributeType attribute_type : entity_type.attributes()) {
      if (!attribute_type.is_obfuscated()) {
        continue;
      }
      EXPECT_TRUE(std::ranges::all_of(entity_type.import_constraints(),
                                      [&](DenseSet<AttributeType> constraint) {
                                        return constraint.contains(
                                            attribute_type);
                                      }))
          << attribute_type << " must appear in all import constraints of "
          << entity_type;
    }
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(AutofillEntityInstanceTest);

}  // namespace
}  // namespace autofill
