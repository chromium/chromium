// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_STRING_FILTERING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_STRING_FILTERING_UTIL_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"

namespace autofill {

// Returns true if `target_token` fuzzy-matches `query_token` (either as a full
// token match or prefix match) within an allowed Levenshtein edit distance
// based on the length of `query_token`.
[[nodiscard]] bool FuzzyMatchesSingleToken(std::u16string_view target_token,
                                           std::u16string_view query_token);

// Returns true if all `query_tokens` fuzzy-match `target_tokens` in an ordered
// sequence (subsequence matching). Empty `query_tokens` match anything.
[[nodiscard]] bool FuzzyMatchesOrderedTokens(
    base::span<const std::u16string> target_tokens,
    base::span<const std::u16string> query_tokens);

// Tokenizes `normalized_target` and `normalized_filter` into words on every
// call and matches them with `FuzzyMatchesOrderedTokens()`. Expects inputs to
// already be normalized.
[[nodiscard]] bool TokenizeAndFuzzyMatchOrderedTokens(
    std::u16string_view normalized_target,
    std::u16string_view normalized_filter);

// A raw-value query for `MatchesRawValueQuery()`, normalized and tokenized
// once rather than once per compared value.
struct RawValueQuery {
  explicit RawValueQuery(std::u16string_view query);
  RawValueQuery(const RawValueQuery&) = delete;
  RawValueQuery& operator=(const RawValueQuery&) = delete;
  ~RawValueQuery();

  // Returns whether the query can match any value at all: its word tokens
  // need to have at least 2 characters in total.
  bool CanMatch() const;

  // `query` normalized for comparison and stripped of all whitespace.
  std::u16string normalized_without_whitespace;
  // The word tokens of the normalized `query`.
  std::vector<std::u16string> tokens;
};

// How `MatchesRawValueQuery()` matches values of a type.
enum class RawValueMatchMode {
  // Not searchable: obfuscated numbers, dates and amounts.
  kNone,
  // Codes, e.g. phone numbers or IBANs, must contain the query.
  kCode,
  // Free text, e.g. street names, is matched fuzzily.
  kText,
};

// Returns how values of `type` match a raw-value query, see
// `MatchesRawValueQuery()`.
[[nodiscard]] constexpr RawValueMatchMode GetRawValueMatchMode(
    MemoryDataType type) {
  switch (type) {
    case MemoryDataType::kAddressZip:
    case MemoryDataType::kPhone:
    case MemoryDataType::kVehicleYear:
    case MemoryDataType::kVehiclePlateNumber:
    case MemoryDataType::kVehicleVin:
    case MemoryDataType::kIban:
    case MemoryDataType::kCreditCardNumber:
    case MemoryDataType::kLoyaltyMembershipId:
    case MemoryDataType::kFlightReservationFlightNumber:
    case MemoryDataType::kFlightReservationTicketNumber:
    case MemoryDataType::kFlightReservationConfirmationCode:
    case MemoryDataType::kShipmentTrackingNumber:
    case MemoryDataType::kShipmentAssociatedOrderId:
    case MemoryDataType::kShipmentDeliveryZipCode:
    case MemoryDataType::kOrderId:
      return RawValueMatchMode::kCode;
    case MemoryDataType::kNameFull:
    case MemoryDataType::kAddressStreetAddress:
    case MemoryDataType::kAddressCity:
    case MemoryDataType::kAddressState:
    case MemoryDataType::kAddressCountry:
    case MemoryDataType::kEmail:
    case MemoryDataType::kCompanyName:
    case MemoryDataType::kVehicleMake:
    case MemoryDataType::kVehicleModel:
    case MemoryDataType::kVehicleOwner:
    case MemoryDataType::kVehiclePlateState:
    case MemoryDataType::kCreditCardNameOnCard:
    case MemoryDataType::kLoyaltyMembershipProgram:
    case MemoryDataType::kLoyaltyMembershipProvider:
    case MemoryDataType::kPassportName:
    case MemoryDataType::kPassportCountry:
    case MemoryDataType::kFlightReservationPassengerName:
    case MemoryDataType::kFlightReservationDepartureAirport:
    case MemoryDataType::kFlightReservationArrivalAirport:
    case MemoryDataType::kShipmentDeliveryAddress:
    case MemoryDataType::kShipmentCarrierName:
    case MemoryDataType::kShipmentCarrierDomain:
    case MemoryDataType::kNationalIdCardName:
    case MemoryDataType::kNationalIdCardCountry:
    case MemoryDataType::kRedressNumberName:
    case MemoryDataType::kKnownTravelerNumberName:
    case MemoryDataType::kDriversLicenseName:
    case MemoryDataType::kDriversLicenseState:
    case MemoryDataType::kOrderAccount:
    case MemoryDataType::kOrderMerchantName:
    case MemoryDataType::kOrderMerchantDomain:
    case MemoryDataType::kOrderProductNames:
      return RawValueMatchMode::kText;
    case MemoryDataType::kCreditCardSecurityCode:
    case MemoryDataType::kPassportNumber:
    case MemoryDataType::kNationalIdCardNumber:
    case MemoryDataType::kRedressNumberNumber:
    case MemoryDataType::kKnownTravelerNumberNumber:
    case MemoryDataType::kDriversLicenseNumber:
    case MemoryDataType::kCreditCardExpirationDate:
    case MemoryDataType::kPassportIssueDate:
    case MemoryDataType::kPassportExpirationDate:
    case MemoryDataType::kFlightReservationDepartureDate:
    case MemoryDataType::kFlightReservationArrivalDate:
    case MemoryDataType::kShipmentEstimatedDeliveryDate:
    case MemoryDataType::kShipmentShippedDate:
    case MemoryDataType::kNationalIdCardIssueDate:
    case MemoryDataType::kNationalIdCardExpirationDate:
    case MemoryDataType::kKnownTravelerNumberExpirationDate:
    case MemoryDataType::kDriversLicenseIssueDate:
    case MemoryDataType::kDriversLicenseExpirationDate:
    case MemoryDataType::kOrderDate:
    case MemoryDataType::kOrderGrandTotal:
    // TODO(crbug.com/566991902): Support searching by nickname, i.e. match
    // these as `kText`. This requires `AutofillDataProvider` to yield entries
    // of these types first.
    case MemoryDataType::kCreditCardNickname:
    case MemoryDataType::kIbanNickname:
    case MemoryDataType::kAddressFull:
    case MemoryDataType::kUnknown:
      return RawValueMatchMode::kNone;
  }
  // Deprecated values of `MemoryDataType` have no enumerator, but are
  // included in `DenseSet<MemoryDataType>::all()`.
  return RawValueMatchMode::kNone;
}

// Returns true if `normalized_target`, a value of type `type`, matches `query`.
// Numbers and codes (e.g. phone numbers or IBANs) must contain the query, free
// text (e.g. street names) is matched by `FuzzyMatchesOrderedTokens()`.
// Obfuscated numbers, dates and amounts never match, nor do queries for which
// `RawValueQuery::CanMatch()` is false. Expects `normalized_target` to already
// be normalized.
[[nodiscard]] bool MatchesRawValueQuery(MemoryDataType type,
                                        std::u16string_view normalized_target,
                                        const RawValueQuery& query);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_STRING_FILTERING_UTIL_H_
