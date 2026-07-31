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
#include "base/task/sequenced_task_runner.h"
#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/at_memory/autofill_data_provider.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/personal_context/proto/features/at_memory.pb.h"
#include "net/base/network_change_notifier.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"

namespace autofill {

namespace {

using ::personal_context::proto::AtMemoryQueryResponse;

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
    const GURL& url,
    std::u16string_view title,
    const std::string& locale) {
  personal_context::proto::AtMemoryQueryRequest request;
  request.set_input_query(base::UTF16ToUTF8(query));
  request.set_url(url.spec());
  request.set_title(base::UTF16ToUTF8(title));
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

// Returns a string_view to the value of the given `type` in the `result`, or
// `std::nullopt` if it doesn't exist. This checks both the primary result type
// and the `metadata_list`. If the attribute was the primary attribute when
// creating the result from an entity, it might have been omitted from the
// `metadata_list`. In that case, we can use `result.type`
// to identify it.
std::optional<std::u16string_view> GetValueForMemoryDataType(
    const MemorySearchResult& result,
    MemoryDataType type) {
  if (result.type == type) {
    return result.value;
  }
  auto it = std::ranges::find(result.metadata_list, type, &EntryMetadata::type);
  if (it != result.metadata_list.end()) {
    return it->value;
  }
  return std::nullopt;
}

// Returns whether two results are considered duplicates and should be merged.
// To be a duplicate, both results must have the exact same textual `value`.
// Since two identical values (e.g. "Peter") could come from two different
// real-world entities (e.g. two separate passports), we also need to ensure
// they originate from the same real-world entity.
//
// For Autofill AI Entities, we determine if they come from the same real-world
// entity by checking if they satisfy "merge constraints" from the entity schema
// (e.g. if both search results belong to passports with the same passport
// number). If they do and their `value`s match, we deduplicate them so we
// don't show the user two identical suggestions. If they don't, they fall back
// to the regular deduplication flow.
//
// For other Autofill data types (Addresses, CreditCards, Ibans) and unknown
// types, determining if they come from the same real-world entity is done by
// checking that there is no contradicting metadata.
bool AreResultsDuplicates(const MemorySearchResult& a,
                          const MemorySearchResult& b) {
  if (a.type != b.type) {
    return false;
  }
  if (a.type == MemoryDataType::kUnknown &&
      (a.type_name != b.type_name || a.type_name.empty())) {
    return false;
  }
  if (base::i18n::FoldCase(a.value) != base::i18n::FoldCase(b.value)) {
    return false;
  }

  std::optional<AtMemoryDataType> at_memory_type = ToAtMemoryDataType(a.type);
  std::optional<EntityType> entity_type;
  if (at_memory_type) {
    std::visit(
        absl::Overload([&](const EntityType& e_type) { entity_type = e_type; },
                       [&](const AttributeType& a_type) {
                         // Extract the parent entity type to use its merge
                         // constraints for deduplication.
                         entity_type = a_type.entity_type();
                       },
                       [](const auto&) {}),
        *at_memory_type);
  }

  if (entity_type) {
    // For Autofill AI entities, we can use merge constraints to evaluate if
    // `a` and `b` correspond to the same entity.
    for (const DenseSet<AttributeType>& constraint :
         entity_type->merge_constraints()) {
      if (std::ranges::all_of(constraint, [&](AttributeType attr_type) {
            MemoryDataType mem_type = AttributeTypeToMemoryDataType(attr_type);
            std::optional<std::u16string_view> val_a =
                GetValueForMemoryDataType(a, mem_type);
            std::optional<std::u16string_view> val_b =
                GetValueForMemoryDataType(b, mem_type);
            return val_a && val_b &&
                   base::i18n::FoldCase(*val_a) == base::i18n::FoldCase(*val_b);
          })) {
        return true;
      }
    }
  }

  auto has_contradicting_metadata = [](const MemorySearchResult& result,
                                       const EntryMetadata& meta) {
    return std::ranges::any_of(
        result.metadata_list, [&meta](const EntryMetadata& result_meta) {
          return result_meta.type == meta.type &&
                 result_meta.type_name == meta.type_name &&
                 base::i18n::FoldCase(result_meta.value) !=
                     base::i18n::FoldCase(meta.value);
        });
  };

  return std::ranges::all_of(a.metadata_list,
                             [&](const EntryMetadata& meta_a) {
                               return !has_contradicting_metadata(b, meta_a);
                             }) &&
         std::ranges::all_of(b.metadata_list, [&](const EntryMetadata& meta_b) {
           return !has_contradicting_metadata(a, meta_b);
         });
}

// Returns the number of metadata fields in the result that have a non-empty
// value.
int CountNonEmptyMetadata(const MemorySearchResult& result) {
  return std::ranges::count_if(
      result.metadata_list,
      [](const EntryMetadata& meta) { return !meta.value.empty(); });
}

// Returns whether the result has Autofill as a source.
bool IsAutofillSourced(const MemorySearchResult& result) {
  return std::ranges::contains(result.sources, MemoryEntrySourceType::kAutofill,
                               &MemoryEntrySource::type);
}

// Primary logic for resolving duplicates.
// Returns `true` if `first` should be preferred over `second`.
// Specifically:
// 1. Pick explicitly saved local Autofill entities first.
// 2. Otherwise pick the one with more valid metadata fields.
// 3. In case of a tie, prioritize Autofill-sourced provider if possible.
bool PreferFirstResult(const MemorySearchResult& first,
                       const MemorySearchResult& second) {
  bool first_is_local = first.is_local;
  bool second_is_local = second.is_local;
  if (first_is_local != second_is_local) {
    return first_is_local;
  }

  int first_metadata = CountNonEmptyMetadata(first);
  int second_metadata = CountNonEmptyMetadata(second);
  if (first_metadata != second_metadata) {
    return first_metadata > second_metadata;
  }

  bool first_is_autofill = IsAutofillSourced(first);
  bool second_is_autofill = IsAutofillSourced(second);
  if (first_is_autofill != second_is_autofill) {
    return first_is_autofill;
  }

  return true;
}

// Deduplicates search results in `MemorySearchResults`.
// For Autofill AI entities, we use merge constraints to evaluate if results
// correspond to the same underlying entity. When duplicates are found, only the
// "better" result is kept. The better result is determined by
// `PreferFirstResult()`, which prioritizes locally stored entities (like
// addresses or credit cards) and falls back to comparing which result has
// more complete metadata. The `sources` from the discarded duplicate are
// intentionally not merged, as we only want to keep the actually relevant
// sources that link to the correct "manage" UI surface for the kept entry.
void DeduplicateResults(std::vector<MemorySearchResult>& results) {
  std::vector<MemorySearchResult> unique_results;
  unique_results.reserve(results.size());
  for (MemorySearchResult& result : results) {
    auto it = std::ranges::find_if(
        unique_results, [&result](const MemorySearchResult& existing) {
          return AreResultsDuplicates(existing, result);
        });
    if (it != unique_results.end()) {
      if (!PreferFirstResult(*it, result)) {
        *it = std::move(result);
      }
    } else {
      unique_results.push_back(std::move(result));
    }
  }
  results = std::move(unique_results);
}

// Reorders secondary metadata attributes in each suggestion by uniqueness.
// Attributes with lower frequency of the same type (more unique values for a
// given attribute type across all suggestions) appear first. Ties preserve
// their original relative order.
// TODO(crbug.com/535951437): Improve handling on `MemoryDataType::kUnknown`
// results.
void ReorderMetadataByUniqueness(std::vector<MemorySearchResult>& results) {
  absl::flat_hash_map<std::pair<MemoryDataType, std::u16string>, size_t>
      frequency_map;
  for (const MemorySearchResult& result : results) {
    for (const EntryMetadata& metadata : result.metadata_list) {
      frequency_map[{metadata.type, metadata.value}]++;
    }
  }

  for (MemorySearchResult& result : results) {
    std::ranges::stable_sort(result.metadata_list,
                             /*comp=*/{},
                             /*proj=*/[&frequency_map](const EntryMetadata& m) {
                               return frequency_map[{m.type, m.value}];
                             });
  }
}

// Filters search results by retaining only those entries that have the maximum
// number of matching filter words. If `filter_words` is empty, all entries are
// returned. If `filter_words` is provided but no entry matches any filter word,
// an empty vector is returned.
std::vector<MemorySearchResult> FilterResults(
    const std::vector<MemorySearchResult>& entries,
    const base::flat_set<std::u16string>& filter_words) {
  if (filter_words.empty()) {
    return entries;
  }

  std::vector<MemorySearchResult> filtered_entries;
  size_t max_matches = 0;
  for (const MemorySearchResult& entry : entries) {
    size_t count = CountFilterWordMatchesInEntry(entry, filter_words);
    if (count <= 0) {
      continue;
    }

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

// Returns true if `data_type` represents a dynamic transaction type (e.g.
// Shipment or Order).
bool IsDynamicTransactionType(MemoryDataType data_type) {
  switch (data_type) {
    case MemoryDataType::kOrderId:
    case MemoryDataType::kOrderAccount:
    case MemoryDataType::kOrderDate:
    case MemoryDataType::kOrderMerchantName:
    case MemoryDataType::kOrderMerchantDomain:
    case MemoryDataType::kOrderProductNames:
    case MemoryDataType::kOrderGrandTotal:
    case MemoryDataType::kShipmentTrackingNumber:
    case MemoryDataType::kShipmentAssociatedOrderId:
    case MemoryDataType::kShipmentDeliveryAddress:
    case MemoryDataType::kShipmentDeliveryZipCode:
    case MemoryDataType::kShipmentCarrierName:
    case MemoryDataType::kShipmentCarrierDomain:
    case MemoryDataType::kShipmentEstimatedDeliveryDate:
    case MemoryDataType::kShipmentShippedDate:
      return true;
    case MemoryDataType::kNameFull:
    case MemoryDataType::kAddressFull:
    case MemoryDataType::kAddressStreetAddress:
    case MemoryDataType::kAddressCity:
    case MemoryDataType::kAddressState:
    case MemoryDataType::kAddressZip:
    case MemoryDataType::kAddressCountry:
    case MemoryDataType::kPhone:
    case MemoryDataType::kEmail:
    case MemoryDataType::kCompanyName:
    case MemoryDataType::kIban:
    case MemoryDataType::kIbanNickname:
    case MemoryDataType::kVehicleMake:
    case MemoryDataType::kVehicleModel:
    case MemoryDataType::kVehicleYear:
    case MemoryDataType::kVehicleOwner:
    case MemoryDataType::kVehiclePlateNumber:
    case MemoryDataType::kVehiclePlateState:
    case MemoryDataType::kVehicleVin:
    case MemoryDataType::kPassportName:
    case MemoryDataType::kPassportCountry:
    case MemoryDataType::kPassportNumber:
    case MemoryDataType::kPassportIssueDate:
    case MemoryDataType::kPassportExpirationDate:
    case MemoryDataType::kFlightReservationFlightNumber:
    case MemoryDataType::kFlightReservationTicketNumber:
    case MemoryDataType::kFlightReservationConfirmationCode:
    case MemoryDataType::kFlightReservationPassengerName:
    case MemoryDataType::kFlightReservationDepartureAirport:
    case MemoryDataType::kFlightReservationArrivalAirport:
    case MemoryDataType::kFlightReservationDepartureDate:
    case MemoryDataType::kFlightReservationArrivalDate:
    case MemoryDataType::kNationalIdCardName:
    case MemoryDataType::kNationalIdCardCountry:
    case MemoryDataType::kNationalIdCardNumber:
    case MemoryDataType::kNationalIdCardIssueDate:
    case MemoryDataType::kNationalIdCardExpirationDate:
    case MemoryDataType::kRedressNumberName:
    case MemoryDataType::kRedressNumberNumber:
    case MemoryDataType::kKnownTravelerNumberName:
    case MemoryDataType::kKnownTravelerNumberNumber:
    case MemoryDataType::kKnownTravelerNumberExpirationDate:
    case MemoryDataType::kDriversLicenseName:
    case MemoryDataType::kDriversLicenseState:
    case MemoryDataType::kDriversLicenseNumber:
    case MemoryDataType::kDriversLicenseIssueDate:
    case MemoryDataType::kDriversLicenseExpirationDate:
    case MemoryDataType::kCreditCardNumber:
    case MemoryDataType::kCreditCardExpirationDate:
    case MemoryDataType::kCreditCardSecurityCode:
    case MemoryDataType::kCreditCardNameOnCard:
    case MemoryDataType::kCreditCardNickname:
    case MemoryDataType::kUnknown:
      return false;
  }
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
// Removes duplicate types and filters out unknown types while preserving the
// original insertion order. Also discards secondary Autofill AI attributes if
// their corresponding primary attribute is present in the set.
std::vector<MemoryDataType> RationalizeFetchPlanDataTypes(
    const std::vector<MemoryDataType>& data_types) {
  base::flat_set<MemoryDataType> present_types(data_types);
  std::vector<MemoryDataType> rationalized;
  base::flat_set<MemoryDataType> seen;
  for (MemoryDataType type : data_types) {
    if (type == MemoryDataType::kUnknown) {
      continue;
    }

    if (std::optional<AtMemoryDataType> internal_type =
            ToAtMemoryDataType(type)) {
      if (const auto* attribute_type =
              std::get_if<AttributeType>(&*internal_type)) {
        AttributeType primary_type =
            GetPrimaryAttributeType(attribute_type->entity_type());
        if (*attribute_type != primary_type) {
          MemoryDataType primary_memory_type =
              AttributeTypeToMemoryDataType(primary_type);
          if (present_types.contains(primary_memory_type)) {
            continue;
          }
        }
      }
    }

    if (seen.insert(type).second) {
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
            ReorderMetadataByUniqueness(results);
            update_cb.Run(MemorySearchResults(
                MemorySearchStatus::kFinalResponseSuccess, std::move(results)));
          },
          std::move(update_callback)));
}

// Extracts the unmasked PII value from `entity` based on the requested
// `data_type`.
std::optional<std::u16string> GetUnmaskedPiiFromEntity(
    const personal_context::proto::Entity& entity,
    MemoryDataType data_type) {
  switch (data_type) {
    case MemoryDataType::kPassportNumber:
      if (entity.has_passport()) {
        return base::UTF8ToUTF16(entity.passport().number());
      }
      break;
    case MemoryDataType::kDriversLicenseNumber:
      if (entity.has_drivers_license()) {
        return base::UTF8ToUTF16(entity.drivers_license().number());
      }
      break;
    case MemoryDataType::kNationalIdCardNumber:
      if (entity.has_national_id()) {
        return base::UTF8ToUTF16(entity.national_id().number());
      }
      break;
    case MemoryDataType::kKnownTravelerNumberNumber:
      if (entity.has_known_traveler_number()) {
        return base::UTF8ToUTF16(entity.known_traveler_number().number());
      }
      break;
    case MemoryDataType::kUnknown:
    case MemoryDataType::kNameFull:
    case MemoryDataType::kAddressFull:
    case MemoryDataType::kAddressStreetAddress:
    case MemoryDataType::kAddressCity:
    case MemoryDataType::kAddressState:
    case MemoryDataType::kAddressZip:
    case MemoryDataType::kAddressCountry:
    case MemoryDataType::kPhone:
    case MemoryDataType::kEmail:
    case MemoryDataType::kCompanyName:
    case MemoryDataType::kIban:
    case MemoryDataType::kIbanNickname:
    case MemoryDataType::kVehicleMake:
    case MemoryDataType::kVehicleModel:
    case MemoryDataType::kVehicleYear:
    case MemoryDataType::kVehicleOwner:
    case MemoryDataType::kVehiclePlateNumber:
    case MemoryDataType::kVehiclePlateState:
    case MemoryDataType::kVehicleVin:
    case MemoryDataType::kPassportName:
    case MemoryDataType::kPassportCountry:
    case MemoryDataType::kPassportIssueDate:
    case MemoryDataType::kPassportExpirationDate:
    case MemoryDataType::kFlightReservationFlightNumber:
    case MemoryDataType::kFlightReservationTicketNumber:
    case MemoryDataType::kFlightReservationConfirmationCode:
    case MemoryDataType::kFlightReservationPassengerName:
    case MemoryDataType::kFlightReservationDepartureAirport:
    case MemoryDataType::kFlightReservationArrivalAirport:
    case MemoryDataType::kFlightReservationDepartureDate:
    case MemoryDataType::kFlightReservationArrivalDate:
    case MemoryDataType::kShipmentTrackingNumber:
    case MemoryDataType::kShipmentAssociatedOrderId:
    case MemoryDataType::kShipmentDeliveryAddress:
    case MemoryDataType::kShipmentDeliveryZipCode:
    case MemoryDataType::kShipmentCarrierName:
    case MemoryDataType::kShipmentCarrierDomain:
    case MemoryDataType::kShipmentEstimatedDeliveryDate:
    case MemoryDataType::kShipmentShippedDate:
    case MemoryDataType::kNationalIdCardName:
    case MemoryDataType::kNationalIdCardCountry:
    case MemoryDataType::kNationalIdCardIssueDate:
    case MemoryDataType::kNationalIdCardExpirationDate:
    case MemoryDataType::kRedressNumberName:
    case MemoryDataType::kRedressNumberNumber:
    case MemoryDataType::kKnownTravelerNumberName:
    case MemoryDataType::kKnownTravelerNumberExpirationDate:
    case MemoryDataType::kDriversLicenseName:
    case MemoryDataType::kDriversLicenseState:
    case MemoryDataType::kDriversLicenseIssueDate:
    case MemoryDataType::kDriversLicenseExpirationDate:
    case MemoryDataType::kOrderId:
    case MemoryDataType::kOrderAccount:
    case MemoryDataType::kOrderDate:
    case MemoryDataType::kOrderMerchantName:
    case MemoryDataType::kOrderMerchantDomain:
    case MemoryDataType::kOrderProductNames:
    case MemoryDataType::kOrderGrandTotal:
    case MemoryDataType::kCreditCardNumber:
    case MemoryDataType::kCreditCardExpirationDate:
    case MemoryDataType::kCreditCardSecurityCode:
    case MemoryDataType::kCreditCardNameOnCard:
    case MemoryDataType::kCreditCardNickname:
      return std::nullopt;
  }
  return std::nullopt;
}

// Runs the callback asynchronously on the current sequenced task runner.
// Used to ensure consistently asynchronous callback execution across all paths,
// including fast-fail and cancellation paths.
void RunCallbackAsync(
    AtMemoryQueryService::FetchUnmaskedPiiEntitiesCallback callback,
    AtMemoryQueryService::SpiiRetrievalResult result) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

// Called when `PersonalContextService` returns the unmasked PII entities.
// Extracts the unmasked string matching `data_type` and runs the `callback`
// with the result or a failure reason.
void OnFetchPiiEntityCompleted(
    MemoryDataType data_type,
    AtMemoryQueryService::FetchUnmaskedPiiEntitiesCallback callback,
    personal_context::FetchPiiEntitiesResult result) {
  if (!result.response.has_value() || result.response->entities().empty()) {
    RunCallbackAsync(
        std::move(callback),
        base::unexpected(
            AtMemoryQueryService::SpiiRetrievalFailureReason::kFetchFailed));
    return;
  }

  std::optional<std::u16string> unmasked_value =
      GetUnmaskedPiiFromEntity(result.response->entities(0), data_type);
  if (!unmasked_value || unmasked_value->empty()) {
    RunCallbackAsync(
        std::move(callback),
        base::unexpected(
            AtMemoryQueryService::SpiiRetrievalFailureReason::kParseFailed));
    return;
  }

  RunCallbackAsync(std::move(callback), std::move(*unmasked_value));
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
  query_weak_ptr_factory_.InvalidateWeakPtrs();
  pii_unmasking_weak_ptr_factory_.InvalidateWeakPtrs();
  data_provider_.reset();
  personal_context_service_ = nullptr;
  if (device_authenticator_) {
    device_authenticator_->Cancel();
    device_authenticator_.reset();
  }
}

void AtMemoryQueryService::Query(
    std::u16string_view query,
    const GURL& url,
    std::u16string_view title,
    base::RepeatingCallback<void(MemorySearchResults)> callback) {
  // Invalidate any in-flight queries.
  query_weak_ptr_factory_.InvalidateWeakPtrs();

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
      BuildAtMemoryQueryRequest(query, url, title, locale_);

  personal_context::ContextMemoryRequestOptions options;
  options.request_timeout = features::kAutofillAtMemoryRequestTimeout.Get();
  personal_context_service_->FetchContext(
      personal_context::proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY,
      request_metadata, options,
      base::BindOnce(&AtMemoryQueryService::OnPersonalContextRetrieved,
                     query_weak_ptr_factory_.GetWeakPtr(), callback));
}

void AtMemoryQueryService::AuthenticateAndFetchPiiEntity(
    const AutofillClient& client,
    const std::u16string& auth_message,
    std::u16string_view masked_value,
    MemoryDataType data_type,
    base::span<const EntryMetadata> metadata_list,
    FetchUnmaskedPiiEntitiesCallback callback) {
  if (device_authenticator_) {
    RunCallbackAsync(
        std::move(callback),
        base::unexpected(SpiiRetrievalFailureReason::kReauthInProgress));
    return;
  }

  if (net::NetworkChangeNotifier::IsOffline()) {
    RunCallbackAsync(
        std::move(callback),
        base::unexpected(SpiiRetrievalFailureReason::kNoConnection));
    return;
  }

  device_authenticator_ =
      client.GetDeviceAuthenticator("Autofill.AtMemory.ReauthToUnmask");
  if (!device_authenticator_ ||
      !device_authenticator_->CanAuthenticateWithBiometricOrScreenLock()) {
    device_authenticator_.reset();
    RunCallbackAsync(
        std::move(callback),
        base::unexpected(SpiiRetrievalFailureReason::kReauthFailed));
    return;
  }

  device_authenticator_->AuthenticateWithMessage(
      auth_message,
      base::BindOnce(&AtMemoryQueryService::OnAuthenticationCompleted,
                     pii_unmasking_weak_ptr_factory_.GetWeakPtr(),
                     std::u16string(masked_value), data_type,
                     base::ToVector(metadata_list), std::move(callback)));
}

void AtMemoryQueryService::OnPersonalContextRetrieved(
    base::RepeatingCallback<void(MemorySearchResults)> callback,
    personal_context::FetchContextResult result) {
  auto run_callback = [&](MemorySearchStatus status,
                          std::vector<MemorySearchResult> entries = {}) {
    MemorySearchResults search_results(status, std::move(entries));
    search_results.server_request_id = result.server_request_id;
    callback.Run(std::move(search_results));
  };

  if (!result.response.has_value()) {
    personal_context::ContextMemoryError::ExecutionError error =
        result.response.error().error();
    if (error ==
        personal_context::ContextMemoryError::ExecutionError::kCancelled) {
      return;
    }
    run_callback(MapContextMemoryError(error));
    return;
  }

  AtMemoryQueryResponse response;
  if (!response.ParseFromString(result.response->value())) {
    run_callback(MemorySearchStatus::kInternalFailure);
    return;
  }

  switch (response.query_classification()) {
    case AtMemoryQueryResponse::QUERY_CLASSIFICATION_AT_MEMORY:
      break;
    case AtMemoryQueryResponse::QUERY_CLASSIFICATION_UNSUPPORTED:
    case AtMemoryQueryResponse::QUERY_CLASSIFICATION_SENSITIVE:
    case AtMemoryQueryResponse::QUERY_CLASSIFICATION_RECITATION:
      run_callback(MemorySearchStatus::kUnsupportedQuery);
      return;
    case AtMemoryQueryResponse::QUERY_CLASSIFICATION_UNSPECIFIED:
    default:
      run_callback(MemorySearchStatus::kInternalFailure);
      return;
  }

  std::vector<MemorySearchResult> remote_results =
      ExtractRemoteResults(response, locale_);

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
    ReorderMetadataByUniqueness(ranked_results);
    run_callback(MemorySearchStatus::kFinalResponseSuccess,
                 std::move(ranked_results));
    return;
  }

  data_provider_->RetrieveAll(
      local_data_types,
      base::BindOnce(&AtMemoryQueryService::OnLocalDataRetrieved,
                     query_weak_ptr_factory_.GetWeakPtr(), callback,
                     std::move(remote_results), std::move(filter_words),
                     std::move(result.server_request_id)));
}

void AtMemoryQueryService::OnLocalDataRetrieved(
    base::RepeatingCallback<void(MemorySearchResults)> callback,
    std::vector<MemorySearchResult> remote_results,
    base::flat_set<std::u16string> filter_words,
    std::string server_request_id,
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
  ReorderMetadataByUniqueness(ranked_results);

  MemorySearchResults search_results(MemorySearchStatus::kFinalResponseSuccess,
                                     std::move(ranked_results));
  search_results.server_request_id = std::move(server_request_id);
  callback.Run(std::move(search_results));
}

void AtMemoryQueryService::OnAuthenticationCompleted(
    std::u16string masked_value,
    MemoryDataType data_type,
    std::vector<EntryMetadata> metadata_list,
    FetchUnmaskedPiiEntitiesCallback callback,
    bool auth_succeeded) {
  device_authenticator_.reset();
  if (!auth_succeeded) {
    RunCallbackAsync(
        std::move(callback),
        base::unexpected(SpiiRetrievalFailureReason::kReauthFailed));
    return;
  }

  if (!personal_context_service_) {
    RunCallbackAsync(
        std::move(callback),
        base::unexpected(SpiiRetrievalFailureReason::kFetchFailed));
    return;
  }

  personal_context::proto::FetchPiiEntitiesRequest request;
  request.set_feature(
      personal_context::proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY);
  *request.add_masked_entities() =
      ToPersonalContextEntity(masked_value, data_type, metadata_list);

  personal_context::ContextMemoryRequestOptions options;
  options.request_timeout = features::kAutofillAtMemoryRequestTimeout.Get();

  personal_context_service_->FetchPiiEntities(
      request, options,
      base::BindOnce(OnFetchPiiEntityCompleted, data_type,
                     std::move(callback)));
}

}  // namespace autofill
