// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/from_accessibility_annotator.h"

#include <optional>
#include <vector>

#include "base/time/time.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/common/dense_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Optional;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

namespace aa = accessibility_annotator;

Matcher<const AttributeInstance&> IsAttribute(AttributeType type,
                                              std::u16string value) {
  return AllOf(Property(&AttributeInstance::type, type),
               Property(&AttributeInstance::GetCompleteRawInfo, value));
}

template <typename... Matchers>
  requires((std::convertible_to<Matchers, Matcher<const AttributeInstance&>>) &&
           ...)
Matcher<base::span<const AttributeInstance>> AttributesAre(
    Matchers... attribute_matchers) {
  return UnorderedElementsAre(attribute_matchers...);
}

Matcher<const EntityInstance&> IsEntity(
    Matcher<const EntityType&> type,
    Matcher<const EntityInstance::EntityId&> guid,
    Matcher<base::span<const AttributeInstance>> attributes_matchers) {
  return AllOf(Property(&EntityInstance::type, type),
               Property(&EntityInstance::guid, guid),
               Property(&EntityInstance::attributes, attributes_matchers));
}

// Tests that kAllEntityTypesSharedWithAccessibilityAnnotator contains the
// EntityTypes for which FromAccessibilityAnnotator() is not std::nullopt.
TEST(FromAccessibilityAnnotatorTest,
     kAllEntityTypesSharedWithAccessibilityAnnotator) {
  EXPECT_THAT(kAllEntityTypesSharedWithAccessibilityAnnotator,
              UnorderedElementsAre(
                  aa::EntityType::kFlightReservation, aa::EntityType::kOrder,
                  aa::EntityType::kShipment, aa::EntityType::kDriversLicense,
                  aa::EntityType::kPassport, aa::EntityType::kNationalId,
                  aa::EntityType::kVehicle))
      << "When a new EntityType is added to Accessibility Annotator and this "
         "EntityType is supported by Autofill, it must be here.";
}

// Tests that a set of accessibility annotator entity types is correctly
// mapped to a set of Autofill AI entity types.
TEST(FromAccessibilityAnnotatorTest, EntityTypeEnumSet) {
  EXPECT_THAT(FromAccessibilityAnnotator(aa::EntityTypeEnumSet{}), IsEmpty());
  EXPECT_THAT(
      FromAccessibilityAnnotator({aa::EntityType::kFlightReservation,
                                  aa::EntityType::kOrder,
                                  aa::EntityType::kDriversLicense}),
      UnorderedElementsAre(EntityType(EntityTypeName::kFlightReservation),
                           EntityType(EntityTypeName::kOrder),
                           EntityType(EntityTypeName::kDriversLicense)));
  EXPECT_THAT(
      FromAccessibilityAnnotator(aa::EntityTypeEnumSet::All()),
      UnorderedElementsAre(EntityType(EntityTypeName::kFlightReservation),
                           EntityType(EntityTypeName::kOrder),
                           EntityType(EntityTypeName::kShipment),
                           EntityType(EntityTypeName::kDriversLicense),
                           EntityType(EntityTypeName::kNationalIdCard),
                           EntityType(EntityTypeName::kPassport),
                           EntityType(EntityTypeName::kVehicle)))
      << "When a new EntityType is added to Accessibility Annotator and this "
         "EntityType is supported by Autofill, it must be here.";
}

// Tests that an empty Accessibility Entity cannot be converted into an Autofill
// AI EntityInstance.
TEST(FromAccessibilityAnnotatorTest, EntityConversion_EmptyEntity) {
  aa::Entity entity;
  entity.entity_id = "test-id";
  entity.specifics.emplace<aa::Passport>();

  EXPECT_EQ(FromAccessibilityAnnotator(entity), std::nullopt);
}

