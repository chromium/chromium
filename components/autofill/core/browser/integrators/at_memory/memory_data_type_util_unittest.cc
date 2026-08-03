// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"

#include <vector>

#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#include "components/personal_context/proto/features/at_memory.pb.h"
#include "components/personal_context/proto/features/common_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Ne;

// Tests that `ToPersonalContextEntity` correctly converts individual memory
// entry attributes and metadata into the corresponding fields of the personal
// context Entity proto message.
TEST(MemoryDataTypeUtilTest, ToPersonalContextEntity) {
  // Test Passport conversion.
  {
    std::u16string value = u"P12345";
    MemoryDataType memory_data_type = MemoryDataType::kPassportNumber;

    std::vector<EntryMetadata> metadata_list;
    metadata_list.emplace_back(MemoryDataType::kPassportName, u"Passport Name",
                               u"Jane Doe");
    metadata_list.emplace_back(MemoryDataType::kPassportCountry,
                               u"Passport Country", u"US");
    metadata_list.emplace_back(MemoryDataType::kPassportExpirationDate,
                               u"Passport Expiration Date", u"2030-05-20");

    personal_context::proto::Entity entity =
        ToPersonalContextEntity(value, memory_data_type, metadata_list);

    ASSERT_TRUE(entity.has_passport());
    EXPECT_EQ(entity.passport().number(), "P12345");
    EXPECT_EQ(entity.passport().name(), "Jane Doe");
    EXPECT_EQ(entity.passport().issuing_country(), "US");
    EXPECT_EQ(entity.passport().expiration_date().year(), 2030);
    EXPECT_EQ(entity.passport().expiration_date().month(), 5);
    EXPECT_EQ(entity.passport().expiration_date().day(), 20);
  }

  // Test Order conversion, including order date parsing and product names list.
  {
    std::u16string value = u"ORD-123";
    MemoryDataType memory_data_type = MemoryDataType::kOrderId;

    std::vector<EntryMetadata> metadata_list;
    metadata_list.emplace_back(MemoryDataType::kOrderDate, u"Order Date",
                               u"2026-07-07");
    metadata_list.emplace_back(MemoryDataType::kOrderProductNames, u"Products",
                               u"Book A, Toy B");

    personal_context::proto::Entity entity =
        ToPersonalContextEntity(value, memory_data_type, metadata_list);

    ASSERT_TRUE(entity.has_order());
    EXPECT_EQ(entity.order().order_id(), "ORD-123");
    EXPECT_EQ(entity.order().order_date().year(), 2026);
    EXPECT_EQ(entity.order().order_date().month(), 7);
    EXPECT_EQ(entity.order().order_date().day(), 7);
    ASSERT_EQ(entity.order().product_names_size(), 2);
    EXPECT_EQ(entity.order().product_names(0), "Book A");
    EXPECT_EQ(entity.order().product_names(1), "Toy B");
  }

  // Test Shipment conversion, including associated order IDs parsing and
  // shipped date parsing.
  {
    std::u16string value = u"TRACK-888";
    MemoryDataType memory_data_type = MemoryDataType::kShipmentTrackingNumber;

    std::vector<EntryMetadata> metadata_list;
    metadata_list.emplace_back(MemoryDataType::kShipmentAssociatedOrderId,
                               u"Order IDs", u"ORD-001, ORD-002");
    metadata_list.emplace_back(MemoryDataType::kShipmentShippedDate,
                               u"Ship Date", u"2026-07-07");

    personal_context::proto::Entity entity =
        ToPersonalContextEntity(value, memory_data_type, metadata_list);

    ASSERT_TRUE(entity.has_shipment());
    EXPECT_EQ(entity.shipment().tracking_number(), "TRACK-888");
    ASSERT_EQ(entity.shipment().associated_order_ids_size(), 2);
    EXPECT_EQ(entity.shipment().associated_order_ids(0), "ORD-001");
    EXPECT_EQ(entity.shipment().associated_order_ids(1), "ORD-002");
    EXPECT_EQ(entity.shipment().ship_date().year(), 2026);
    EXPECT_EQ(entity.shipment().ship_date().month(), 7);
    EXPECT_EQ(entity.shipment().ship_date().day(), 7);
  }
}

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

  // Entity types map to their primary attributes:
  EXPECT_EQ(ToMemoryDataType(personal_context::proto::MEMORY_DATA_TYPE_VEHICLE),
            MemoryDataType::kVehiclePlateNumber);
  EXPECT_EQ(
      ToMemoryDataType(personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_FULL),
      MemoryDataType::kPassportNumber);
}

