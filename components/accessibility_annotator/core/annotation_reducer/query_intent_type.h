// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_QUERY_INTENT_TYPE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_QUERY_INTENT_TYPE_H_

namespace accessibility_annotator {

// Represents the type of data a query or piece of information is related to.
enum class QueryIntentType {
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
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_QUERY_INTENT_TYPE_H_
