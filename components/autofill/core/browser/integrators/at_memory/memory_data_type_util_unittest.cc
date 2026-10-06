// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"

#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_conversion_util.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/personal_context/proto/features/at_memory.pb.h"
#include "components/personal_context/proto/features/common_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Message;
using ::testing::Ne;

// Tests mapping of proto MemoryDataType enum values to local MemoryDataType
// enums.
TEST(MemoryDataTypeUtilTest, ToMemoryDataTypeMapping) {
  EXPECT_EQ(
      ToMemoryDataType(personal_context::proto::MEMORY_DATA_TYPE_UNSPECIFIED),
      MemoryDataType::kUnknown);
  EXPECT_EQ(ToMemoryDataType(
                personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NUMBER),
            MemoryDataType::kPassportNumber);
  EXPECT_EQ(ToMemoryDataType(
                personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_COUNTRY),
            MemoryDataType::kPassportCountry);
  EXPECT_EQ(
      ToMemoryDataType(personal_context::proto::
                           MEMORY_DATA_TYPE_FLIGHT_RESERVATION_FLIGHT_NUMBER),
      MemoryDataType::kFlightReservationFlightNumber);
  EXPECT_EQ(
      ToMemoryDataType(
          personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_NUMBER),
      MemoryDataType::kDriversLicenseNumber);
  EXPECT_EQ(
      ToMemoryDataType(
          personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_SHIP_DATE),
      MemoryDataType::kShipmentShippedDate);
  EXPECT_EQ(
      ToMemoryDataType(
          personal_context::proto::MEMORY_DATA_TYPE_LOYALTY_MEMBERSHIP_ID),
      MemoryDataType::kLoyaltyMembershipId);
  EXPECT_EQ(
      ToMemoryDataType(
          personal_context::proto::MEMORY_DATA_TYPE_LOYALTY_MEMBERSHIP_PROGRAM),
      MemoryDataType::kLoyaltyMembershipProgram);
  EXPECT_EQ(ToMemoryDataType(personal_context::proto::
                                 MEMORY_DATA_TYPE_LOYALTY_MEMBERSHIP_PROVIDER),
            MemoryDataType::kLoyaltyMembershipProvider);

  // Entity types map to their primary attributes:
  EXPECT_EQ(ToMemoryDataType(personal_context::proto::MEMORY_DATA_TYPE_VEHICLE),
            MemoryDataType::kVehiclePlateNumber);
  EXPECT_EQ(
      ToMemoryDataType(personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_FULL),
      MemoryDataType::kPassportNumber);
}

TEST(MemoryDataTypeUtilTest, ToFieldType) {
  EXPECT_EQ(ToFieldType(MemoryDataType::kNameFull), NAME_FULL);
  EXPECT_EQ(ToFieldType(MemoryDataType::kIban), IBAN_VALUE);
  EXPECT_EQ(ToFieldType(MemoryDataType::kLoyaltyMembershipId),
            LOYALTY_MEMBERSHIP_ID);
  EXPECT_EQ(ToFieldType(MemoryDataType::kLoyaltyMembershipProgram),
            LOYALTY_MEMBERSHIP_PROGRAM);
  EXPECT_EQ(ToFieldType(MemoryDataType::kLoyaltyMembershipProvider),
            LOYALTY_MEMBERSHIP_PROVIDER);
  EXPECT_EQ(ToFieldType(MemoryDataType::kPassportNumber), std::nullopt);
}

TEST(MemoryDataTypeUtilTest, ToAttributeType) {
  EXPECT_EQ(ToAttributeType(MemoryDataType::kPassportNumber),
            AttributeType(AttributeTypeName::kPassportNumber));
  EXPECT_EQ(ToAttributeType(MemoryDataType::kIban), std::nullopt);
}

