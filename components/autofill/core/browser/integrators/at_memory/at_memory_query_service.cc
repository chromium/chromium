// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/at_memory/at_memory_query_service.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/extend.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type_util.h"
#include "components/autofill/core/browser/at_memory/autofill_data_provider.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/personal_context/proto/features/at_memory.pb.h"
#include "components/personal_context/proto/features/common_data.pb.h"
#include "net/base/network_change_notifier.h"

namespace autofill {

namespace {

using ::accessibility_annotator::EntryMetadata;
using ::accessibility_annotator::MemoryDataType;
using ::accessibility_annotator::MemoryEntrySource;
using ::accessibility_annotator::MemoryEntrySourceType;
using ::accessibility_annotator::MemorySearchResult;
using ::accessibility_annotator::MemorySearchResults;
using ::accessibility_annotator::MemorySearchStatus;
using ::personal_context::proto::AtMemoryQueryResponse;

MemoryDataType ToMemoryDataType(
    personal_context::proto::MemoryDataType data_type) {
  switch (data_type) {
    case personal_context::proto::MEMORY_DATA_TYPE_UNSPECIFIED:
      return MemoryDataType::kUnknown;
    case personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL:
      return MemoryDataType::kNameFull;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL:
      return MemoryDataType::kAddressFull;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_STREET_ADDRESS:
      return MemoryDataType::kAddressStreetAddress;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_CITY:
      return MemoryDataType::kAddressCity;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_STATE:
      return MemoryDataType::kAddressState;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_ZIP:
      return MemoryDataType::kAddressZip;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_COUNTRY:
      return MemoryDataType::kAddressCountry;
    case personal_context::proto::MEMORY_DATA_TYPE_PHONE:
      return MemoryDataType::kPhone;
    case personal_context::proto::MEMORY_DATA_TYPE_EMAIL:
      return MemoryDataType::kEmail;
    case personal_context::proto::MEMORY_DATA_TYPE_COMPANY_NAME:
      return MemoryDataType::kCompanyName;
    case personal_context::proto::MEMORY_DATA_TYPE_IBAN:
      return MemoryDataType::kIban;
    case personal_context::proto::MEMORY_DATA_TYPE_IBAN_NICKNAME:
      return MemoryDataType::kIbanNickname;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE:
      return MemoryDataType::kVehicle;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_MAKE:
      return MemoryDataType::kVehicleMake;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_MODEL:
      return MemoryDataType::kVehicleModel;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_YEAR:
      return MemoryDataType::kVehicleYear;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_OWNER:
      return MemoryDataType::kVehicleOwner;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_PLATE_NUMBER:
      return MemoryDataType::kVehiclePlateNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_PLATE_STATE:
      return MemoryDataType::kVehiclePlateState;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_VIN:
      return MemoryDataType::kVehicleVin;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_FULL:
      return MemoryDataType::kPassportFull;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NAME:
      return MemoryDataType::kPassportName;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_COUNTRY:
      return MemoryDataType::kPassportCountry;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NUMBER:
      return MemoryDataType::kPassportNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_ISSUE_DATE:
      return MemoryDataType::kPassportIssueDate;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_EXPIRATION_DATE:
      return MemoryDataType::kPassportExpirationDate;
    case personal_context::proto::MEMORY_DATA_TYPE_FLIGHT_RESERVATION_FULL:
      return MemoryDataType::kFlightReservationFull;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_FLIGHT_NUMBER:
      return MemoryDataType::kFlightReservationFlightNumber;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_TICKET_NUMBER:
      return MemoryDataType::kFlightReservationTicketNumber;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_CONFIRMATION_CODE:
      return MemoryDataType::kFlightReservationConfirmationCode;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_PASSENGER_NAME:
      return MemoryDataType::kFlightReservationPassengerName;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_DEPARTURE_AIRPORT:
      return MemoryDataType::kFlightReservationDepartureAirport;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_ARRIVAL_AIRPORT:
      return MemoryDataType::kFlightReservationArrivalAirport;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_DEPARTURE_DATE:
      return MemoryDataType::kFlightReservationDepartureDate;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_ARRIVAL_DATE:
      return MemoryDataType::kFlightReservationArrivalDate;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_FULL:
      return MemoryDataType::kShipmentFull;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_TRACKING_NUMBER:
      return MemoryDataType::kShipmentTrackingNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_ASSOCIATED_ORDER_ID:
      return MemoryDataType::kShipmentAssociatedOrderId;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_DELIVERY_ADDRESS:
      return MemoryDataType::kShipmentDeliveryAddress;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_DELIVERY_ZIP_CODE:
      return MemoryDataType::kShipmentDeliveryZipCode;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_CARRIER_NAME:
      return MemoryDataType::kShipmentCarrierName;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_CARRIER_DOMAIN:
      return MemoryDataType::kShipmentCarrierDomain;
    case personal_context::proto::
        MEMORY_DATA_TYPE_SHIPMENT_ESTIMATED_DELIVERY_DATE:
      return MemoryDataType::kShipmentEstimatedDeliveryDate;
    case personal_context::proto::MEMORY_DATA_TYPE_NATIONAL_ID_CARD_FULL:
      return MemoryDataType::kNationalIdCardFull;
    case personal_context::proto::MEMORY_DATA_TYPE_NATIONAL_ID_CARD_NAME:
      return MemoryDataType::kNationalIdCardName;
    case personal_context::proto::MEMORY_DATA_TYPE_NATIONAL_ID_CARD_COUNTRY:
      return MemoryDataType::kNationalIdCardCountry;
    case personal_context::proto::MEMORY_DATA_TYPE_NATIONAL_ID_CARD_NUMBER:
      return MemoryDataType::kNationalIdCardNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_NATIONAL_ID_CARD_ISSUE_DATE:
      return MemoryDataType::kNationalIdCardIssueDate;
    case personal_context::proto::
        MEMORY_DATA_TYPE_NATIONAL_ID_CARD_EXPIRATION_DATE:
      return MemoryDataType::kNationalIdCardExpirationDate;
    case personal_context::proto::MEMORY_DATA_TYPE_REDRESS_NUMBER_FULL:
      return MemoryDataType::kRedressNumberFull;
    case personal_context::proto::MEMORY_DATA_TYPE_REDRESS_NUMBER_NAME:
      return MemoryDataType::kRedressNumberName;
    case personal_context::proto::MEMORY_DATA_TYPE_REDRESS_NUMBER_NUMBER:
      return MemoryDataType::kRedressNumberNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_KNOWN_TRAVELER_NUMBER_FULL:
      return MemoryDataType::kKnownTravelerNumberFull;
    case personal_context::proto::MEMORY_DATA_TYPE_KNOWN_TRAVELER_NUMBER_NAME:
      return MemoryDataType::kKnownTravelerNumberName;
    case personal_context::proto::MEMORY_DATA_TYPE_KNOWN_TRAVELER_NUMBER_NUMBER:
      return MemoryDataType::kKnownTravelerNumberNumber;
    case personal_context::proto::
        MEMORY_DATA_TYPE_KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE:
      return MemoryDataType::kKnownTravelerNumberExpirationDate;
    case personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_FULL:
      return MemoryDataType::kDriversLicenseFull;
    case personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_NAME:
      return MemoryDataType::kDriversLicenseName;
    case personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_STATE:
      return MemoryDataType::kDriversLicenseState;
    case personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_NUMBER:
      return MemoryDataType::kDriversLicenseNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_ISSUE_DATE:
      return MemoryDataType::kDriversLicenseIssueDate;
    case personal_context::proto::
        MEMORY_DATA_TYPE_DRIVERS_LICENSE_EXPIRATION_DATE:
      return MemoryDataType::kDriversLicenseExpirationDate;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_FULL:
      return MemoryDataType::kOrderFull;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_ID:
      return MemoryDataType::kOrderId;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_ACCOUNT:
      return MemoryDataType::kOrderAccount;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_DATE:
      return MemoryDataType::kOrderDate;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_MERCHANT_NAME:
      return MemoryDataType::kOrderMerchantName;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_MERCHANT_DOMAIN:
      return MemoryDataType::kOrderMerchantDomain;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_PRODUCT_NAMES:
      return MemoryDataType::kOrderProductNames;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_GRAND_TOTAL:
      return MemoryDataType::kOrderGrandTotal;
    case personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_NUMBER:
      return MemoryDataType::kCreditCardNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_EXPIRATION_DATE:
      return MemoryDataType::kCreditCardExpirationDate;
    case personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_SECURITY_CODE:
      return MemoryDataType::kCreditCardSecurityCode;
    case personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_NAME_ON_CARD:
      return MemoryDataType::kCreditCardNameOnCard;
    case personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_NICKNAME:
      return MemoryDataType::kCreditCardNickname;
  }
}

std::vector<MemoryEntrySource> ExtractSources(
    const personal_context::proto::AtMemorySearchResult& proto_result) {
  std::vector<MemoryEntrySource> sources;
  for (const personal_context::proto::SourceReference& proto_source :
       proto_result.sources()) {
    if (proto_source.has_gmail()) {
      std::string_view message_url = proto_source.gmail().message_url();
      sources.emplace_back(MemoryEntrySourceType::kGmail,
                           message_url.empty()
                               ? std::nullopt
                               : std::make_optional<std::string>(message_url));
    } else if (proto_source.has_photos()) {
      std::string_view photos_url = proto_source.photos().photos_url();
      sources.emplace_back(MemoryEntrySourceType::kPhotos,
                           photos_url.empty()
                               ? std::nullopt
                               : std::make_optional<std::string>(photos_url));
    }
  }
  return sources;
}

std::vector<EntryMetadata> ExtractMetadata(
    const personal_context::proto::AtMemorySearchResult& proto_result) {
  std::vector<EntryMetadata> metadata_list;
  for (const personal_context::proto::Attribute& secondary :
       proto_result.secondary_attributes()) {
    MemoryDataType other_type = MemoryDataType::kUnknown;
    std::u16string other_type_name;
    if (secondary.has_schemaful_key()) {
      other_type = ToMemoryDataType(secondary.schemaful_key());
    } else if (secondary.has_schemaless_key()) {
      other_type_name = base::UTF8ToUTF16(secondary.schemaless_key());
    }
    metadata_list.emplace_back(other_type, other_type_name,
                               base::UTF8ToUTF16(secondary.value()));
  }
  return metadata_list;
}

MemorySearchResult ConvertToMemorySearchResult(
    const personal_context::proto::AtMemorySearchResult& proto_result) {
  MemoryDataType memory_data_type = MemoryDataType::kUnknown;
  std::u16string type_name;
  std::u16string primary_value;
  if (proto_result.has_primary_attribute()) {
    const personal_context::proto::Attribute& primary =
        proto_result.primary_attribute();
    if (primary.has_schemaful_key()) {
      memory_data_type = ToMemoryDataType(primary.schemaful_key());
    } else if (primary.has_schemaless_key()) {
      type_name = base::UTF8ToUTF16(primary.schemaless_key());
    }
    primary_value = base::UTF8ToUTF16(primary.value());
  }

  MemorySearchResult pcontext_result(memory_data_type, type_name, primary_value,
                                     proto_result.relevance_score());
  pcontext_result.sources = ExtractSources(proto_result);
  pcontext_result.metadata_list = ExtractMetadata(proto_result);
  pcontext_result.is_obfuscated = IsSpiiMemoryDataType(memory_data_type);
  return pcontext_result;
}

std::vector<MemorySearchResult> ExtractRemoteResults(
    const personal_context::proto::AtMemoryQueryResponse& response) {
  std::vector<MemorySearchResult> remote_results;
  for (int32_t i = 0; i < response.results_size(); ++i) {
    const personal_context::proto::AtMemorySearchResult& proto_result =
        response.results(i);
    MemorySearchResult pcontext_result =
        ConvertToMemorySearchResult(proto_result);
    pcontext_result.remote_response_index = i;
    if (!pcontext_result.value.empty()) {
      remote_results.push_back(std::move(pcontext_result));
    }
  }
  return remote_results;
}

std::vector<personal_context::proto::MemoryDataType>
GetSupportedLocalDataTypes() {
  std::vector<personal_context::proto::MemoryDataType> types;
  for (int i = personal_context::proto::MemoryDataType_MIN + 1;
       i <= personal_context::proto::MemoryDataType_MAX; ++i) {
    if (personal_context::proto::MemoryDataType_IsValid(i)) {
      types.push_back(static_cast<personal_context::proto::MemoryDataType>(i));
    }
  }
  return types;
}

personal_context::proto::AtMemoryQueryRequest BuildAtMemoryQueryRequest(
    std::u16string_view query,
    const std::string& locale) {
  personal_context::proto::AtMemoryQueryRequest request;
  request.set_input_query(base::UTF16ToUTF8(query));
  request.set_locale(locale);
  for (auto type : GetSupportedLocalDataTypes()) {
    request.add_supported_local_data_types(type);
  }
  return request;
}

// Tokenizes `text` using native word boundaries and counts the number of
// occurrences of any filter word in `filter_words_set`.
size_t CountFilterWordMatchesInText(
    std::u16string_view text,
    const base::flat_set<std::u16string>& filter_words_set) {
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init()) {
    return 0;
  }