// Tests conversion of a flight reservation from Accessibility Annotator to
// Autofill AI.
TEST(FromAccessibilityAnnotatorTest, EntityConversion_FlightReservation) {
  using enum AttributeTypeName;
  aa::FlightReservation fr;
  fr.flight_number = "QF8415";
  fr.ticket_number = "123";
  fr.confirmation_code = "456";
  fr.passenger_name = "Walter Faber";
  fr.departure_airport = "SYD";
  fr.arrival_airport = "FRA";
  ASSERT_TRUE(
      base::Time::FromLocalExploded(base::Time::Exploded{.year = 2030,
                                                         .month = 7,
                                                         .day_of_month = 31,
                                                         .hour = 12,
                                                         .minute = 7},
                                    &fr.departure_date));
  ASSERT_TRUE(
      base::Time::FromLocalExploded(base::Time::Exploded{.year = 2035,
                                                         .month = 12,
                                                         .day_of_month = 24,
                                                         .hour = 7,
                                                         .minute = 30},
                                    &fr.arrival_date));

  aa::Entity entity;
  entity.entity_id = "test-id";
  entity.specifics = fr;

  EXPECT_THAT(
      FromAccessibilityAnnotator(entity),
      Optional(IsEntity(
          EntityType(EntityTypeName::kFlightReservation),
          EntityInstance::EntityId("test-id"),
          AttributesAre(
              IsAttribute(AttributeType(kFlightReservationFlightNumber),
                          u"QF8415"),
              IsAttribute(AttributeType(kFlightReservationTicketNumber),
                          u"123"),
              IsAttribute(AttributeType(kFlightReservationConfirmationCode),
                          u"456"),
              IsAttribute(AttributeType(kFlightReservationPassengerName),
                          u"Walter Faber"),
              IsAttribute(AttributeType(kFlightReservationDepartureAirport),
                          u"SYD"),
              IsAttribute(AttributeType(kFlightReservationArrivalAirport),
                          u"FRA"),
              IsAttribute(AttributeType(kFlightReservationDepartureDate),
                          u"2030-07-31")))));
}

// Tests conversion of a order from Accessibility Annotator to Autofill AI.
TEST(FromAccessibilityAnnotatorTest, EntityConversion_Order) {
  using enum AttributeTypeName;
  aa::Order o;
  o.id = "order-123";
  o.account = "account-456";
  o.order_date = aa::Date{.day = 31, .month = 7, .year = 2030};
  o.merchant_name = "Tante Emma";
  o.merchant_domain = GURL("https://foo.com/");
  o.products.emplace_back();
  o.products.back().name = "Apple";
  o.products.back().quantity = 5;
  o.products.back().description = "Pink lady";
  o.products.emplace_back();
  o.products.back().name = "Orange";
  o.products.back().quantity = 2;
  o.products.back().description = "Green oranges";
  o.grand_total = "$123,456";

  aa::Entity entity;
  entity.entity_id = "test-id";
  entity.specifics = o;

  EXPECT_THAT(
      FromAccessibilityAnnotator(entity),
      Optional(IsEntity(
          EntityType(EntityTypeName::kOrder),
          EntityInstance::EntityId("test-id"),
          AttributesAre(
              IsAttribute(AttributeType(kOrderId), u"order-123"),
              IsAttribute(AttributeType(kOrderAccount), u"account-456"),
              IsAttribute(AttributeType(kOrderDate), u"2030-07-31"),
              IsAttribute(AttributeType(kOrderMerchantName), u"Tante Emma"),
              IsAttribute(AttributeType(kOrderMerchantDomain),
                          u"https://foo.com/"),
              IsAttribute(AttributeType(kOrderProductNames),
                          u"Apple, Orange")))));
}

// Tests conversion of a shipment from Accessibility Annotator to Autofill AI.
TEST(FromAccessibilityAnnotatorTest, EntityConversion_Shipment) {
  using enum AttributeTypeName;
  aa::Shipment s;
  s.tracking_number = "238947234597";
  s.associated_order_id = "#shonet34234";
  s.delivery_address = "Foostreet 123, 234987 Bar, USA";
  s.delivery_zip_code = "234987";
  s.carrier_name = "Bar";
  s.carrier_domain = GURL("https://bar.com");
  s.estimated_delivery_date = aa::Date{.day = 31, .month = 7, .year = 2030};

  aa::Entity entity;
  entity.entity_id = "test-id";
  entity.specifics = s;

  EXPECT_THAT(
      FromAccessibilityAnnotator(entity),
      Optional(IsEntity(
          EntityType(EntityTypeName::kShipment),
          EntityInstance::EntityId("test-id"),
          AttributesAre(
              IsAttribute(AttributeType(kShipmentTrackingNumber),
                          u"238947234597"),
              IsAttribute(AttributeType(kShipmentOrderIds), u"#shonet34234"),
              IsAttribute(AttributeType(kShipmentCarrierName), u"Bar"),
              IsAttribute(AttributeType(kShipmentCarrierDomain),
                          u"https://bar.com/"),
              IsAttribute(AttributeType(kShipmentEstimatedDeliveryDate),
                          u"2030-07-31"),
              IsAttribute(AttributeType(kShipmentDeliveryZipCode),
                          u"234987")))));
}