TEST(MemoryDataTypeUtilTest, ToEntityType) {
  EXPECT_EQ(ToEntityType(MemoryDataType::kPassportNumber),
            EntityType(EntityTypeName::kPassport));
  EXPECT_EQ(ToEntityType(MemoryDataType::kDriversLicenseNumber),
            EntityType(EntityTypeName::kDriversLicense));
  EXPECT_EQ(ToEntityType(MemoryDataType::kNationalIdCardNumber),
            EntityType(EntityTypeName::kNationalIdCard));
  EXPECT_EQ(ToEntityType(MemoryDataType::kFlightReservationArrivalDate),
            EntityType(EntityTypeName::kFlightReservation));
  EXPECT_EQ(ToEntityType(MemoryDataType::kKnownTravelerNumberNumber),
            EntityType(EntityTypeName::kKnownTravelerNumber));
  EXPECT_EQ(ToEntityType(MemoryDataType::kRedressNumberNumber),
            EntityType(EntityTypeName::kRedressNumber));
  EXPECT_EQ(ToEntityType(MemoryDataType::kVehicleMake),
            EntityType(EntityTypeName::kVehicle));
  EXPECT_EQ(ToEntityType(MemoryDataType::kOrderGrandTotal),
            EntityType(EntityTypeName::kOrder));
  EXPECT_EQ(ToEntityType(MemoryDataType::kShipmentDeliveryAddress),
            EntityType(EntityTypeName::kShipment));
  EXPECT_EQ(ToEntityType(MemoryDataType::kShipmentAssociatedOrderId),
            EntityType(EntityTypeName::kShipment));
  EXPECT_EQ(ToEntityType(MemoryDataType::kShipmentEstimatedDeliveryDate),
            EntityType(EntityTypeName::kShipment));
  EXPECT_EQ(ToEntityType(MemoryDataType::kNameFull), std::nullopt);
  EXPECT_EQ(ToEntityType(MemoryDataType::kCreditCardNumber), std::nullopt);
  EXPECT_EQ(ToEntityType(MemoryDataType::kIban), std::nullopt);
  EXPECT_EQ(ToEntityType(MemoryDataType::kLoyaltyMembershipId), std::nullopt);
  EXPECT_EQ(ToEntityType(MemoryDataType::kUnknown), std::nullopt);
}

// Values that were removed from the `MemoryDataType` enum.
constexpr auto kDeprecatedMemoryDataTypeValues =
    base::MakeFixedFlatSet<std::underlying_type_t<MemoryDataType>>(
        {13, 21, 27, 36, 45, 51, 54, 58, 64});

// Tests that the per-type `ToAttributeType()` mapping and the per-category
// `ToEntityType()` mapping cannot drift apart. Every attribute of every entity
// must resolve to that same entity.
TEST(MemoryDataTypeUtilTest, ToEntityTypeIsConsistentWithToAttributeType) {
  for (EntityType entity_type : DenseSet<EntityType>::all()) {
    for (AttributeType attribute_type : entity_type.attributes()) {
      MemoryDataType type = AttributeTypeToMemoryDataType(attribute_type);
      if (type == MemoryDataType::kUnknown) {
        // Not every `AttributeType` has a `MemoryDataType` counterpart.
        continue;
      }
      SCOPED_TRACE(Message() << attribute_type);
      EXPECT_EQ(ToEntityType(type), entity_type);
      EXPECT_EQ(ToAttributeType(type), attribute_type);
    }
  }

  for (std::underlying_type_t<MemoryDataType> value = 1;
       value <= std::to_underlying(MemoryDataType::kMaxValue); ++value) {
    if (kDeprecatedMemoryDataTypeValues.contains(value)) {
      continue;
    }
    const auto type = static_cast<MemoryDataType>(value);
    if (std::optional<AttributeType> mapped_attribute = ToAttributeType(type)) {
      SCOPED_TRACE(Message() << MemoryDataTypeToStringView(type));
      EXPECT_EQ(ToEntityType(type), mapped_attribute->entity_type());
    }
  }
}

// Tests that no `MemoryDataType` renders as an empty string. Types that are
// absent from `entity_schema.json` cannot be resolved via `ToAttributeType()`
// and need their own string.
TEST(MemoryDataTypeUtilTest, EveryTypeHasANonEmptyNameForI18n) {
  // `kUnknown` is 0 and intentionally has no name: such entries carry a
  // free-form `MemorySearchResult::type_name` instead.
  for (std::underlying_type_t<MemoryDataType> value = 1;
       value <= std::to_underlying(MemoryDataType::kMaxValue); ++value) {
    if (kDeprecatedMemoryDataTypeValues.contains(value)) {
      continue;
    }
    const auto type = static_cast<MemoryDataType>(value);
    SCOPED_TRACE(Message() << MemoryDataTypeToStringView(type));
    EXPECT_FALSE(GetMemoryDataTypeNameForI18n(type).empty());
  }
}