  size_t count = 0;
  while (iter.Advance()) {
    if (iter.IsWord()) {
      std::u16string word = base::i18n::FoldCase(iter.GetString());
      if (filter_words_set.contains(word)) {
        count++;
      }
    }
  }
  return count;
}

// Counts total filter word matches across `entry`'s value and any of its
// metadata values.
size_t CountFilterWordMatchesInEntry(
    const MemorySearchResult& entry,
    const base::flat_set<std::u16string>& filter_words_set) {
  size_t count = CountFilterWordMatchesInText(entry.value, filter_words_set);
  for (const EntryMetadata& metadata : entry.metadata_list) {
    count += CountFilterWordMatchesInText(metadata.value, filter_words_set);
  }
  return count;
}

// Deduplicates search results in `MemorySearchResults`.
// An entry is considered a duplicate if its `type`, `value` and its
// `metadata_list` are identical to an entry already in the unique set.
// The first occurrence of a duplicate entry is preserved, maintaining its
// relative order and other fields (like confidence_score). The `sources` of
// subsequent duplicates are merged into the preserved entry.
void DeduplicateResults(std::vector<MemorySearchResult>& results) {
  std::vector<MemorySearchResult> unique_results;
  unique_results.reserve(results.size());
  for (MemorySearchResult& result : results) {
    auto it = std::ranges::find_if(
        unique_results, [&result](const MemorySearchResult& existing) {
          return existing.type == result.type &&
                 existing.value == result.value &&
                 existing.metadata_list == result.metadata_list;
        });
    if (it != unique_results.end()) {
      for (MemoryEntrySource& source : result.sources) {
        if (!std::ranges::contains(it->sources, source)) {
          it->sources.push_back(std::move(source));
        }
      }
    } else {
      unique_results.push_back(std::move(result));
    }
  }
  results = std::move(unique_results);
}

// Filters search results by retaining only those entries that have the maximum
// number of matching filter words. If `filter_words` is empty or if no entry
// matches any filter word, all entries are returned.
std::vector<MemorySearchResult> FilterResults(
    const std::vector<MemorySearchResult>& entries,
    const base::flat_set<std::u16string>& filter_words) {
  if (filter_words.empty()) {
    return entries;
  }

  std::vector<MemorySearchResult> filtered_entries;
  filtered_entries.reserve(entries.size());
  size_t max_matches = 0;
  for (const MemorySearchResult& entry : entries) {
    size_t count = CountFilterWordMatchesInEntry(entry, filter_words);
    if (count > max_matches) {
      max_matches = count;
      filtered_entries.clear();
      filtered_entries.push_back(entry);
    } else if (count == max_matches) {
      filtered_entries.push_back(entry);
    }
  }

  return filtered_entries;
}

MemorySearchStatus MapContextMemoryError(
    personal_context::ContextMemoryError::ExecutionError error) {
  switch (error) {
    case personal_context::ContextMemoryError::ExecutionError::
        kPermissionDenied:
    case personal_context::ContextMemoryError::ExecutionError::
        kRequestThrottled:
    case personal_context::ContextMemoryError::ExecutionError::kRetryableError:
    case personal_context::ContextMemoryError::ExecutionError::
        kNonRetryableError:
    case personal_context::ContextMemoryError::ExecutionError::kCancelled:
    case personal_context::ContextMemoryError::ExecutionError::
        kResponseParseError:
    case personal_context::ContextMemoryError::ExecutionError::kInvalidRequest:
    case personal_context::ContextMemoryError::ExecutionError::kGenericFailure:
    case personal_context::ContextMemoryError::ExecutionError::kUnknown:
      return MemorySearchStatus::kInternalFailure;
  }
}

// Returns the parent full memory data type for a given sub-type, if one exists.
std::optional<MemoryDataType> GetParentMemoryDataType(
    MemoryDataType data_type) {
  switch (data_type) {
    case MemoryDataType::kAddressStreetAddress:
    case MemoryDataType::kAddressCity:
    case MemoryDataType::kAddressState:
    case MemoryDataType::kAddressZip:
    case MemoryDataType::kAddressCountry:
      return MemoryDataType::kAddressFull;
    case MemoryDataType::kVehicleMake:
    case MemoryDataType::kVehicleModel:
    case MemoryDataType::kVehicleYear:
    case MemoryDataType::kVehicleOwner:
    case MemoryDataType::kVehiclePlateNumber:
    case MemoryDataType::kVehiclePlateState:
    case MemoryDataType::kVehicleVin:
      return MemoryDataType::kVehicle;
    case MemoryDataType::kPassportName:
    case MemoryDataType::kPassportCountry:
    case MemoryDataType::kPassportNumber:
    case MemoryDataType::kPassportIssueDate:
    case MemoryDataType::kPassportExpirationDate:
      return MemoryDataType::kPassportFull;
    case MemoryDataType::kFlightReservationFlightNumber:
    case MemoryDataType::kFlightReservationTicketNumber:
    case MemoryDataType::kFlightReservationConfirmationCode:
    case MemoryDataType::kFlightReservationPassengerName:
    case MemoryDataType::kFlightReservationDepartureAirport:
    case MemoryDataType::kFlightReservationArrivalAirport:
    case MemoryDataType::kFlightReservationDepartureDate:
    case MemoryDataType::kFlightReservationArrivalDate:
      return MemoryDataType::kFlightReservationFull;
    case MemoryDataType::kShipmentTrackingNumber:
    case MemoryDataType::kShipmentAssociatedOrderId:
    case MemoryDataType::kShipmentDeliveryAddress:
    case MemoryDataType::kShipmentDeliveryZipCode:
    case MemoryDataType::kShipmentCarrierName:
    case MemoryDataType::kShipmentCarrierDomain:
    case MemoryDataType::kShipmentEstimatedDeliveryDate:
    case MemoryDataType::kShipmentShippedDate:
      return MemoryDataType::kShipmentFull;
    case MemoryDataType::kNationalIdCardName:
    case MemoryDataType::kNationalIdCardCountry:
    case MemoryDataType::kNationalIdCardNumber:
    case MemoryDataType::kNationalIdCardIssueDate:
    case MemoryDataType::kNationalIdCardExpirationDate:
      return MemoryDataType::kNationalIdCardFull;
    case MemoryDataType::kRedressNumberName:
    case MemoryDataType::kRedressNumberNumber:
      return MemoryDataType::kRedressNumberFull;
    case MemoryDataType::kKnownTravelerNumberName:
    case MemoryDataType::kKnownTravelerNumberNumber:
    case MemoryDataType::kKnownTravelerNumberExpirationDate:
      return MemoryDataType::kKnownTravelerNumberFull;
    case MemoryDataType::kDriversLicenseName:
    case MemoryDataType::kDriversLicenseState:
    case MemoryDataType::kDriversLicenseNumber:
    case MemoryDataType::kDriversLicenseIssueDate:
    case MemoryDataType::kDriversLicenseExpirationDate:
      return MemoryDataType::kDriversLicenseFull;
    case MemoryDataType::kOrderId:
    case MemoryDataType::kOrderAccount:
    case MemoryDataType::kOrderDate:
    case MemoryDataType::kOrderMerchantName:
    case MemoryDataType::kOrderMerchantDomain:
    case MemoryDataType::kOrderProductNames:
    case MemoryDataType::kOrderGrandTotal:
      return MemoryDataType::kOrderFull;
    case MemoryDataType::kIbanNickname:
      return MemoryDataType::kIban;
    case MemoryDataType::kCreditCardExpirationDate:
    case MemoryDataType::kCreditCardSecurityCode:
    case MemoryDataType::kCreditCardNameOnCard:
    case MemoryDataType::kCreditCardNickname:
      return MemoryDataType::kCreditCardNumber;
    // Types without a parent data type.
    case MemoryDataType::kUnknown:
    case MemoryDataType::kNameFull:
    case MemoryDataType::kAddressFull:
    case MemoryDataType::kPhone:
    case MemoryDataType::kEmail:
    case MemoryDataType::kCompanyName:
    case MemoryDataType::kIban:
    case MemoryDataType::kVehicle:
    case MemoryDataType::kPassportFull:
    case MemoryDataType::kFlightReservationFull:
    case MemoryDataType::kShipmentFull:
    case MemoryDataType::kNationalIdCardFull:
    case MemoryDataType::kRedressNumberFull:
    case MemoryDataType::kKnownTravelerNumberFull:
    case MemoryDataType::kDriversLicenseFull:
    case MemoryDataType::kOrderFull:
    case MemoryDataType::kCreditCardNumber:
      return std::nullopt;
  }
}

// Returns true if `data_type` represents a dynamic transaction type (e.g.
// Shipment or Order).
bool IsDynamicTransactionType(MemoryDataType data_type) {
  std::optional<MemoryDataType> parent = GetParentMemoryDataType(data_type);
  MemoryDataType root_type = parent.value_or(data_type);
  return root_type == MemoryDataType::kShipmentFull ||
         root_type == MemoryDataType::kOrderFull;
}

// Ranks `local_results` and `remote_results` search results.
// - Local and remote results are sorted by confidence score descending.
// - If all results are dynamic transaction types, remote
//   results are prioritized on top of local results.
// - Otherwise, local results take priority over remote results.
std::vector<MemorySearchResult> RankResults(
    std::vector<MemorySearchResult> local_results,
    std::vector<MemorySearchResult> remote_results) {
  auto compare_confidence = [](const MemorySearchResult& a,
                               const MemorySearchResult& b) {
    return a.confidence_score > b.confidence_score;
  };
  std::ranges::stable_sort(local_results, compare_confidence);
  std::ranges::stable_sort(remote_results, compare_confidence);

  bool all_dynamic =
      std::ranges::all_of(local_results, IsDynamicTransactionType,
                          &MemorySearchResult::type) &&
      std::ranges::all_of(remote_results, IsDynamicTransactionType,
                          &MemorySearchResult::type);

  std::vector<MemorySearchResult> ranked_results;
  if (all_dynamic) {
    ranked_results = std::move(remote_results);
    base::Extend(ranked_results, std::move(local_results));
  } else {
    ranked_results = std::move(local_results);
    base::Extend(ranked_results, std::move(remote_results));
  }

  return ranked_results;
}

// Rationalizes `data_types` returned in an `AutofillFetchPlan`.
// Groups types and removes sub-types when the parent full type is present and
// removes duplicate types while preserving original insertion order.
std::vector<MemoryDataType> RationalizeFetchPlanDataTypes(
    const std::vector<MemoryDataType>& data_types) {
  base::flat_set<MemoryDataType> present_types(data_types);
  std::vector<MemoryDataType> rationalized;
  base::flat_set<MemoryDataType> seen;
  for (MemoryDataType type : data_types) {
    std::optional<MemoryDataType> parent = GetParentMemoryDataType(type);
    if (parent.has_value() && present_types.contains(*parent)) {
      continue;
    }
    if (type != MemoryDataType::kUnknown && seen.insert(type).second) {
      rationalized.push_back(type);
    }
  }
  return rationalized;
}

// For debugging purposes only. Runs a debug query that directly retrieves
// local suggestions via `data_provider`, bypassing query classification and
// remote resolution.
void QueryPersonalContextDebug(
    AutofillDataProvider* data_provider,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback) {
  if (!data_provider) {
    update_callback.Run(
        MemorySearchResults(MemorySearchStatus::kInternalFailure));
    return;
  }
  data_provider->RetrieveAll(
      {static_cast<MemoryDataType>(
          personal_context::features::debug::kMockPersonalContextResultTypeParam
              .Get())},
      base::BindOnce(
          [](base::RepeatingCallback<void(MemorySearchResults)> update_cb,
             std::vector<MemorySearchResult> results) {
            DeduplicateResults(results);
            update_cb.Run(MemorySearchResults(
                MemorySearchStatus::kFinalResponseSuccess, std::move(results)));
          },
          std::move(update_callback)));
}

}  // namespace

