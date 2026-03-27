// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/util.h"

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_util.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"

namespace accessibility_annotator {

std::string_view StripMarkdownCodeBlocks(std::string_view text) {
  if (base::StartsWith(text, "```json", base::CompareCase::SENSITIVE)) {
    text.remove_prefix(7);
  } else if (base::StartsWith(text, "```", base::CompareCase::SENSITIVE)) {
    text.remove_prefix(3);
  }

  if (base::EndsWith(text, "```", base::CompareCase::SENSITIVE)) {
    text.remove_suffix(3);
  }

  return base::TrimWhitespaceASCII(text, base::TRIM_ALL);
}

// TODO(crbug.com/496281633): Avoid using a custom string to enum conversion.
QueryIntentType StringToQueryIntentType(std::string_view intent_str) {
  // LINT.IfChange(QueryIntentType)
  static constexpr auto kIntentMap = base::MakeFixedFlatMap<std::string_view,
                                                            QueryIntentType>({
      {"kAddressCity", QueryIntentType::kAddressCity},
      {"kAddressCountry", QueryIntentType::kAddressCountry},
      {"kAddressFull", QueryIntentType::kAddressFull},
      {"kAddressState", QueryIntentType::kAddressState},
      {"kAddressStreetAddress", QueryIntentType::kAddressStreetAddress},
      {"kAddressZip", QueryIntentType::kAddressZip},
      {"kCompanyName", QueryIntentType::kCompanyName},
      {"kDriversLicenseExpirationDate",
       QueryIntentType::kDriversLicenseExpirationDate},
      {"kDriversLicenseFull", QueryIntentType::kDriversLicenseFull},
      {"kDriversLicenseIssueDate", QueryIntentType::kDriversLicenseIssueDate},
      {"kDriversLicenseName", QueryIntentType::kDriversLicenseName},
      {"kDriversLicenseNumber", QueryIntentType::kDriversLicenseNumber},
      {"kDriversLicenseState", QueryIntentType::kDriversLicenseState},
      {"kEmail", QueryIntentType::kEmail},
      {"kFlightReservationArrivalAirport",
       QueryIntentType::kFlightReservationArrivalAirport},
      {"kFlightReservationConfirmationCode",
       QueryIntentType::kFlightReservationConfirmationCode},
      {"kFlightReservationDepartureAirport",
       QueryIntentType::kFlightReservationDepartureAirport},
      {"kFlightReservationDepartureDate",
       QueryIntentType::kFlightReservationDepartureDate},
      {"kFlightReservationFlightNumber",
       QueryIntentType::kFlightReservationFlightNumber},
      {"kFlightReservationFull", QueryIntentType::kFlightReservationFull},
      {"kFlightReservationPassengerName",
       QueryIntentType::kFlightReservationPassengerName},
      {"kFlightReservationTicketNumber",
       QueryIntentType::kFlightReservationTicketNumber},
      {"kIban", QueryIntentType::kIban},
      {"kIbanNickname", QueryIntentType::kIbanNickname},
      {"kKnownTravelerNumberExpirationDate",
       QueryIntentType::kKnownTravelerNumberExpirationDate},
      {"kKnownTravelerNumberFull", QueryIntentType::kKnownTravelerNumberFull},
      {"kKnownTravelerNumberName", QueryIntentType::kKnownTravelerNumberName},
      {"kKnownTravelerNumberNumber",
       QueryIntentType::kKnownTravelerNumberNumber},
      {"kNameFull", QueryIntentType::kNameFull},
      {"kNationalIdCardCountry", QueryIntentType::kNationalIdCardCountry},
      {"kNationalIdCardExpirationDate",
       QueryIntentType::kNationalIdCardExpirationDate},
      {"kNationalIdCardFull", QueryIntentType::kNationalIdCardFull},
      {"kNationalIdCardIssueDate", QueryIntentType::kNationalIdCardIssueDate},
      {"kNationalIdCardName", QueryIntentType::kNationalIdCardName},
      {"kNationalIdCardNumber", QueryIntentType::kNationalIdCardNumber},
      {"kOrderAccount", QueryIntentType::kOrderAccount},
      {"kOrderDate", QueryIntentType::kOrderDate},
      {"kOrderFull", QueryIntentType::kOrderFull},
      {"kOrderGrandTotal", QueryIntentType::kOrderGrandTotal},
      {"kOrderId", QueryIntentType::kOrderId},
      {"kOrderMerchantDomain", QueryIntentType::kOrderMerchantDomain},
      {"kOrderMerchantName", QueryIntentType::kOrderMerchantName},
      {"kOrderProductNames", QueryIntentType::kOrderProductNames},
      {"kPassportCountry", QueryIntentType::kPassportCountry},
      {"kPassportExpirationDate", QueryIntentType::kPassportExpirationDate},
      {"kPassportFull", QueryIntentType::kPassportFull},
      {"kPassportIssueDate", QueryIntentType::kPassportIssueDate},
      {"kPassportName", QueryIntentType::kPassportName},
      {"kPassportNumber", QueryIntentType::kPassportNumber},
      {"kPhone", QueryIntentType::kPhone},
      {"kRedressNumberFull", QueryIntentType::kRedressNumberFull},
      {"kRedressNumberName", QueryIntentType::kRedressNumberName},
      {"kRedressNumberNumber", QueryIntentType::kRedressNumberNumber},
      {"kVehicle", QueryIntentType::kVehicle},
      {"kVehicleMake", QueryIntentType::kVehicleMake},
      {"kVehicleModel", QueryIntentType::kVehicleModel},
      {"kVehicleOwner", QueryIntentType::kVehicleOwner},
      {"kVehiclePlateNumber", QueryIntentType::kVehiclePlateNumber},
      {"kVehiclePlateState", QueryIntentType::kVehiclePlateState},
      {"kVehicleVin", QueryIntentType::kVehicleVin},
      {"kVehicleYear", QueryIntentType::kVehicleYear},
  });
  // LINT.ThenChange(//components/accessibility_annotator/core/annotation_reducer/query_intent_type.h:QueryIntentType)

  auto found_intent_it = kIntentMap.find(intent_str);
  return found_intent_it != kIntentMap.end() ? found_intent_it->second
                                             : QueryIntentType::kUnknown;
}

}  // namespace accessibility_annotator
