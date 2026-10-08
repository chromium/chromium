// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/at_memory/at_memory_string_filtering_util.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "base/i18n/break_iterator.h"
#include "base/strings/levenshtein_distance.h"
#include "base/strings/string_util.h"
#include "base/strings/whitespace_constants.h"

namespace autofill {

namespace {

// Minimum number of characters, excluding whitespace, that a query needs to
// match.
constexpr size_t kMinQueryLength = 2;

std::vector<std::u16string> TokenizeString(std::u16string_view text) {
  std::vector<std::u16string> tokens;
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init()) {
    return tokens;
  }
  while (iter.Advance()) {
    if (iter.IsWord()) {
      tokens.emplace_back(iter.GetString());
    }
  }
  return tokens;
}

std::u16string RemoveWhitespace(std::u16string_view text) {
  std::u16string result;
  base::RemoveChars(text, base::kWhitespaceUTF16, &result);
  return result;
}

}  // namespace

bool FuzzyMatchesSingleToken(std::u16string_view target_token,
                             std::u16string_view query_token) {
  if (query_token.empty()) {
    return true;
  }

  // Determine maximum allowed Levenshtein edit distance based on query length:
  // - Short tokens (len <= 2): 0 edits allowed (exact or exact prefix match).
  // - Medium tokens (3 <= len <= 5): max 1 edit allowed.
  // - Long tokens (len > 5): max 2 edits allowed.
  const size_t query_len = query_token.length();
  const size_t max_distance = (query_len <= 2) ? 0 : (query_len <= 5 ? 1 : 2);

  // Check full token edit distance.
  if (base::LevenshteinDistance(query_token, target_token, max_distance) <=
      max_distance) {
    return true;
  }

  // Check prefix token edit distance (for incomplete query words during
  // typing).
  if (target_token.length() >= query_len) {
    const std::u16string_view target_prefix = target_token.substr(0, query_len);
    if (base::LevenshteinDistance(query_token, target_prefix, max_distance) <=
        max_distance) {
      return true;
    }
  }

  return false;
}

bool FuzzyMatchesOrderedTokens(std::u16string_view normalized_target,
                               std::u16string_view normalized_filter) {
  if (normalized_filter.empty()) {
    return true;
  }

  const std::vector<std::u16string> query_tokens =
      TokenizeString(normalized_filter);
  if (query_tokens.empty()) {
    return true;
  }

  const std::vector<std::u16string> target_tokens =
      TokenizeString(normalized_target);
  if (query_tokens.size() > target_tokens.size()) {
    return false;
  }

  size_t target_idx = 0;
  for (const std::u16string& query_token : query_tokens) {
    bool matched = false;
    while (target_idx < target_tokens.size()) {
      if (FuzzyMatchesSingleToken(target_tokens[target_idx], query_token)) {
        target_idx++;
        matched = true;
        break;
      }
      target_idx++;
    }
    if (!matched) {
      return false;
    }
  }

  return true;
}

bool MatchesRawValueQuery(MemoryDataType type,
                          std::u16string_view normalized_target,
                          std::u16string_view normalized_query) {
  if (normalized_query.size() < kMinQueryLength) {
    return false;
  }
  switch (type) {
    // Codes must contain the query.
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
      return RemoveWhitespace(normalized_target)
          .contains(RemoveWhitespace(normalized_query));
    // Free text is matched fuzzily.
    case MemoryDataType::kNameFull:
    case MemoryDataType::kAddressFull:
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
    case MemoryDataType::kIbanNickname:
    case MemoryDataType::kCreditCardNameOnCard:
    case MemoryDataType::kCreditCardNickname:
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
      return FuzzyMatchesOrderedTokens(normalized_target, normalized_query);
    // Sensitive numbers are obfuscated, so they are never searched.
    case MemoryDataType::kCreditCardSecurityCode:
    case MemoryDataType::kPassportNumber:
    case MemoryDataType::kNationalIdCardNumber:
    case MemoryDataType::kRedressNumberNumber:
    case MemoryDataType::kKnownTravelerNumberNumber:
    case MemoryDataType::kDriversLicenseNumber:
    // Dates and amounts have no raw-value representation to search.
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
    case MemoryDataType::kUnknown:
      return false;
  }
}

}  // namespace autofill
