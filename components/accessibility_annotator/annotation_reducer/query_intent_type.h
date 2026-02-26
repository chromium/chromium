// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_ANNOTATION_REDUCER_QUERY_INTENT_TYPE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_ANNOTATION_REDUCER_QUERY_INTENT_TYPE_H_

namespace annotation_reducer {

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
  kIban,
  kVehicle,
  kVehiclePlateNumber,
  kVehicleVin,
  kPassportFull,
  kFlightReservationFull,
  kNationalIdCardFull,
  kRedressNumberFull,
  kKnownTravelerNumberFull,
  kDriversLicenseFull,
};

}  // namespace annotation_reducer

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_ANNOTATION_REDUCER_QUERY_INTENT_TYPE_H_