TEST(MemoryDataTypeUtilTest, GetMemoryDataTypeCategory) {
  EXPECT_EQ(GetMemoryDataTypeCategory(MemoryDataType::kNameFull),
            MemoryDataTypeCategory::kContactInfo);
  EXPECT_EQ(GetMemoryDataTypeCategory(MemoryDataType::kCreditCardNumber),
            MemoryDataTypeCategory::kCreditCard);
  EXPECT_EQ(GetMemoryDataTypeCategory(MemoryDataType::kIban),
            MemoryDataTypeCategory::kIban);
  EXPECT_EQ(GetMemoryDataTypeCategory(MemoryDataType::kLoyaltyMembershipId),
            MemoryDataTypeCategory::kLoyaltyCard);
  EXPECT_EQ(GetMemoryDataTypeCategory(MemoryDataType::kPassportNumber),
            MemoryDataTypeCategory::kPassport);
  EXPECT_EQ(GetMemoryDataTypeCategory(MemoryDataType::kVehicleMake),
            MemoryDataTypeCategory::kVehicle);
  EXPECT_EQ(GetMemoryDataTypeCategory(MemoryDataType::kOrderId),
            MemoryDataTypeCategory::kOrder);
  EXPECT_EQ(GetMemoryDataTypeCategory(MemoryDataType::kUnknown),
            MemoryDataTypeCategory::kUnknown);
}

TEST(MemoryDataTypeUtilTest, ToAutofillPolicyDataCategory) {
  EXPECT_EQ(ToAutofillPolicyDataCategory(MemoryDataType::kNameFull),
            AutofillClient::AutofillPolicyDataCategory::kContactInfo);
  EXPECT_EQ(ToAutofillPolicyDataCategory(MemoryDataType::kCreditCardNumber),
            AutofillClient::AutofillPolicyDataCategory::kPayments);
  EXPECT_EQ(ToAutofillPolicyDataCategory(MemoryDataType::kIban),
            AutofillClient::AutofillPolicyDataCategory::kPayments);
  EXPECT_EQ(ToAutofillPolicyDataCategory(MemoryDataType::kLoyaltyMembershipId),
            AutofillClient::AutofillPolicyDataCategory::kPayments);
  EXPECT_EQ(ToAutofillPolicyDataCategory(MemoryDataType::kPassportNumber),
            AutofillClient::AutofillPolicyDataCategory::kIdentityDocs);
  EXPECT_EQ(ToAutofillPolicyDataCategory(MemoryDataType::kVehicleMake),
            AutofillClient::AutofillPolicyDataCategory::kTravel);
  EXPECT_EQ(ToAutofillPolicyDataCategory(MemoryDataType::kOrderId),
            AutofillClient::AutofillPolicyDataCategory::kShopping);
  EXPECT_EQ(ToAutofillPolicyDataCategory(MemoryDataType::kUnknown),
            std::nullopt);
}

// Tests extraction of source references (Gmail, Photos) into MemoryEntrySource
// structs.
TEST(MemoryDataTypeUtilTest, ExtractSourcesFromProto) {
  personal_context::proto::AtMemorySearchResult proto_result;
  personal_context::proto::SourceReference* source_gmail =
      proto_result.add_sources();
  source_gmail->mutable_gmail()->mutable_message_urls()->set_desktop_web_url(
      "https://mail.google.com/mail/#inbox/123");
  source_gmail->mutable_gmail()->mutable_message_urls()->set_mobile_web_url(
      "https://mail.google.com/mail/mu/mp/#cv/Inbox/123");

  personal_context::proto::SourceReference* source_photos =
      proto_result.add_sources();
  source_photos->mutable_photos()->set_photos_url(
      "https://photos.google.com/photo/456");

  const std::vector<MemoryEntrySource> sources = ExtractSources(proto_result);
  EXPECT_THAT(
      sources,
      ElementsAre(
          MemoryEntrySource(MemoryEntrySourceType::kGmail,
                            GetGmailSourceUrl(source_gmail->gmail()).spec()),
          MemoryEntrySource(MemoryEntrySourceType::kPhotos,
                            "https://photos.google.com/photo/456")));
}

