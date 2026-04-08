// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"

#include <optional>
#include <variant>

#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/from_accessibility_annotator.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using accessibility_annotator::EntryType;
using testing::Eq;
using testing::IsEmpty;
using testing::Optional;
using testing::VariantWith;

TEST(AtMemoryDataTypeTest, ToAtMemoryDataType) {
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kNameFull),
              Optional(VariantWith<FieldType>(NAME_FULL)));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kAddressFull),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_ADDRESS)));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kAddressStreetAddress),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_STREET_ADDRESS)));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kAddressCity),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_CITY)));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kAddressState),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_STATE)));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kAddressZip),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_ZIP)));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kAddressCountry),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_COUNTRY)));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kPhone),
              Optional(VariantWith<FieldType>(PHONE_HOME_WHOLE_NUMBER)));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kEmail),
              Optional(VariantWith<FieldType>(EMAIL_ADDRESS)));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kCompanyName),
              Optional(VariantWith<FieldType>(COMPANY_NAME)));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kIban),
              Optional(VariantWith<FieldType>(IBAN_VALUE)));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kCreditCardNumber),
              Optional(VariantWith<FieldType>(CREDIT_CARD_NUMBER)));
  EXPECT_THAT(
      ToAtMemoryDataType(EntryType::kCreditCardExpirationDate),
      Optional(VariantWith<FieldType>(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR)));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kCreditCardSecurityCode),
              Optional(VariantWith<FieldType>(CREDIT_CARD_VERIFICATION_CODE)));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kCreditCardNameOnCard),
              Optional(VariantWith<FieldType>(CREDIT_CARD_NAME_FULL)));

  EXPECT_THAT(ToAtMemoryDataType(EntryType::kVehicle),
              Optional(VariantWith<autofill::EntityType>(
                  autofill::EntityType(EntityTypeName::kVehicle))));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kVehicleMake),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kVehicleMake))));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kVehiclePlateState),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kVehiclePlateState))));

  EXPECT_THAT(ToAtMemoryDataType(EntryType::kPassportFull),
              Optional(VariantWith<autofill::EntityType>(
                  autofill::EntityType(EntityTypeName::kPassport))));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kPassportNumber),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kPassportNumber))));

  EXPECT_THAT(ToAtMemoryDataType(EntryType::kFlightReservationFull),
              Optional(VariantWith<autofill::EntityType>(
                  autofill::EntityType(EntityTypeName::kFlightReservation))));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kFlightReservationFlightNumber),
              Optional(VariantWith<AttributeType>(AttributeType(
                  AttributeTypeName::kFlightReservationFlightNumber))));

  EXPECT_THAT(ToAtMemoryDataType(EntryType::kNationalIdCardFull),
              Optional(VariantWith<autofill::EntityType>(
                  autofill::EntityType(EntityTypeName::kNationalIdCard))));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kNationalIdCardNumber),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kNationalIdCardNumber))));

  EXPECT_THAT(ToAtMemoryDataType(EntryType::kRedressNumberFull),
              Optional(VariantWith<autofill::EntityType>(
                  autofill::EntityType(EntityTypeName::kRedressNumber))));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kRedressNumberNumber),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kRedressNumberNumber))));

  EXPECT_THAT(ToAtMemoryDataType(EntryType::kKnownTravelerNumberFull),
              Optional(VariantWith<autofill::EntityType>(
                  autofill::EntityType(EntityTypeName::kKnownTravelerNumber))));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kKnownTravelerNumberNumber),
              Optional(VariantWith<AttributeType>(AttributeType(
                  AttributeTypeName::kKnownTravelerNumberNumber))));

  EXPECT_THAT(ToAtMemoryDataType(EntryType::kDriversLicenseFull),
              Optional(VariantWith<autofill::EntityType>(
                  autofill::EntityType(EntityTypeName::kDriversLicense))));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kDriversLicenseNumber),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kDriversLicenseNumber))));

  EXPECT_THAT(ToAtMemoryDataType(EntryType::kOrderFull),
              Optional(VariantWith<autofill::EntityType>(
                  autofill::EntityType(EntityTypeName::kOrder))));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kOrderId),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kOrderId))));

  EXPECT_THAT(ToAtMemoryDataType(EntryType::kShipmentFull),
              Optional(VariantWith<autofill::EntityType>(
                  autofill::EntityType(EntityTypeName::kShipment))));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kShipmentTrackingNumber),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kShipmentTrackingNumber))));
  EXPECT_THAT(ToAtMemoryDataType(EntryType::kShipmentAssociatedOrderId),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kShipmentOrderIds))));

  EXPECT_THAT(ToAtMemoryDataType(EntryType::kUnknown), Eq(std::nullopt));
}

TEST(AtMemoryDataTypeTest, AttributeTypeToEntryType) {
  EXPECT_THAT(
      AttributeTypeToEntryType(AttributeType(AttributeTypeName::kVehicleMake)),
      Eq(accessibility_annotator::EntryType::kVehicleMake));
  EXPECT_THAT(AttributeTypeToEntryType(
                  AttributeType(AttributeTypeName::kPassportNumber)),
              Eq(accessibility_annotator::EntryType::kPassportNumber));
  EXPECT_THAT(AttributeTypeToEntryType(
                  AttributeType(AttributeTypeName::kShipmentTrackingNumber)),
              Eq(accessibility_annotator::EntryType::kShipmentTrackingNumber));
  EXPECT_THAT(
      AttributeTypeToEntryType(
          AttributeType(AttributeTypeName::kShipmentOrderIds)),
      Eq(accessibility_annotator::EntryType::kShipmentAssociatedOrderId));
}

TEST(AtMemoryDataTypeTest, GetEntryTypeNameForI18n) {
  EXPECT_THAT(GetEntryTypeNameForI18n(EntryType::kNameFull), Eq(u"Name"));
  EXPECT_THAT(GetEntryTypeNameForI18n(EntryType::kPhone), Eq(u"Phone"));
  EXPECT_THAT(GetEntryTypeNameForI18n(EntryType::kVehicle), Eq(u"Vehicle"));
  EXPECT_THAT(GetEntryTypeNameForI18n(EntryType::kVehicleOwner), Eq(u"Owner"));
  EXPECT_THAT(GetEntryTypeNameForI18n(EntryType::kUnknown), IsEmpty());
}

}  // namespace
}  // namespace autofill
