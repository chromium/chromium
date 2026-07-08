// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_MEMORY_DATA_TYPE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_MEMORY_DATA_TYPE_H_

namespace accessibility_annotator {

// Represents the type of data a query or piece of information is related to.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(MemoryDataType)
enum class MemoryDataType {
  kUnknown = 0,
  kNameFull = 1,
  kAddressFull = 2,
  kAddressStreetAddress = 3,
  kAddressCity = 4,
  kAddressState = 5,
  kAddressZip = 6,
  kAddressCountry = 7,
  kPhone = 8,
  kEmail = 9,
  kCompanyName = 10,
  kIban = 11,
  kIbanNickname = 12,
  kVehicle = 13,
  kVehicleMake = 14,
  kVehicleModel = 15,
  kVehicleYear = 16,
  kVehicleOwner = 17,
  kVehiclePlateNumber = 18,
  kVehiclePlateState = 19,
  kVehicleVin = 20,
  kPassportFull = 21,
  kPassportName = 22,
  kPassportCountry = 23,
  kPassportNumber = 24,
  kPassportIssueDate = 25,
  kPassportExpirationDate = 26,
  kFlightReservationFull = 27,
  kFlightReservationFlightNumber = 28,
  kFlightReservationTicketNumber = 29,
  kFlightReservationConfirmationCode = 30,
  kFlightReservationPassengerName = 31,
  kFlightReservationDepartureAirport = 32,
  kFlightReservationArrivalAirport = 33,
  kFlightReservationDepartureDate = 34,
  kFlightReservationArrivalDate = 35,
  kShipmentFull = 36,
  kShipmentTrackingNumber = 37,
  kShipmentAssociatedOrderId = 38,
  kShipmentDeliveryAddress = 39,
  kShipmentDeliveryZipCode = 40,
  kShipmentCarrierName = 41,
  kShipmentCarrierDomain = 42,
  kShipmentEstimatedDeliveryDate = 43,
  kShipmentShippedDate = 44,
  kNationalIdCardFull = 45,
  kNationalIdCardName = 46,
  kNationalIdCardCountry = 47,
  kNationalIdCardNumber = 48,
  kNationalIdCardIssueDate = 49,
  kNationalIdCardExpirationDate = 50,
  kRedressNumberFull = 51,
  kRedressNumberName = 52,
  kRedressNumberNumber = 53,
  kKnownTravelerNumberFull = 54,
  kKnownTravelerNumberName = 55,
  kKnownTravelerNumberNumber = 56,
  kKnownTravelerNumberExpirationDate = 57,
  kDriversLicenseFull = 58,
  kDriversLicenseName = 59,
  kDriversLicenseState = 60,
  kDriversLicenseNumber = 61,
  kDriversLicenseIssueDate = 62,
  kDriversLicenseExpirationDate = 63,
  kOrderFull = 64,
  kOrderId = 65,
  kOrderAccount = 66,
  kOrderDate = 67,
  kOrderMerchantName = 68,
  kOrderMerchantDomain = 69,
  kOrderProductNames = 70,
  kOrderGrandTotal = 71,
  kCreditCardNumber = 72,
  kCreditCardExpirationDate = 73,
  kCreditCardSecurityCode = 74,
  kCreditCardNameOnCard = 75,
  kCreditCardNickname = 76,
  kMaxValue = kCreditCardNickname,
};
// LINT.ThenChange(
//     //components/accessibility_annotator/core/annotation_reducer/util.cc:MemoryDataType,
//     //components/accessibility_annotator/core/annotation_reducer/util.cc:AnswerTypeToMemoryDataType,
//     //tools/metrics/histograms/metadata/autofill/enums.xml:MemoryDataType)

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_MEMORY_DATA_TYPE_H_