// Tests extraction of source references (Gmail, Photos) into MemoryEntrySource
// structs.
TEST(MemoryDataTypeUtilTest, ExtractSourcesFromProto) {
  personal_context::proto::AtMemorySearchResult proto_result;
  personal_context::proto::SourceReference* source_gmail =
      proto_result.add_sources();
  source_gmail->mutable_gmail()->set_message_url(
      "https://mail.google.com/mail/u/0/#inbox/123");

  personal_context::proto::SourceReference* source_photos =
      proto_result.add_sources();
  source_photos->mutable_photos()->set_photos_url(
      "https://photos.google.com/photo/456");

  const std::vector<MemoryEntrySource> sources = ExtractSources(proto_result);
  ASSERT_EQ(sources.size(), 2u);
  EXPECT_EQ(sources[0].type, MemoryEntrySourceType::kGmail);
  EXPECT_EQ(sources[0].deeplink_url,
            "https://mail.google.com/mail/u/0/#inbox/123");
  EXPECT_EQ(sources[1].type, MemoryEntrySourceType::kPhotos);
  EXPECT_EQ(sources[1].deeplink_url, "https://photos.google.com/photo/456");
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
  EXPECT_TRUE(result.is_obfuscated);
  ASSERT_EQ(result.metadata_list.size(), 1u);
  EXPECT_EQ(result.metadata_list[0].type, MemoryDataType::kPassportCountry);
  EXPECT_EQ(result.metadata_list[0].value, u"US");
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
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].type, MemoryDataType::kEmail);
  EXPECT_EQ(results[0].value, u"test@example.com");
  EXPECT_EQ(results[0].remote_response_index, 0);
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

  // ICU inserts a "narrow space" between the time and the "PM".
  EXPECT_EQ(ConvertToMemorySearchResult(proto_result, "en-US").value,
            u"2026-12-31 2:30\u202FPM");
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
                date_typed),
            u"Jun 7");

  personal_context::proto::TypedValue datetime_typed;
  datetime_typed.mutable_date_time()->set_year(2024);
  datetime_typed.mutable_date_time()->set_month(6);
  datetime_typed.mutable_date_time()->set_day(7);
  datetime_typed.mutable_date_time()->set_hours(15);
  datetime_typed.mutable_date_time()->set_minutes(30);

  EXPECT_EQ(FormatMemoryDataTypeLabelValue(
                MemoryDataType::kFlightReservationArrivalDate,
                u"2024-06-07 3:30 PM", datetime_typed),
            u"Jun 7");

  // String fallback test with space separator
  EXPECT_EQ(FormatMemoryDataTypeLabelValue(
                MemoryDataType::kFlightReservationDepartureDate,
                u"2024-06-07 3:30 PM", std::nullopt),
            u"Jun 7");

  // String fallback test with 'T' separator
  EXPECT_EQ(FormatMemoryDataTypeLabelValue(
                MemoryDataType::kFlightReservationArrivalDate,
                u"2024-06-07T03:30:39", std::nullopt),
            u"Jun 7");

  // Non-flight date types should return the value untouched
  EXPECT_EQ(FormatMemoryDataTypeLabelValue(
                MemoryDataType::kFlightReservationPassengerName, u"John Doe",
                std::nullopt),
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