// Tests conversion of AtMemorySearchResult proto with schemaful primary and
// secondary attributes.
TEST(MemoryDataTypeUtilTest,
     ConvertToMemorySearchResultSchemafulPrimaryAndSecondary) {
  personal_context::proto::AtMemorySearchResult proto_result;
  proto_result.set_relevance_score(0.85f);

  personal_context::proto::Attribute* primary =
      proto_result.mutable_primary_attribute();
  primary->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NUMBER);
  primary->set_value("A12345678");

  personal_context::proto::Attribute* secondary =
      proto_result.add_secondary_attributes();
  secondary->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_COUNTRY);
  secondary->set_value("US");

  MemorySearchResult result =
      ConvertToMemorySearchResult(proto_result, "en-US");
  EXPECT_EQ(result.type, MemoryDataType::kPassportNumber);
  EXPECT_EQ(result.value, u"A12345678");
  EXPECT_EQ(result.confidence_score, 0.85f);
  EXPECT_FALSE(result.is_obfuscated);
  EXPECT_THAT(result.metadata_list,
              ElementsAre(AllOf(
                  Field(&EntryMetadata::type, MemoryDataType::kPassportCountry),
                  Field(&EntryMetadata::value, u"US"))));
}

// Tests conversion of AtMemorySearchResult proto with schemaless key.
TEST(MemoryDataTypeUtilTest, ConvertToMemorySearchResultSchemalessKey) {
  personal_context::proto::AtMemorySearchResult proto_result;
  proto_result.set_relevance_score(0.5f);

  personal_context::proto::Attribute* primary =
      proto_result.mutable_primary_attribute();
  primary->set_schemaless_key("custom_passport_key");
  primary->set_value("CUSTOM_VAL");

  MemorySearchResult result =
      ConvertToMemorySearchResult(proto_result, "en-US");
  EXPECT_EQ(result.type, MemoryDataType::kUnknown);
  EXPECT_EQ(result.type_name, u"custom_passport_key");
  EXPECT_EQ(result.value, u"CUSTOM_VAL");
  EXPECT_FALSE(result.is_obfuscated);
}

// Tests that ExtractRemoteResults converts response results and filters out
// empty values.
TEST(MemoryDataTypeUtilTest, ExtractRemoteResultsFiltersEmptyValues) {
  personal_context::proto::AtMemoryQueryResponse response;

  personal_context::proto::AtMemorySearchResult* valid_result =
      response.add_results();
  valid_result->mutable_primary_attribute()->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_EMAIL);
  valid_result->mutable_primary_attribute()->set_value("test@example.com");

  personal_context::proto::AtMemorySearchResult* empty_result =
      response.add_results();
  empty_result->mutable_primary_attribute()->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_PHONE);
  empty_result->mutable_primary_attribute()->set_value("");

  std::vector<MemorySearchResult> results =
      ExtractRemoteResults(response, "en-US");
  EXPECT_THAT(results,
              ElementsAre(AllOf(
                  Field(&MemorySearchResult::type, MemoryDataType::kEmail),
                  Field(&MemorySearchResult::value, u"test@example.com"),
                  Field(&MemorySearchResult::remote_response_index, 0))));
}

// Tests formatting of Date attribute values into YYYY-MM-DD strings.
TEST(MemoryDataTypeUtilTest, ConvertToMemorySearchResultDate) {
  personal_context::proto::AtMemorySearchResult proto_result;
  personal_context::proto::Attribute* attr =
      proto_result.mutable_primary_attribute();
  attr->set_value("2026 07 24");
  personal_context::proto::Date* date =
      attr->mutable_typed_value()->mutable_date();
  date->set_year(2026);
  date->set_month(7);
  date->set_day(24);

  EXPECT_EQ(ConvertToMemorySearchResult(proto_result, "en-US").value,
            u"2026-07-24");
}

// Tests formatting of DateTime attribute values into YYYY-MM-DD <time> strings.
TEST(MemoryDataTypeUtilTest, ConvertToMemorySearchResultDateTime) {
  personal_context::proto::AtMemorySearchResult proto_result;
  personal_context::proto::Attribute* attr =
      proto_result.mutable_primary_attribute();
  attr->set_value("2026-12-31");
  personal_context::proto::DateTime* date_time =
      attr->mutable_typed_value()->mutable_date_time();
  date_time->set_year(2026);
  date_time->set_month(12);
  date_time->set_day(31);
  date_time->set_hours(14);
  date_time->set_minutes(30);

  // ICU inserts a "narrow space" between the time and the "PM" for 12h locales.
  EXPECT_EQ(ConvertToMemorySearchResult(proto_result, "en-US").value,
            u"2026-12-31 2:30\u202FPM");
  // 24h locale formatting (German).
  EXPECT_EQ(ConvertToMemorySearchResult(proto_result, "de").value,
            u"2026-12-31 14:30");
}

