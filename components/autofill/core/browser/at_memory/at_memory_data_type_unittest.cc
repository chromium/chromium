// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"

#include <optional>
#include <variant>

#include "components/accessibility_annotator/annotation_reducer/query_intent_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using annotation_reducer::QueryIntentType;
using testing::Eq;
using testing::Optional;
using testing::VariantWith;

TEST(AtMemoryDataTypeTest, ToAtMemoryDataType) {
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kNameFull),
              Optional(VariantWith<FieldType>(NAME_FULL)));
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kAddressFull),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_ADDRESS)));
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kAddressStreetAddress),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_STREET_ADDRESS)));
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kAddressCity),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_CITY)));
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kAddressState),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_STATE)));
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kAddressZip),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_ZIP)));
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kAddressCountry),
              Optional(VariantWith<FieldType>(ADDRESS_HOME_COUNTRY)));
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kPhone),
              Optional(VariantWith<FieldType>(PHONE_HOME_WHOLE_NUMBER)));
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kEmail),
              Optional(VariantWith<FieldType>(EMAIL_ADDRESS)));
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kIban),
              Optional(VariantWith<FieldType>(IBAN_VALUE)));

  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kVehicle),
              Optional(VariantWith<autofill::EntityType>(
                  autofill::EntityType(EntityTypeName::kVehicle))));
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kVehiclePlateNumber),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kVehiclePlateNumber))));
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kVehicleVin),
              Optional(VariantWith<AttributeType>(
                  AttributeType(AttributeTypeName::kVehicleVin))));

  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kPassportFull),
              Optional(VariantWith<autofill::EntityType>(
                  autofill::EntityType(EntityTypeName::kPassport))));
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kFlightReservationFull),
              Optional(VariantWith<autofill::EntityType>(
                  autofill::EntityType(EntityTypeName::kFlightReservation))));
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kNationalIdCardFull),
              Optional(VariantWith<autofill::EntityType>(
                  autofill::EntityType(EntityTypeName::kNationalIdCard))));
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kRedressNumberFull),
              Optional(VariantWith<autofill::EntityType>(
                  autofill::EntityType(EntityTypeName::kRedressNumber))));
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kKnownTravelerNumberFull),
              Optional(VariantWith<autofill::EntityType>(
                  autofill::EntityType(EntityTypeName::kKnownTravelerNumber))));
  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kDriversLicenseFull),
              Optional(VariantWith<autofill::EntityType>(
                  autofill::EntityType(EntityTypeName::kDriversLicense))));

  EXPECT_THAT(ToAtMemoryDataType(QueryIntentType::kUnknown), Eq(std::nullopt));
}

}  // namespace
}  // namespace autofill
