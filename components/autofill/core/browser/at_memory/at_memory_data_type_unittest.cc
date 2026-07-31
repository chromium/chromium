// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"

#include <optional>
#include <variant>

#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using testing::Eq;
using testing::IsEmpty;
using testing::Optional;
using testing::VariantWith;

TEST(AtMemoryDataTypeTest, ToAtMemoryDataType) {
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kNameFull),
              Optional(VariantWith<FieldType>(NAME_FULL)));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kAddressFull),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_ADDRESS)));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kAddressStreetAddress),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_STREET_ADDRESS)));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kAddressCity),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_CITY)));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kAddressState),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_STATE)));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kAddressZip),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_ZIP)));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kAddressCountry),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_COUNTRY)));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kPhone),
              Optional(VariantWith<FieldType>(PHONE_HOME_WHOLE_NUMBER)));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kEmail),
              Optional(VariantWith<FieldType>(EMAIL_ADDRESS)));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kCompanyName),
              Optional(VariantWith<FieldType>(COMPANY_NAME)));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kIban),
              Optional(VariantWith<FieldType>(IBAN_VALUE)));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kCreditCardNumber),
              Optional(VariantWith<FieldType>(CREDIT_CARD_NUMBER)));
  EXPECT_THAT(
      ToAtMemoryDataType(MemoryDataType::kCreditCardExpirationDate),
      Optional(VariantWith<FieldType>(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR)));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kCreditCardSecurityCode),
              Optional(VariantWith<FieldType>(CREDIT_CARD_VERIFICATION_CODE)));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kCreditCardNameOnCard),
              Optional(VariantWith<FieldType>(CREDIT_CARD_NAME_FULL)));

  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kVehicleMake),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kVehicleMake))));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kVehiclePlateState),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kVehiclePlateState))));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kPassportNumber),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kPassportNumber))));
  EXPECT_THAT(
      ToAtMemoryDataType(MemoryDataType::kFlightReservationFlightNumber),
      Optional(VariantWith<AttributeType>(
          AttributeType(AttributeTypeName::kFlightReservationFlightNumber))));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kNationalIdCardNumber),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kNationalIdCardNumber))));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kRedressNumberNumber),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kRedressNumberNumber))));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kKnownTravelerNumberNumber),
              Optional(VariantWith<AttributeType>(AttributeType(
                  AttributeTypeName::kKnownTravelerNumberNumber))));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kDriversLicenseNumber),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kDriversLicenseNumber))));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kOrderId),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kOrderId))));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kShipmentTrackingNumber),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kShipmentTrackingNumber))));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kShipmentAssociatedOrderId),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kShipmentOrderIds))));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kShipmentShippedDate),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kShipmentShippedDate))));
  EXPECT_THAT(
      ToAtMemoryDataType(MemoryDataType::kShipmentEstimatedDeliveryDate),
      Eq(std::nullopt));
  EXPECT_THAT(ToAtMemoryDataType(MemoryDataType::kUnknown), Eq(std::nullopt));
}

TEST(AtMemoryDataTypeTest, ToAutofillPolicyDataCategory) {
  using Category = AutofillClient::AutofillPolicyDataCategory;

  // FieldTypes
  EXPECT_THAT(ToAutofillPolicyDataCategory(AtMemoryDataType(NAME_FULL)),
              Optional(Category::kContactInfo));
  EXPECT_THAT(
      ToAutofillPolicyDataCategory(AtMemoryDataType(CREDIT_CARD_NUMBER)),
      Optional(Category::kPayments));
  EXPECT_THAT(ToAutofillPolicyDataCategory(AtMemoryDataType(IBAN_VALUE)),
              Optional(Category::kPayments));
  EXPECT_THAT(ToAutofillPolicyDataCategory(AtMemoryDataType(UNKNOWN_TYPE)),
              Eq(std::nullopt));


  // AttributeTypes
  EXPECT_THAT(ToAutofillPolicyDataCategory(AtMemoryDataType(
                  AttributeType(AttributeTypeName::kPassportNumber))),
              Optional(Category::kIdentityDocs));
  EXPECT_THAT(ToAutofillPolicyDataCategory(AtMemoryDataType(
                  AttributeType(AttributeTypeName::kVehicleMake))),
              Optional(Category::kTravel));
  EXPECT_THAT(ToAutofillPolicyDataCategory(
                  AtMemoryDataType(AttributeType(AttributeTypeName::kOrderId))),
              Optional(Category::kShopping));
}

TEST(AtMemoryDataTypeTest, AttributeTypeToMemoryDataType) {
  EXPECT_THAT(AttributeTypeToMemoryDataType(
                  AttributeType(AttributeTypeName::kVehicleMake)),
              Eq(MemoryDataType::kVehicleMake));
  EXPECT_THAT(AttributeTypeToMemoryDataType(
                  AttributeType(AttributeTypeName::kPassportNumber)),
              Eq(MemoryDataType::kPassportNumber));
  EXPECT_THAT(AttributeTypeToMemoryDataType(
                  AttributeType(AttributeTypeName::kShipmentTrackingNumber)),
              Eq(MemoryDataType::kShipmentTrackingNumber));
  EXPECT_THAT(AttributeTypeToMemoryDataType(
                  AttributeType(AttributeTypeName::kShipmentOrderIds)),
              Eq(MemoryDataType::kShipmentAssociatedOrderId));
  EXPECT_THAT(AttributeTypeToMemoryDataType(
                  AttributeType(AttributeTypeName::kShipmentShippedDate)),
              Eq(MemoryDataType::kShipmentShippedDate));
}

TEST(AtMemoryDataTypeTest, GetMemoryDataTypeNameForI18n) {
  EXPECT_THAT(GetMemoryDataTypeNameForI18n(MemoryDataType::kNameFull),
              Eq(u"Name"));
  EXPECT_THAT(GetMemoryDataTypeNameForI18n(MemoryDataType::kPhone),
              Eq(u"Phone"));
  EXPECT_THAT(GetMemoryDataTypeNameForI18n(MemoryDataType::kVehicleOwner),
              Eq(u"Owner"));
  EXPECT_THAT(GetMemoryDataTypeNameForI18n(MemoryDataType::kUnknown),
              IsEmpty());
}

}  // namespace
}  // namespace autofill