// Tests formatting of DateTime attribute values when FromLocalExploded fails
// due to an invalid time.
TEST(MemoryDataTypeUtilTest,
     ConvertToMemorySearchResultDateTimeInvalidTimeFallback) {
  personal_context::proto::AtMemorySearchResult proto_result;
  personal_context::proto::Attribute* attr =
      proto_result.mutable_primary_attribute();
  attr->set_value("2026-12-31 25:00");
  personal_context::proto::DateTime* date_time =
      attr->mutable_typed_value()->mutable_date_time();
  date_time->set_year(2026);
  date_time->set_month(12);
  date_time->set_day(31);
  date_time->set_hours(25);  // Invalid hour.
  date_time->set_minutes(0);

  EXPECT_EQ(ConvertToMemorySearchResult(proto_result, "en-US").value,
            u"2026-12-31");
}

// Tests formatting of ISO country code attribute values.
TEST(MemoryDataTypeUtilTest, ConvertToMemorySearchResultCountryCode) {
  personal_context::proto::AtMemorySearchResult proto_result;
  personal_context::proto::Attribute* attr =
      proto_result.mutable_primary_attribute();
  attr->mutable_typed_value()->set_country_code("DE");

  EXPECT_EQ(ConvertToMemorySearchResult(proto_result, "en-US").value,
            u"Germany");
}

// Tests formatting of ISO country code attribute values using a non-English
// app_locale.
TEST(MemoryDataTypeUtilTest, ConvertToMemorySearchResultCountryCodeWithLocale) {
  personal_context::proto::AtMemorySearchResult proto_result;
  personal_context::proto::Attribute* attr =
      proto_result.mutable_primary_attribute();
  attr->mutable_typed_value()->set_country_code("DE");

  EXPECT_EQ(ConvertToMemorySearchResult(proto_result, "de").value,
            u"Deutschland");
}

// Tests formatting of lowercase ISO country code attribute values.
TEST(MemoryDataTypeUtilTest, ConvertToMemorySearchResultCountryCodeLowercase) {
  personal_context::proto::AtMemorySearchResult proto_result;
  personal_context::proto::Attribute* attr =
      proto_result.mutable_primary_attribute();
  attr->mutable_typed_value()->set_country_code("de");

  EXPECT_EQ(ConvertToMemorySearchResult(proto_result, "en-US").value,
            u"Germany");
}

// Tests fallback to untyped value when country code is invalid.
TEST(MemoryDataTypeUtilTest, ConvertToMemorySearchResultCountryCodeFallback) {
  personal_context::proto::AtMemorySearchResult proto_result;
  personal_context::proto::Attribute* attr =
      proto_result.mutable_primary_attribute();
  attr->set_value("Untyped Fallback");
  attr->mutable_typed_value()->set_country_code("INVALID");

  EXPECT_EQ(ConvertToMemorySearchResult(proto_result, "en-US").value,
            u"Untyped Fallback");
}

// Tests formatting of StringList attribute values joined by commas.
TEST(MemoryDataTypeUtilTest, ConvertToMemorySearchResultStringList) {
  personal_context::proto::AtMemorySearchResult proto_result;
  personal_context::proto::Attribute* attr =
      proto_result.mutable_primary_attribute();
  personal_context::proto::StringList* list =
      attr->mutable_typed_value()->mutable_string_list();
  list->add_values("Item 1");
  list->add_values("Item 2");

  EXPECT_EQ(ConvertToMemorySearchResult(proto_result, "en-US").value,
            u"Item 1, Item 2");
}

// Tests fallback to untyped string value when typed_value is unset.
TEST(MemoryDataTypeUtilTest, ConvertToMemorySearchResultFallback) {
  personal_context::proto::AtMemorySearchResult proto_result;
  personal_context::proto::Attribute* attr =
      proto_result.mutable_primary_attribute();
  attr->set_value("Fallback String");

  EXPECT_EQ(ConvertToMemorySearchResult(proto_result, "en-US").value,
            u"Fallback String");
}