AtMemoryQueryService::AtMemoryQueryService(
    std::unique_ptr<AutofillDataProvider> data_provider,
    personal_context::PersonalContextService* personal_context_service,
    const std::string& locale)
    : data_provider_(std::move(data_provider)),
      personal_context_service_(personal_context_service),
      locale_(locale) {}

AtMemoryQueryService::~AtMemoryQueryService() = default;

void AtMemoryQueryService::Shutdown() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  data_provider_.reset();
  personal_context_service_ = nullptr;
}

void AtMemoryQueryService::Query(
    std::u16string_view query,
    base::RepeatingCallback<void(MemorySearchResults)> callback) {
  // Invalidate any in-flight queries.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (net::NetworkChangeNotifier::IsOffline()) {
    callback.Run(MemorySearchResults(MemorySearchStatus::kNoConnectionFailure));
    return;
  }
  if (base::FeatureList::IsEnabled(
          personal_context::features::debug::kMockPersonalContextResult)) {
    QueryPersonalContextDebug(data_provider_.get(), callback);
    return;
  }

  if (!personal_context_service_) {
    callback.Run(MemorySearchResults(MemorySearchStatus::kInternalFailure));
    return;
  }

  personal_context::proto::AtMemoryQueryRequest request_metadata =
      BuildAtMemoryQueryRequest(query, locale_);

  personal_context::ContextMemoryRequestOptions options;
  // TODO(crbug.com/525668259): Control this timeout via a Finch parameter.
  options.request_timeout = base::Seconds(30);
  personal_context_service_->FetchContext(
      personal_context::proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY,
      request_metadata, options,
      base::BindOnce(&AtMemoryQueryService::OnPersonalContextRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void AtMemoryQueryService::OnPersonalContextRetrieved(
    base::RepeatingCallback<void(MemorySearchResults)> callback,
    personal_context::FetchContextResult result) {
  if (!result.response.has_value()) {
    personal_context::ContextMemoryError::ExecutionError error =
        result.response.error().error();
    if (error ==
        personal_context::ContextMemoryError::ExecutionError::kCancelled) {
      return;
    }
    callback.Run(MemorySearchResults(MapContextMemoryError(error)));
    return;
  }

  AtMemoryQueryResponse response;
  if (!response.ParseFromString(result.response->value())) {
    callback.Run(MemorySearchResults(MemorySearchStatus::kInternalFailure));
    return;
  }

  switch (response.query_classification()) {
    case AtMemoryQueryResponse::QUERY_CLASSIFICATION_AT_MEMORY:
      break;
    case AtMemoryQueryResponse::QUERY_CLASSIFICATION_UNSUPPORTED:
    case AtMemoryQueryResponse::QUERY_CLASSIFICATION_SENSITIVE:
      // TODO(crbug.com/532082682): Clarify what
      // `QUERY_CLASSIFICATION_SENSITIVE` should result in.
      callback.Run(MemorySearchResults(MemorySearchStatus::kUnsupportedQuery));
      return;
    case AtMemoryQueryResponse::QUERY_CLASSIFICATION_UNSPECIFIED:
    default:
      callback.Run(MemorySearchResults(MemorySearchStatus::kInternalFailure));
      return;
  }

  std::vector<MemorySearchResult> remote_results =
      ExtractRemoteResults(response);

  std::vector<MemoryDataType> local_data_types;
  base::flat_set<std::u16string> filter_words;
  if (response.has_autofill_fetch_plan()) {
    const personal_context::proto::AutofillFetchPlan& plan =
        response.autofill_fetch_plan();
    local_data_types = base::ToVector(plan.data_types(), [](int type) {
      return ToMemoryDataType(
          static_cast<personal_context::proto::MemoryDataType>(type));
    });
    local_data_types = RationalizeFetchPlanDataTypes(local_data_types);
    filter_words = base::MakeFlatSet<std::u16string>(
        plan.filter_keywords(), {}, [](const std::string& word) {
          return base::i18n::FoldCase(base::UTF8ToUTF16(word));
        });
  }

  if (local_data_types.empty() || !data_provider_) {
    std::vector<MemorySearchResult> ranked_results =
        RankResults(/*local_results=*/{}, std::move(remote_results));
    DeduplicateResults(ranked_results);
    callback.Run(MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess,
                                     std::move(ranked_results)));
    return;
  }

  data_provider_->RetrieveAll(
      local_data_types,
      base::BindOnce(&AtMemoryQueryService::OnLocalDataRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), callback,
                     std::move(remote_results), std::move(filter_words)));
}

void AtMemoryQueryService::OnLocalDataRetrieved(
    base::RepeatingCallback<void(MemorySearchResults)> callback,
    std::vector<MemorySearchResult> remote_results,
    base::flat_set<std::u16string> filter_words,
    std::vector<MemorySearchResult> local_results) {
  base::UmaHistogramCounts1000(
      "AccessibilityAnnotator.AtMemoryQueryService.ProviderResultCount."
      "AutofillDataProvider",
      local_results.size());

  std::vector<MemorySearchResult> filtered_local_results =
      FilterResults(local_results, filter_words);
  std::vector<MemorySearchResult> ranked_results =
      RankResults(std::move(filtered_local_results), std::move(remote_results));
  DeduplicateResults(ranked_results);

  callback.Run(MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess,
                                   std::move(ranked_results)));
}

}  // namespace autofill