// Tests conversion of a drivers license from Accessibility Annotator to
// Autofill AI.
TEST(FromAccessibilityAnnotatorTest, EntityConversion_DriversLicense) {
  using enum AttributeTypeName;
  aa::DriversLicense dl;
  dl.number = "ABC123456";
  dl.name = "John Doe";
  dl.state = "US";
  dl.expiration_date = aa::Date{.day = 31, .month = 7, .year = 2030};
  dl.issue_date = aa::Date{.day = 29, .month = 2, .year = 2000};

  aa::Entity entity;
  entity.entity_id = "test-id";
  entity.specifics = dl;

  EXPECT_THAT(
      FromAccessibilityAnnotator(entity),
      Optional(IsEntity(
          EntityType(EntityTypeName::kDriversLicense),
          EntityInstance::EntityId("test-id"),
          AttributesAre(
              IsAttribute(AttributeType(kDriversLicenseNumber), u"ABC123456"),
              IsAttribute(AttributeType(kDriversLicenseState), u"US"),
              IsAttribute(AttributeType(kDriversLicenseName), u"John Doe"),
              IsAttribute(AttributeType(kDriversLicenseIssueDate),
                          u"2000-02-29"),
              IsAttribute(AttributeType(kDriversLicenseExpirationDate),
                          u"2030-07-31")))));
}

// Tests conversion of a passport from Accessibility Annotator to Autofill AI.
TEST(FromAccessibilityAnnotatorTest, EntityConversion_Passport) {
  using enum AttributeTypeName;
  aa::Passport pp;
  pp.number = "ABC123456";
  pp.name = "John Doe";
  pp.issuing_country = "US";
  pp.expiration_date = aa::Date{.day = 31, .month = 7, .year = 2030};
  pp.issue_date = aa::Date{.day = 29, .month = 2, .year = 2000};

  aa::Entity entity;
  entity.entity_id = "test-id";
  entity.specifics = pp;

  EXPECT_THAT(
      FromAccessibilityAnnotator(entity),
      Optional(IsEntity(
          EntityType(EntityTypeName::kPassport),
          EntityInstance::EntityId("test-id"),
          AttributesAre(
              IsAttribute(AttributeType(kPassportNumber), u"ABC123456"),
              IsAttribute(AttributeType(kPassportCountry), u"US"),
              IsAttribute(AttributeType(kPassportName), u"John Doe"),
              IsAttribute(AttributeType(kPassportIssueDate), u"2000-02-29"),
              IsAttribute(AttributeType(kPassportExpirationDate),
                          u"2030-07-31")))));
}

// Tests conversion of a national ID card from Accessibility Annotator to
// Autofill AI.
TEST(FromAccessibilityAnnotatorTest, EntityConversion_NationalIdCard) {
  using enum AttributeTypeName;
  aa::NationalId ni;
  ni.number = "ABC123456";
  ni.name = "John Doe";
  ni.issuing_country = "US";
  ni.expiration_date = aa::Date{.day = 31, .month = 7, .year = 2030};
  ni.issue_date = aa::Date{.day = 29, .month = 2, .year = 2000};

  aa::Entity entity;
  entity.entity_id = "test-id";
  entity.specifics = ni;

  EXPECT_THAT(
      FromAccessibilityAnnotator(entity),
      Optional(IsEntity(
          EntityType(EntityTypeName::kNationalIdCard),
          EntityInstance::EntityId("test-id"),
          AttributesAre(
              IsAttribute(AttributeType(kNationalIdCardNumber), u"ABC123456"),
              IsAttribute(AttributeType(kNationalIdCardCountry), u"US"),
              IsAttribute(AttributeType(kNationalIdCardName), u"John Doe"),
              IsAttribute(AttributeType(kNationalIdCardIssueDate),
                          u"2000-02-29"),
              IsAttribute(AttributeType(kNationalIdCardExpirationDate),
                          u"2030-07-31")))));
}

// Tests conversion of a vehicle from Accessibility Annotator to Autofill AI.
TEST(FromAccessibilityAnnotatorTest, EntityConversion_Vehicle) {
  using enum AttributeTypeName;
  aa::Vehicle v;
  v.model = "507";
  v.make = "BMW";
  v.year = "1955";
  v.owner = "Abraham Lincoln";
  v.plate_number = "MUC TO123";
  v.plate_state = "Bayern";
  v.vin = "2198572345897";

  aa::Entity entity;
  entity.entity_id = "test-id";
  entity.specifics = v;

  EXPECT_THAT(
      FromAccessibilityAnnotator(entity),
      Optional(IsEntity(
          EntityType(EntityTypeName::kVehicle),
          EntityInstance::EntityId("test-id"),
          AttributesAre(
              IsAttribute(AttributeType(kVehicleModel), u"507"),
              IsAttribute(AttributeType(kVehicleMake), u"BMW"),
              IsAttribute(AttributeType(kVehicleYear), u"1955"),
              IsAttribute(AttributeType(kVehicleOwner), u"Abraham Lincoln"),
              IsAttribute(AttributeType(kVehiclePlateNumber), u"MUC TO123"),
              IsAttribute(AttributeType(kVehiclePlateState), u"Bayern"),
              IsAttribute(AttributeType(kVehicleVin), u"2198572345897")))));
}

}  // namespace
}  // namespace autofill
