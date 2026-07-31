// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"

#include <optional>
#include <variant>

#include "base/notreached.h"
#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {

std::optional<AtMemoryDataType> ToAtMemoryDataType(
    MemoryDataType memory_data_type) {
#define INTENT_TO_FIELD_TYPE(intent, field_type) \
  case MemoryDataType::intent:                   \
    return field_type
#define INTENT_TO_ATTRIBUTE_TYPE(intent_and_attribute_type) \
  case MemoryDataType::intent_and_attribute_type:           \
    return AttributeType(AttributeTypeName::intent_and_attribute_type)
#define INTENT_TO_ATTRIBUTE_TYPE_WITH_NAME(intent, attribute_type) \
  case MemoryDataType::intent:                                     \
    return AttributeType(AttributeTypeName::attribute_type)

  switch (memory_data_type) {
    INTENT_TO_FIELD_TYPE(kNameFull, NAME_FULL);
    INTENT_TO_FIELD_TYPE(kAddressFull, ADDRESS_HOME_ADDRESS);
    INTENT_TO_FIELD_TYPE(kAddressStreetAddress, ADDRESS_HOME_STREET_ADDRESS);
    INTENT_TO_FIELD_TYPE(kAddressCity, ADDRESS_HOME_CITY);
    INTENT_TO_FIELD_TYPE(kAddressState, ADDRESS_HOME_STATE);
    INTENT_TO_FIELD_TYPE(kAddressZip, ADDRESS_HOME_ZIP);
    INTENT_TO_FIELD_TYPE(kAddressCountry, ADDRESS_HOME_COUNTRY);
    INTENT_TO_FIELD_TYPE(kPhone, PHONE_HOME_WHOLE_NUMBER);
    INTENT_TO_FIELD_TYPE(kEmail, EMAIL_ADDRESS);
    INTENT_TO_FIELD_TYPE(kCompanyName, COMPANY_NAME);
    INTENT_TO_FIELD_TYPE(kIban, IBAN_VALUE);
    INTENT_TO_FIELD_TYPE(kCreditCardNumber, CREDIT_CARD_NUMBER);
    INTENT_TO_FIELD_TYPE(kCreditCardExpirationDate,
                         CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
    INTENT_TO_FIELD_TYPE(kCreditCardSecurityCode,
                         CREDIT_CARD_VERIFICATION_CODE);
    INTENT_TO_FIELD_TYPE(kCreditCardNameOnCard, CREDIT_CARD_NAME_FULL);
    INTENT_TO_ATTRIBUTE_TYPE(kVehicleMake);
    INTENT_TO_ATTRIBUTE_TYPE(kVehicleModel);
    INTENT_TO_ATTRIBUTE_TYPE(kVehicleYear);
    INTENT_TO_ATTRIBUTE_TYPE(kVehicleOwner);
    INTENT_TO_ATTRIBUTE_TYPE(kVehiclePlateNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kVehiclePlateState);
    INTENT_TO_ATTRIBUTE_TYPE(kVehicleVin);
    INTENT_TO_ATTRIBUTE_TYPE(kPassportName);
    INTENT_TO_ATTRIBUTE_TYPE(kPassportCountry);
    INTENT_TO_ATTRIBUTE_TYPE(kPassportNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kPassportIssueDate);
    INTENT_TO_ATTRIBUTE_TYPE(kPassportExpirationDate);
    INTENT_TO_ATTRIBUTE_TYPE(kFlightReservationFlightNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kFlightReservationTicketNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kFlightReservationConfirmationCode);
    INTENT_TO_ATTRIBUTE_TYPE(kFlightReservationPassengerName);
    INTENT_TO_ATTRIBUTE_TYPE(kFlightReservationDepartureAirport);
    INTENT_TO_ATTRIBUTE_TYPE(kFlightReservationArrivalAirport);
    INTENT_TO_ATTRIBUTE_TYPE(kFlightReservationDepartureDate);
    INTENT_TO_ATTRIBUTE_TYPE(kNationalIdCardName);
    INTENT_TO_ATTRIBUTE_TYPE(kNationalIdCardCountry);
    INTENT_TO_ATTRIBUTE_TYPE(kNationalIdCardNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kNationalIdCardIssueDate);
    INTENT_TO_ATTRIBUTE_TYPE(kNationalIdCardExpirationDate);
    INTENT_TO_ATTRIBUTE_TYPE(kRedressNumberName);
    INTENT_TO_ATTRIBUTE_TYPE(kRedressNumberNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kKnownTravelerNumberName);
    INTENT_TO_ATTRIBUTE_TYPE(kKnownTravelerNumberNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kKnownTravelerNumberExpirationDate);
    INTENT_TO_ATTRIBUTE_TYPE(kDriversLicenseName);
    INTENT_TO_ATTRIBUTE_TYPE(kDriversLicenseState);
    INTENT_TO_ATTRIBUTE_TYPE(kDriversLicenseNumber);
    INTENT_TO_ATTRIBUTE_TYPE(kDriversLicenseIssueDate);
    INTENT_TO_ATTRIBUTE_TYPE(kDriversLicenseExpirationDate);
    INTENT_TO_ATTRIBUTE_TYPE(kOrderId);
    INTENT_TO_ATTRIBUTE_TYPE(kOrderAccount);
    INTENT_TO_ATTRIBUTE_TYPE(kOrderDate);
    INTENT_TO_ATTRIBUTE_TYPE(kOrderMerchantName);
    INTENT_TO_ATTRIBUTE_TYPE(kOrderMerchantDomain);
    INTENT_TO_ATTRIBUTE_TYPE(kOrderProductNames);
    INTENT_TO_ATTRIBUTE_TYPE(kShipmentTrackingNumber);
    INTENT_TO_ATTRIBUTE_TYPE_WITH_NAME(kShipmentAssociatedOrderId,
                                       kShipmentOrderIds);
    INTENT_TO_ATTRIBUTE_TYPE(kShipmentCarrierName);
    INTENT_TO_ATTRIBUTE_TYPE(kShipmentCarrierDomain);
    INTENT_TO_ATTRIBUTE_TYPE(kShipmentShippedDate);
    INTENT_TO_ATTRIBUTE_TYPE(kShipmentDeliveryZipCode);
    case MemoryDataType::kShipmentEstimatedDeliveryDate:
    case MemoryDataType::kUnknown:
    case MemoryDataType::kIbanNickname:
    case MemoryDataType::kCreditCardNickname:
    case MemoryDataType::kFlightReservationArrivalDate:
    case MemoryDataType::kOrderGrandTotal:
    case MemoryDataType::kShipmentDeliveryAddress:
      return std::nullopt;
  }
  NOTREACHED();

#undef INTENT_TO_ATTRIBUTE_TYPE
#undef INTENT_TO_FIELD_TYPE
#undef INTENT_TO_ATTRIBUTE_TYPE_WITH_NAME
}

std::optional<AutofillClient::AutofillPolicyDataCategory>
ToAutofillPolicyDataCategory(const AtMemoryDataType& type) {
  return std::visit(
      absl::Overload{
          [](FieldType field_type)
              -> std::optional<AutofillClient::AutofillPolicyDataCategory> {
            switch (GroupTypeOfFieldType(field_type)) {
              case FieldTypeGroup::kCreditCard:
              case FieldTypeGroup::kStandaloneCvcField:
              case FieldTypeGroup::kIban:
                return AutofillClient::AutofillPolicyDataCategory::kPayments;
              case FieldTypeGroup::kName:
              case FieldTypeGroup::kEmail:
              case FieldTypeGroup::kCompany:
              case FieldTypeGroup::kAddress:
              case FieldTypeGroup::kPhone:
                return AutofillClient::AutofillPolicyDataCategory::kContactInfo;
              case FieldTypeGroup::kNoGroup:
              case FieldTypeGroup::kPasswordField:
              case FieldTypeGroup::kTransaction:
              case FieldTypeGroup::kUsernameField:
              case FieldTypeGroup::kUnfillable:
              case FieldTypeGroup::kAutofillAi:
              case FieldTypeGroup::kLoyaltyCard:
              case FieldTypeGroup::kOneTimePassword:
                return std::nullopt;
            }
            NOTREACHED();
          },
          [](const AttributeType& attribute_type)
              -> std::optional<AutofillClient::AutofillPolicyDataCategory> {
            switch (attribute_type.entity_type().name()) {
              case EntityTypeName::kNationalIdCard:
              case EntityTypeName::kPassport:
              case EntityTypeName::kDriversLicense:
                return AutofillClient::AutofillPolicyDataCategory::
                    kIdentityDocs;
              case EntityTypeName::kVehicle:
              case EntityTypeName::kFlightReservation:
              case EntityTypeName::kRedressNumber:
              case EntityTypeName::kKnownTravelerNumber:
                return AutofillClient::AutofillPolicyDataCategory::kTravel;
              case EntityTypeName::kOrder:
              case EntityTypeName::kShipment:
                return AutofillClient::AutofillPolicyDataCategory::kShopping;
            }
            NOTREACHED();
          }},
      type);
}

}  // namespace autofill