// Tests that ConvertToMemorySearchResult formats typed_value on primary and
// secondary attributes.
TEST(MemoryDataTypeUtilTest, ConvertToMemorySearchResultFormatsTypedValue) {
  personal_context::proto::AtMemorySearchResult proto_result;

  personal_context::proto::Attribute* primary =
      proto_result.mutable_primary_attribute();
  primary->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_EXPIRATION_DATE);
  primary->mutable_typed_value()->mutable_date()->set_year(2030);
  primary->mutable_typed_value()->mutable_date()->set_month(5);
  primary->mutable_typed_value()->mutable_date()->set_day(20);

  personal_context::proto::Attribute* secondary =
      proto_result.add_secondary_attributes();
  secondary->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_COUNTRY);
  secondary->mutable_typed_value()->set_country_code("FR");

  MemorySearchResult result =
      ConvertToMemorySearchResult(proto_result, "en-US");
  EXPECT_EQ(result.value, u"2030-05-20");
  EXPECT_TRUE(result.typed_value.has_value());
  EXPECT_THAT(result.metadata_list,
              ElementsAre(Field(&EntryMetadata::value, u"France")));
  EXPECT_THAT(
      result.metadata_list,
      ElementsAre(Field(&EntryMetadata::typed_value, Ne(std::nullopt))));
}

// Tests that `FormatMemoryDataTypeLabelValue` formats flight departure and
// arrival dates into a short "MMM d" string (e.g. "Jun 7") when provided with a
// Date or DateTime TypedValue or a string fallback.
TEST(MemoryDataTypeUtilTest, FormatMemoryDataTypeLabelValueFlightDate) {
  personal_context::proto::TypedValue date_typed;
  date_typed.mutable_date()->set_year(2024);
  date_typed.mutable_date()->set_month(6);
  date_typed.mutable_date()->set_day(7);

  // FormatMemoryDataTypeLabelValue formats flight dates as "MMM d" for labels
  EXPECT_EQ(FormatMemoryDataTypeLabelValue(
                MemoryDataType::kFlightReservationDepartureDate, u"2024-06-07",
                date_typed, "en-US"),
            u"Jun 7");

  // Localized date formatting (German and Polish).
  EXPECT_EQ(FormatMemoryDataTypeLabelValue(
                MemoryDataType::kFlightReservationDepartureDate, u"2024-06-07",
                date_typed, "de"),
            u"7. Juni");
  EXPECT_EQ(FormatMemoryDataTypeLabelValue(
                MemoryDataType::kFlightReservationDepartureDate, u"2024-06-07",
                date_typed, "pl"),
            u"7 cze");

  personal_context::proto::TypedValue datetime_typed;
  datetime_typed.mutable_date_time()->set_year(2024);
  datetime_typed.mutable_date_time()->set_month(6);
  datetime_typed.mutable_date_time()->set_day(7);
  datetime_typed.mutable_date_time()->set_hours(15);
  datetime_typed.mutable_date_time()->set_minutes(30);

  EXPECT_EQ(FormatMemoryDataTypeLabelValue(
                MemoryDataType::kFlightReservationArrivalDate,
                u"2024-06-07 3:30 PM", datetime_typed, "en-US"),
            u"Jun 7");

  // String fallback test with space separator
  EXPECT_EQ(FormatMemoryDataTypeLabelValue(
                MemoryDataType::kFlightReservationDepartureDate,
                u"2024-06-07 3:30 PM", std::nullopt, "en-US"),
            u"Jun 7");

  // String fallback test with 'T' separator
  EXPECT_EQ(FormatMemoryDataTypeLabelValue(
                MemoryDataType::kFlightReservationArrivalDate,
                u"2024-06-07T03:30:39", std::nullopt, "en-US"),
            u"Jun 7");

  // Non-flight date types should return the value untouched
  EXPECT_EQ(FormatMemoryDataTypeLabelValue(
                MemoryDataType::kFlightReservationPassengerName, u"John Doe",
                std::nullopt, "en-US"),
            u"John Doe");
}

// Tests equality comparison between TypedValue proto messages using operator==.
TEST(MemoryDataTypeUtilTest, TypedValueEquality) {
  personal_context::proto::TypedValue a;
  a.mutable_date()->set_year(2024);
  a.mutable_date()->set_month(6);
  a.mutable_date()->set_day(7);

  personal_context::proto::TypedValue b = a;
  personal_context::proto::TypedValue c;
  c.set_country_code("US");

  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

}  // namespace
}  // namespace autofill
