// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_ENTRY_TYPE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_ENTRY_TYPE_H_

namespace accessibility_annotator {

// Represents the type of data a query or piece of information is related to.
// LINT.IfChange(EntryType)
enum class EntryType {
  kUnknown,
  kNameFull,
  kAddressFull,
  kAddressStreetAddress,
  kAddressCity,
  kAddressState,
  kAddressZip,
  kAddressCountry,
  kPhone,
  kEmail,
  kCompanyName,
  kIban,
  kIbanNickname,
  kVehicle,
  kVehicleMake,
  kVehicleModel,
  kVehicleYear,
  kVehicleOwner,
  kVehiclePlateNumber,
  kVehiclePlateState,
  kVehicleVin,
  kPassportFull,
  kPassportName,
  kPassportCountry,
  kPassportNumber,
  kPassportIssueDate,
  kPassportExpirationDate,
  kFlightReservationFull,
  kFlightReservationFlightNumber,
  kFlightReservationTicketNumber,
  kFlightReservationConfirmationCode,
  kFlightReservationPassengerName,
  kFlightReservationDepartureAirport,
  kFlightReservationArrivalAirport,
  kFlightReservationDepartureDate,
  kFlightReservationArrivalDate,
  kShipmentFull,
  kShipmentTrackingNumber,
  kShipmentAssociatedOrderId,
  kShipmentDeliveryAddress,
  kShipmentDeliveryZipCode,
  kShipmentCarrierName,
  kShipmentCarrierDomain,
  kShipmentEstimatedDeliveryDate,
  kNationalIdCardFull,
  kNationalIdCardName,
  kNationalIdCardCountry,
  kNationalIdCardNumber,
  kNationalIdCardIssueDate,
  kNationalIdCardExpirationDate,
  kRedressNumberFull,
  kRedressNumberName,
  kRedressNumberNumber,
  kKnownTravelerNumberFull,
  kKnownTravelerNumberName,
  kKnownTravelerNumberNumber,
  kKnownTravelerNumberExpirationDate,
  kDriversLicenseFull,
  kDriversLicenseName,
  kDriversLicenseState,
  kDriversLicenseNumber,
  kDriversLicenseIssueDate,
  kDriversLicenseExpirationDate,
  kOrderFull,
  kOrderId,
  kOrderAccount,
  kOrderDate,
  kOrderMerchantName,
  kOrderMerchantDomain,
  kOrderProductNames,
  kOrderGrandTotal,
  kCreditCardNumber,
  kCreditCardExpirationDate,
  kCreditCardSecurityCode,
  kCreditCardNameOnCard,
  kCreditCardNickname,
};
// LINT.ThenChange(
//     //components/accessibility_annotator/core/annotation_reducer/util.cc:EntryType,
//     //components/accessibility_annotator/core/annotation_reducer/util.cc:AnswerTypeToEntryType)

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_ENTRY_TYPE_H_
