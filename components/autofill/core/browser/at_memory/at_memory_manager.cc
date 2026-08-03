// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_manager.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"
#include "components/autofill/core/browser/at_memory/at_memory_metrics_recorder.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/autofill_ai/autofill_ai_access_manager.h"
#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/at_memory/at_memory_query_service.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/network_change_notifier.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

Suggestion CreateFetchingSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_FETCHING),
      SuggestionType::kAtMemoryFetching);
  suggestion.acceptability =
      Suggestion::Acceptability::kSelectableButUnacceptable;
  suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  return suggestion;
}

std::optional<Suggestion> CreateManageSuggestion(MemoryDataType type) {
  auto create_suggestion = [](SuggestionType suggestion_type, int string_id) {
    Suggestion suggestion(l10n_util::GetStringUTF16(string_id),
                          suggestion_type);
    suggestion.icon = Suggestion::Icon::kSettings;
    suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
    return suggestion;
  };

  switch (type) {
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
      return create_suggestion(SuggestionType::kManageAddress,
                               IDS_AUTOFILL_AT_MEMORY_MANAGE_CONTACT_INFO);

    case MemoryDataType::kCreditCardNumber:
    case MemoryDataType::kCreditCardExpirationDate:
    case MemoryDataType::kCreditCardSecurityCode:
    case MemoryDataType::kCreditCardNameOnCard:
    case MemoryDataType::kCreditCardNickname:
      return create_suggestion(SuggestionType::kManageCreditCard,
                               IDS_AUTOFILL_MANAGE_PAYMENT_METHODS);

    case MemoryDataType::kIban:
    case MemoryDataType::kIbanNickname:
      return create_suggestion(SuggestionType::kManageIban,
                               IDS_AUTOFILL_MANAGE_PAYMENT_METHODS);

    case MemoryDataType::kPassportName:
    case MemoryDataType::kPassportCountry:
    case MemoryDataType::kPassportNumber:
    case MemoryDataType::kPassportIssueDate:
    case MemoryDataType::kPassportExpirationDate:
    case MemoryDataType::kNationalIdCardName:
    case MemoryDataType::kNationalIdCardCountry:
    case MemoryDataType::kNationalIdCardNumber:
    case MemoryDataType::kNationalIdCardIssueDate:
    case MemoryDataType::kNationalIdCardExpirationDate:
    case MemoryDataType::kDriversLicenseName:
    case MemoryDataType::kDriversLicenseState:
    case MemoryDataType::kDriversLicenseNumber:
    case MemoryDataType::kDriversLicenseIssueDate:
    case MemoryDataType::kDriversLicenseExpirationDate:
      return create_suggestion(
          SuggestionType::kManageAutofillAiIdentityDocs,
          IDS_AUTOFILL_AI_MANAGE_IDENTITY_DOCS_SUGGESTION_MAIN_TEXT);

    case MemoryDataType::kFlightReservationFlightNumber:
    case MemoryDataType::kFlightReservationTicketNumber:
    case MemoryDataType::kFlightReservationConfirmationCode:
    case MemoryDataType::kFlightReservationPassengerName:
    case MemoryDataType::kFlightReservationDepartureAirport:
    case MemoryDataType::kFlightReservationArrivalAirport:
    case MemoryDataType::kFlightReservationDepartureDate:
    case MemoryDataType::kFlightReservationArrivalDate:
    case MemoryDataType::kRedressNumberName:
    case MemoryDataType::kRedressNumberNumber:
    case MemoryDataType::kKnownTravelerNumberName:
    case MemoryDataType::kKnownTravelerNumberNumber:
    case MemoryDataType::kKnownTravelerNumberExpirationDate:
    case MemoryDataType::kVehicleMake:
    case MemoryDataType::kVehicleModel:
    case MemoryDataType::kVehicleYear:
    case MemoryDataType::kVehicleOwner:
    case MemoryDataType::kVehiclePlateNumber:
    case MemoryDataType::kVehiclePlateState:
    case MemoryDataType::kVehicleVin:
      return create_suggestion(
          SuggestionType::kManageAutofillAiTravel,
          IDS_AUTOFILL_AI_MANAGE_TRAVEL_SUGGESTION_MAIN_TEXT);

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
      return create_suggestion(
          SuggestionType::kManageAutofillAiShopping,
          IDS_AUTOFILL_AI_MANAGE_SHOPPING_SUGGESTION_MAIN_TEXT);

    case MemoryDataType::kUnknown:
      return std::nullopt;
  }
}

// Returns the primary type name label for `entry`. For AutofillAi
// entities and attributes, this resolves to the Entity name.
std::u16string GetSuggestionLabelTypeName(const MemorySearchResult& entry) {
  std::optional<AtMemoryDataType> data_type = ToAtMemoryDataType(entry.type);
  if (data_type) {
    if (const AttributeType* attribute_type =
            std::get_if<AttributeType>(&*data_type)) {
      return attribute_type->entity_type().GetNameForI18n();
    }
  }
  return entry.type == MemoryDataType::kUnknown
             ? entry.type_name
             : GetMemoryDataTypeNameForI18n(entry.type);
}

Suggestion::AtMemoryPayload::Identifier GetPayloadIdentifier(
    MemoryDataType type,
    const std::variant<std::monostate, std::string, int64_t>& identifier) {
  if (std::holds_alternative<std::monostate>(identifier)) {
    return std::monostate();
  }

  switch (type) {
    case MemoryDataType::kIban: {
      if (const std::string* guid = std::get_if<std::string>(&identifier)) {
        return Iban::Guid(*guid);
      }
      if (const int64_t* instrument_id = std::get_if<int64_t>(&identifier)) {
        return Iban::InstrumentId(*instrument_id);
      }
      NOTREACHED();
    }
    case MemoryDataType::kPassportNumber:
    case MemoryDataType::kDriversLicenseNumber:
    case MemoryDataType::kNationalIdCardNumber:
    case MemoryDataType::kKnownTravelerNumberNumber:
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
    case MemoryDataType::kOrderGrandTotal: {
      return EntityInstance::EntityId(std::get<std::string>(identifier));
    }
    case MemoryDataType::kCreditCardNumber:
    case MemoryDataType::kCreditCardSecurityCode:
    case MemoryDataType::kCreditCardExpirationDate:
    case MemoryDataType::kCreditCardNameOnCard:
    case MemoryDataType::kNameFull:
    case MemoryDataType::kAddressFull:
    case MemoryDataType::kAddressStreetAddress:
    case MemoryDataType::kAddressCity:
    case MemoryDataType::kAddressState:
    case MemoryDataType::kAddressZip:
    case MemoryDataType::kAddressCountry:
    case MemoryDataType::kPhone:
    case MemoryDataType::kEmail:
    case MemoryDataType::kCompanyName: {
      return std::get<std::string>(identifier);
    }
    case MemoryDataType::kCreditCardNickname:
    case MemoryDataType::kIbanNickname:
    case MemoryDataType::kUnknown:
      return std::monostate();
  }
}

Suggestion::Icon GetIcon(const MemorySearchResult& search_result) {
  const bool is_autofill_only =
      search_result.sources.size() == 1 &&
      search_result.sources.front().type == MemoryEntrySourceType::kAutofill;
  switch (search_result.type) {
    case MemoryDataType::kNameFull:
    case MemoryDataType::kAddressFull:
    case MemoryDataType::kAddressStreetAddress:
    case MemoryDataType::kAddressCity:
    case MemoryDataType::kAddressState:
    case MemoryDataType::kAddressZip:
    case MemoryDataType::kAddressCountry:
    case MemoryDataType::kPhone:
    case MemoryDataType::kCompanyName:
    case MemoryDataType::kEmail:
      return is_autofill_only ? Suggestion::Icon::kLocation
                              : Suggestion::Icon::kLocationSpark;
    case MemoryDataType::kVehicleMake:
    case MemoryDataType::kVehicleModel:
    case MemoryDataType::kVehicleYear:
    case MemoryDataType::kVehicleOwner:
    case MemoryDataType::kVehiclePlateNumber:
    case MemoryDataType::kVehiclePlateState:
    case MemoryDataType::kVehicleVin:
      return is_autofill_only ? Suggestion::Icon::kVehicle
                              : Suggestion::Icon::kVehicleSpark;
    case MemoryDataType::kPassportName:
    case MemoryDataType::kPassportCountry:
    case MemoryDataType::kPassportNumber:
    case MemoryDataType::kPassportIssueDate:
    case MemoryDataType::kPassportExpirationDate:
      return is_autofill_only ? Suggestion::Icon::kPassport
                              : Suggestion::Icon::kPassportSpark;
    case MemoryDataType::kFlightReservationFlightNumber:
    case MemoryDataType::kFlightReservationTicketNumber:
    case MemoryDataType::kFlightReservationConfirmationCode:
    case MemoryDataType::kFlightReservationPassengerName:
    case MemoryDataType::kFlightReservationDepartureAirport:
    case MemoryDataType::kFlightReservationArrivalAirport:
    case MemoryDataType::kFlightReservationDepartureDate:
    case MemoryDataType::kFlightReservationArrivalDate:
      return is_autofill_only ? Suggestion::Icon::kFlight
                              : Suggestion::Icon::kFlightSpark;
    case MemoryDataType::kNationalIdCardName:
    case MemoryDataType::kNationalIdCardCountry:
    case MemoryDataType::kNationalIdCardNumber:
    case MemoryDataType::kNationalIdCardIssueDate:
    case MemoryDataType::kNationalIdCardExpirationDate:
    case MemoryDataType::kDriversLicenseName:
    case MemoryDataType::kDriversLicenseState:
    case MemoryDataType::kDriversLicenseNumber:
    case MemoryDataType::kDriversLicenseIssueDate:
    case MemoryDataType::kDriversLicenseExpirationDate:
      return is_autofill_only ? Suggestion::Icon::kIdCard
                              : Suggestion::Icon::kIdCardSpark;
    case MemoryDataType::kRedressNumberName:
    case MemoryDataType::kRedressNumberNumber:
    case MemoryDataType::kKnownTravelerNumberName:
    case MemoryDataType::kKnownTravelerNumberNumber:
    case MemoryDataType::kKnownTravelerNumberExpirationDate:
      return is_autofill_only ? Suggestion::Icon::kIdCard2
                              : Suggestion::Icon::kIdCard2Spark;
    case MemoryDataType::kCreditCardNumber:
    case MemoryDataType::kCreditCardExpirationDate:
    case MemoryDataType::kCreditCardSecurityCode:
    case MemoryDataType::kCreditCardNameOnCard:
    case MemoryDataType::kCreditCardNickname:
    case MemoryDataType::kIban:
    case MemoryDataType::kIbanNickname:
      return is_autofill_only ? Suggestion::Icon::kCardGenericVector
                              : Suggestion::Icon::kCardGenericSpark;
    case MemoryDataType::kOrderId:
    case MemoryDataType::kOrderAccount:
    case MemoryDataType::kOrderDate:
    case MemoryDataType::kOrderMerchantName:
    case MemoryDataType::kOrderMerchantDomain:
    case MemoryDataType::kOrderProductNames:
    case MemoryDataType::kOrderGrandTotal:
      return is_autofill_only ? Suggestion::Icon::kOrder
                              : Suggestion::Icon::kOrderSpark;
    case MemoryDataType::kShipmentTrackingNumber:
    case MemoryDataType::kShipmentAssociatedOrderId:
    case MemoryDataType::kShipmentDeliveryAddress:
    case MemoryDataType::kShipmentDeliveryZipCode:
    case MemoryDataType::kShipmentCarrierName:
    case MemoryDataType::kShipmentCarrierDomain:
    case MemoryDataType::kShipmentEstimatedDeliveryDate:
    case MemoryDataType::kShipmentShippedDate:
      return is_autofill_only ? Suggestion::Icon::kShipment
                              : Suggestion::Icon::kShipmentSpark;
    case MemoryDataType::kUnknown:
      return is_autofill_only ? Suggestion::Icon::kNoIcon
                              : Suggestion::Icon::kTextSpark;
  }
  NOTREACHED();
}

// Returns true if `entry` is sourced from Autofill.
// We assume that if an `entry` is Autofill-sourced, it is only sourced from
// Autofill (no mixed sources).
bool IsMemorySearchResultAutofillSourced(const MemorySearchResult& entry) {
  const bool is_autofill_sourced =
      std::ranges::contains(entry.sources, MemoryEntrySourceType::kAutofill,
                            &MemoryEntrySource::type);
  // Mixing Autofill with other sources is currently not in scope, and this
  // DCHECK acts as a temporary way to catch violations of this assumption.
  DCHECK(!is_autofill_sourced ||
         (is_autofill_sourced && entry.sources.size() == 1));
  return is_autofill_sourced;
}

// Obfuscates `value` if it is from a non-Autofill source and is sensitive
// information.
std::u16string MaybeObfuscateValue(const std::u16string& value,
                                   MemoryDataType type,
                                   bool is_personal_context_sourced) {
  constexpr size_t kVisibleSuffixLength = 4;
  if (value.empty()) {
    return value;
  }
  if (is_personal_context_sourced && IsSpiiMemoryDataType(type)) {
    return GetObfuscatedValue(value, kVisibleSuffixLength);
  }
  return value;
}

// Metadata are displayed as nested results in the flyout menu.
Suggestion CreateManageEnhancedAutofillSuggestion() {
  Suggestion manage_enhanced_autofill(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ENHANCED_AUTOFILL),
      SuggestionType::kManageEnhancedAutofill);
  manage_enhanced_autofill.icon = Suggestion::Icon::kSettings;
  manage_enhanced_autofill.filtration_policy =
      Suggestion::FiltrationPolicy::kStatic;
  return manage_enhanced_autofill;
}

// Creates secondary suggestions representing metadata items for the given
// AtMemory search result entry.
std::vector<Suggestion> CreateSecondarySuggestions(
    const MemorySearchResult& entry,
    bool is_personal_context_sourced) {
  std::vector<Suggestion> children;
  children.reserve(entry.metadata_list.size());
  for (const EntryMetadata& metadata : entry.metadata_list) {
    Suggestion child(MaybeObfuscateValue(metadata.value, metadata.type,
                                         is_personal_context_sourced),
                     SuggestionType::kAtMemorySearchResult);
    std::u16string child_type_name =
        metadata.type_name.empty() ? GetMemoryDataTypeNameForI18n(metadata.type)
                                   : metadata.type_name;
    if (!child_type_name.empty()) {
      child.labels = {{Suggestion::Text(child_type_name)}};
    }
    Suggestion::AtMemoryPayload child_at_memory_payload(metadata.value,
                                                        metadata.type);
    child_at_memory_payload.type_name = child_type_name;
    child_at_memory_payload.identifier =
        GetPayloadIdentifier(metadata.type, entry.identifier);
    child_at_memory_payload.is_personal_context_sourced =
        is_personal_context_sourced;
    child.payload = std::move(child_at_memory_payload);
    child.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
    children.push_back(std::move(child));
  }
  return children;
}

Suggestion CreateSourceAttributionSuggestion() {
  Suggestion source_info(SuggestionType::kAtMemorySourceAttribution);
  source_info.minor_texts.emplace_back(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_AT_MEMORY_SOURCE_ATTRIBUTION_PERSONAL_INTELLIGENCE));
  source_info.acceptability =
      Suggestion::Acceptability::kSelectableButUnacceptable;
  source_info.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  return source_info;
}

std::vector<Suggestion> CreateFooterSuggestions(
    const MemorySearchResult& entry) {
  std::vector<Suggestion> suggestions;
  if (IsMemorySearchResultAutofillSourced(entry)) {
    if (std::optional<Suggestion> suggestion =
            CreateManageSuggestion(entry.type)) {
      suggestions.emplace_back(std::move(*suggestion));
    }
  } else {
    Suggestion separator(SuggestionType::kSeparator);
    separator.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
    suggestions.reserve(3);
    suggestions.emplace_back(CreateSourceAttributionSuggestion());
    suggestions.emplace_back(std::move(separator));
    suggestions.emplace_back(CreateManageEnhancedAutofillSuggestion());
  }
  return suggestions;
}

Suggestion TransformResultIntoSuggestion(const MemorySearchResult& entry) {
  const bool is_personal_context_sourced =
      !IsMemorySearchResultAutofillSourced(entry);
  Suggestion suggestion(
      MaybeObfuscateValue(entry.value, entry.type, is_personal_context_sourced),
      SuggestionType::kAtMemorySearchResult);
  suggestion.icon = GetIcon(entry);

  // Label row: [type_name, metadata[0].value, ...]
  std::vector<Suggestion::Text> label_row;
  std::u16string type_name = GetSuggestionLabelTypeName(entry);
  if (!type_name.empty()) {
    label_row.emplace_back(type_name);
  }
  for (const EntryMetadata& metadata : entry.metadata_list) {
    // CVCs are always fully obfuscated. They add no value to a label.
    if (metadata.type == MemoryDataType::kCreditCardSecurityCode) {
      continue;
    }
    if (!label_row.empty()) {
      label_row.emplace_back(u"\u2022");  // Bullet (•)
    }
    label_row.emplace_back(MaybeObfuscateValue(metadata.value, metadata.type,
                                               is_personal_context_sourced));
  }
  if (!label_row.empty()) {
    suggestion.labels.emplace_back(std::move(label_row));
  }
  Suggestion::AtMemoryPayload at_memory_payload(entry.value, entry.type);
  at_memory_payload.type_name = std::move(type_name);
  at_memory_payload.identifier =
      GetPayloadIdentifier(entry.type, entry.identifier);
  at_memory_payload.is_personal_context_sourced = is_personal_context_sourced;

  std::underlying_type_t<MemoryEntrySourceType> sources_bitmask = 0;
  for (const auto& source : entry.sources) {
    sources_bitmask |= std::to_underlying(source.type);
  }
  at_memory_payload.sources_bitmask = sources_bitmask;

  suggestion.payload = std::move(at_memory_payload);
  suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;

  suggestion.children =
      CreateSecondarySuggestions(entry, is_personal_context_sourced);
  std::vector<Suggestion> footer_children = CreateFooterSuggestions(entry);

  // Add a separator only when there are both secondary suggestions above and
  // footer links below so they do not visually blend together.
  if (!suggestion.children.empty() && !footer_children.empty()) {
    Suggestion separator(SuggestionType::kSeparator);
    separator.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
    suggestion.children.emplace_back(std::move(separator));
  }
  base::Extend(suggestion.children, std::move(footer_children));

  return suggestion;
}

// Creates a suggestion to display when the query is supported, but yields no
// results.
Suggestion CreateNoDataSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_NO_DATA),
      SuggestionType::kAtMemorySearchResult);
  suggestion.acceptability =
      Suggestion::Acceptability::kSelectableButUnacceptable;
  suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  suggestion.icon = Suggestion::Icon::kSadTab;
  return suggestion;
}

// Creates a suggestion to display when AtMemory search fails to connect to the
// server.
Suggestion CreateNoConnectionSuggestion(std::u16string query) {
  Suggestion suggestion(std::move(query),
                        SuggestionType::kAtMemoryNoConnection);
  suggestion.labels = {{Suggestion::Text(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_NO_CONNECTION))}};
  suggestion.acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;
  suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  suggestion.icon = Suggestion::Icon::kSadTab;
  return suggestion;
}

// Creates a catch-all suggestion to display when AtMemory search fails due to
// an unexpected or generic error.
Suggestion CreateGenericErrorSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_GENERIC_ERROR),
      SuggestionType::kAtMemoryGenericError);
  suggestion.acceptability =
      Suggestion::Acceptability::kSelectableButUnacceptable;
  suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  suggestion.icon = Suggestion::Icon::kSadTab;
  return suggestion;
}

std::optional<std::u16string> GetAttributeFillValue(
    const EntityInstance& entity,
    const AttributeType& attribute_type,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    BrowserAutofillManager& manager) {
  base::optional_ref<const AttributeInstance> attribute =
      entity.attribute(attribute_type);
  if (!attribute) {
    return std::nullopt;
  }

  auto [form, autofill_field] = manager.FindFormAndField(form_id, field_id);
  const std::string app_locale = manager.client().GetAppLocale();
  // Using `GetFillingValueAndTypeForEntity` is preferred when the field is
  // known because it handles field-specific requirements such as state/country
  // normalization, dates, and truncation for length limits.
  if (autofill_field) {
    std::vector<AutofillFieldWithAttributeType> fields_and_types = {
        AutofillFieldWithAttributeType(*autofill_field, attribute_type)};
    FillingValueAndType value_and_type = GetFillingValueAndTypeForEntity(
        entity, fields_and_types, *autofill_field,
        mojom::ActionPersistence::kFill, app_locale,
        manager.client().GetAddressNormalizer());
    return value_and_type.value;
  }
  return attribute->GetCompleteInfo(app_locale);
}

// Extracts `EntryMetadata` items stored in `suggestion.children` to provide
// contextual metadata when unmasking/fetching sensitive PII data.
std::vector<EntryMetadata> GetMetadataFromSuggestion(
    const Suggestion& suggestion) {
  std::vector<EntryMetadata> metadata;
  for (const Suggestion& child : suggestion.children) {
    const auto* child_payload =
        std::get_if<Suggestion::AtMemoryPayload>(&child.payload);
    if (child.type != SuggestionType::kAtMemorySearchResult || !child_payload) {
      continue;
    }
    std::u16string label_text =
        !child.labels.empty() && !child.labels[0].empty()
            ? child.labels[0][0].value
            : std::u16string();
    metadata.emplace_back(child_payload->memory_data_type,
                          std::move(label_text), child_payload->value);
  }
  return metadata;
}

bool ShouldEraseMemorySearchResult(MemoryDataType type,
                                   base::span<const MemoryEntrySource> sources,
                                   AutofillClient& client,
                                   bool is_context_secure) {
  std::optional<AtMemoryAction> action =
      ToAtMemoryRetrieveForFillingAction(type);
  if (!action) {
    return false;
  }
  return !MayPerformAtMemoryAction(
      *action, client,
      /*url=*/std::nullopt,
      RetrieveForFillingParams{.is_spii = IsSpiiMemoryDataType(type),
                               .sources = sources,
                               .is_context_secure = is_context_secure});
}

}  // namespace

AtMemoryManager::AtMemoryManager(BrowserAutofillManager* manager)
    : owner_(manager) {}

AtMemoryManager::~AtMemoryManager() = default;

void AtMemoryManager::OnPopupShown(
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    AutofillSuggestionTriggerSource trigger_source,
    base::optional_ref<const AutofillSuggestionDelegate::SuggestionMetadata>
        parent_suggestion_metadata,
    bool is_context_secure,
    UpdateSuggestionsCallback update_callback,
    ukm::SourceId ukm_source_id) {
  if (!IsAtMemoryTriggerSource(trigger_source)) {
    return;
  }

  if (!parent_suggestion_metadata && !at_memory_metrics_recorder_) {
    const auto [form, field] = owner_->FindFormAndField(form_id, field_id);
    const FormSignature form_signature =
        form ? form->form_signature() : FormSignature(0);
    const FieldSignature field_signature =
        field ? field->GetFieldSignature() : FieldSignature(0);
    trigger_source_ = trigger_source;
    is_context_secure_ = is_context_secure;
    update_callback_ = std::move(update_callback);
    at_memory_metrics_recorder_ = std::make_unique<AtMemoryMetricsRecorder>(
        owner_->client().GetMqlsUploadService(),
        owner_->client().GetUkmRecorder(), ukm_source_id,
        owner_->client().GetLastCommittedPrimaryMainFrameURL(),
        owner_->client().GetPageTitle(), field_id, form_signature,
        field_signature);
  }

  if (at_memory_metrics_recorder_) {
    at_memory_metrics_recorder_->OnPopupShown(trigger_source,
                                              parent_suggestion_metadata);
  }
}

bool AtMemoryManager::OnFilterChanged(const std::u16string& filter) {
  if (!IsAtMemoryTriggerSource(trigger_source_)) {
    return false;
  }
  if (filter.empty()) {
    CancelPendingQueries();
    ClearSuggestions();
    return true;
  }
  std::vector<Suggestion> suggestions;
  if (net::NetworkChangeNotifier::IsOffline()) {
    suggestions.push_back(CreateNoConnectionSuggestion(filter));
  } else {
    suggestions.push_back(CreateSearchAffordanceSuggestion(filter));
  }

  if (!owner_->client().ShouldShowPersonalContextAtMemoryNotice()) {
    suggestions.emplace_back(SuggestionType::kSeparator);
    suggestions.back().filtration_policy =
        Suggestion::FiltrationPolicy::kStatic;
    suggestions.push_back(CreateAiDisclosureSuggestion());
  }

  SendSuggestions(std::move(suggestions));
  return true;
}

bool AtMemoryManager::OnSearchSubmitted(const std::u16string& filter) {
  if (!IsAtMemoryTriggerSource(trigger_source_)) {
    return false;
  }
  if (at_memory_metrics_recorder_) {
    at_memory_metrics_recorder_->OnQuerySubmitted(filter);
  }
  ExecuteQuery(filter);
  return true;
}

void AtMemoryManager::OnPopupHidden() {
  trigger_source_ = AutofillSuggestionTriggerSource::kUnspecified;
  update_callback_.Reset();
  if (at_memory_metrics_recorder_) {
    at_memory_metrics_recorder_.reset();
  }
  CancelPendingQueries();
  is_context_secure_ = false;
}

IsAsync AtMemoryManager::FillOrPreviewSearchResult(
    mojom::ActionPersistence action_persistence,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    base::optional_ref<const AutofillSuggestionDelegate::SuggestionMetadata>
        metadata) {
  const Suggestion::AtMemoryPayload& payload =
      suggestion.GetPayload<Suggestion::AtMemoryPayload>();

  switch (action_persistence) {
    case mojom::ActionPersistence::kPreview:
      owner_->FillOrPreviewField(
          action_persistence, mojom::FieldActionType::kReplaceAtMemoryTrigger,
          form_id, field_id,
          MaybeObfuscateValue(payload.value, payload.memory_data_type,
                              payload.is_personal_context_sourced),
          FillingProduct::kAtMemory,
          /*field_type_used=*/std::nullopt);
      return IsAsync(false);
    case mojom::ActionPersistence::kFill: {
      return FillSearchResult(form_id, field_id, suggestion, metadata);
    }
  }
}

IsAsync AtMemoryManager::FillSearchResult(
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    base::optional_ref<const AutofillSuggestionDelegate::SuggestionMetadata>
        metadata) {
  const Suggestion::AtMemoryPayload& payload =
      suggestion.GetPayload<Suggestion::AtMemoryPayload>();
  if (at_memory_metrics_recorder_) {
    at_memory_metrics_recorder_->OnSuggestionAccepted(
        payload.memory_data_type, payload.sources_bitmask, metadata);
  }
  // Transfer ownership of the metrics session to the filling path.
  // Ensures that the metrics will be properly recorded once the suggestion
  // is filled or one of the async steps in between fails.
  std::unique_ptr<AtMemoryMetricsRecorder> metrics =
      std::move(at_memory_metrics_recorder_);
  switch (payload.memory_data_type) {
    case MemoryDataType::kIban: {
      IsAsync is_async(false);
      std::visit(
          absl::Overload{[&](const Iban::Guid& guid) {
                           is_async = FillIban(guid, form_id, field_id,
                                               suggestion, std::move(metrics));
                         },
                         [&](const Iban::InstrumentId& instrument_id) {
                           is_async = FillIban(instrument_id, form_id, field_id,
                                               suggestion, std::move(metrics));
                         },
                         [](std::monostate) { NOTREACHED(); },
                         [](const std::string&) { NOTREACHED(); },
                         [](const EntityInstance::EntityId&) { NOTREACHED(); }},
          payload.identifier);
      return is_async;
    }
    case MemoryDataType::kCreditCardNumber:
    case MemoryDataType::kCreditCardSecurityCode: {
      CHECK(std::holds_alternative<std::string>(payload.identifier));
      FillCreditCard(std::get<std::string>(payload.identifier), form_id,
                     field_id, suggestion, std::move(metrics));
      // TODO(crbug.com/531988037): Implement spinning loader logic for credit
      // cards.
      return IsAsync(false);
    }
    case MemoryDataType::kPassportNumber:
    case MemoryDataType::kDriversLicenseNumber:
    case MemoryDataType::kNationalIdCardNumber:
    case MemoryDataType::kKnownTravelerNumberNumber:
    case MemoryDataType::kRedressNumberNumber: {
      return FillSensitiveAutofillAiOrPersonalContextData(
          form_id, field_id, suggestion, std::move(metrics));
    }

    case MemoryDataType::kNameFull:
    case MemoryDataType::kAddressFull:
    case MemoryDataType::kAddressStreetAddress:
    case MemoryDataType::kAddressCity:
    case MemoryDataType::kAddressState:
    case MemoryDataType::kAddressZip:
    case MemoryDataType::kAddressCountry:
    case MemoryDataType::kPhone:
    case MemoryDataType::kEmail:
    case MemoryDataType::kCompanyName: {
      RecordAddressProfileUse(payload.identifier);
      if (metrics) {
        metrics->MarkFilled();
      }
      owner_->FillOrPreviewField(
          mojom::ActionPersistence::kFill,
          mojom::FieldActionType::kReplaceAtMemoryTrigger, form_id, field_id,
          payload.value, FillingProduct::kAtMemory,
          /*field_type_used=*/std::nullopt);
      return IsAsync(false);
    }

    case MemoryDataType::kCreditCardExpirationDate:
    case MemoryDataType::kCreditCardNameOnCard: {
      RecordCreditCardUse(payload.identifier);
      if (metrics) {
        metrics->MarkFilled();
      }
      owner_->FillOrPreviewField(
          mojom::ActionPersistence::kFill,
          mojom::FieldActionType::kReplaceAtMemoryTrigger, form_id, field_id,
          payload.value, FillingProduct::kAtMemory,
          /*field_type_used=*/std::nullopt);
      return IsAsync(false);
    }

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
    case MemoryDataType::kOrderGrandTotal: {
      RecordAutofillAiEntityUse(payload.identifier);
      if (metrics) {
        metrics->MarkFilled();
      }
      owner_->FillOrPreviewField(
          mojom::ActionPersistence::kFill,
          mojom::FieldActionType::kReplaceAtMemoryTrigger, form_id, field_id,
          payload.value, FillingProduct::kAtMemory,
          /*field_type_used=*/std::nullopt);
      return IsAsync(false);
    }

    case MemoryDataType::kCreditCardNickname:
    case MemoryDataType::kIbanNickname:
    case MemoryDataType::kUnknown: {
      if (metrics) {
        metrics->MarkFilled();
      }
      owner_->FillOrPreviewField(
          mojom::ActionPersistence::kFill,
          mojom::FieldActionType::kReplaceAtMemoryTrigger, form_id, field_id,
          payload.value, FillingProduct::kAtMemory,
          /*field_type_used=*/std::nullopt);
      return IsAsync(false);
    }
  }
  NOTREACHED();
}

void AtMemoryManager::RecordAddressProfileUse(
    const Suggestion::AtMemoryPayload::Identifier& identifier) {
  const std::string* guid = std::get_if<std::string>(&identifier);
  if (!guid || guid->empty()) {
    return;
  }

  AddressDataManager& adm =
      owner_->client().GetPersonalDataManager().address_data_manager();
  if (const AutofillProfile* profile = adm.GetProfileByGUID(*guid)) {
    adm.RecordUseOf(*profile);
  }
}

void AtMemoryManager::RecordCreditCardUse(
    const Suggestion::AtMemoryPayload::Identifier& identifier) {
  const std::string* guid = std::get_if<std::string>(&identifier);
  if (!guid || guid->empty()) {
    return;
  }

  PaymentsDataManager& pdm =
      owner_->client().GetPersonalDataManager().payments_data_manager();
  if (const CreditCard* credit_card = pdm.GetCreditCardByGUID(*guid)) {
    pdm.RecordUseOfCard(*credit_card);
  }
}

void AtMemoryManager::RecordAutofillAiEntityUse(
    const Suggestion::AtMemoryPayload::Identifier& identifier) {
  if (EntityDataManager* edm = owner_->client().GetEntityDataManager()) {
    if (const EntityInstance::EntityId* entity_id =
            std::get_if<EntityInstance::EntityId>(&identifier)) {
      if (!entity_id->value().empty()) {
        edm->RecordEntityUsed(*entity_id, base::Time::Now());
      }
    }
  }
}

bool AtMemoryManager::IsSearching() const {
  return is_searching_;
}

void AtMemoryManager::MaybeAppendPersonalContextNotice(
    std::vector<Suggestion>& suggestions) const {
  if (!owner_->client().ShouldShowPersonalContextAtMemoryNotice()) {
    return;
  }
  if (std::ranges::contains(suggestions, SuggestionType::kPersonalContextNotice,
                            &Suggestion::type)) {
    return;
  }
  // Before search results are returned (when only the search affordance to
  // start the query is present), place the search affordance first and append
  // the notice card at the end. After actual search results are returned, place
  // the notice card first on top of the search results.
  Suggestion notice(SuggestionType::kPersonalContextNotice);
  notice.filtration_policy = Suggestion::FiltrationPolicy::kStatic;

  if (suggestions.size() == 1u &&
      (suggestions[0].type == SuggestionType::kAtMemorySearchAffordance ||
       suggestions[0].type == SuggestionType::kAtMemoryFetching)) {
    suggestions.emplace_back(SuggestionType::kSeparator);
    suggestions.back().filtration_policy =
        Suggestion::FiltrationPolicy::kStatic;
    suggestions.push_back(std::move(notice));
    return;
  }

  // This handles both empty vectors and vectors containing search results.
  suggestions.insert(suggestions.begin(), std::move(notice));
}

void AtMemoryManager::ExecuteQuery(const std::u16string& filter) {
  AtMemoryQueryService* query_service =
      owner_->client().GetAtMemoryQueryService();
  if (!query_service || !IsAtMemoryTriggerSource(trigger_source_) ||
      !update_callback_) {
    return;
  }

  // Cancel stale updates from previous queries.
  // At any point in time, there can be only one pending query.
  CancelPendingQueries();

  if (filter.empty()) {
    ClearSuggestions();
    return;
  }

  is_searching_ = true;
  // Notify the UI that search has started.
  ShowFetchingSuggestion();
  query_service->Query(
      filter, owner_->client().GetLastCommittedPrimaryMainFrameURL(),
      owner_->client().GetPageTitle(),
      base::BindRepeating(&AtMemoryManager::OnSearchResultsReceived,
                          query_weak_ptr_factory_.GetWeakPtr(), filter));
}

// Creates a suggestion to offer to open Gemini in the sidebar when the query is
// unsupported.
Suggestion AtMemoryManager::CreateUnsupportedQuerySuggestion(
    const std::u16string& query) {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_UNSUPPORTED_QUERY_TITLE),
      SuggestionType::kOpenGemini);
  suggestion.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_AT_MEMORY_UNSUPPORTED_QUERY_DESCRIPTION))}};
  suggestion.acceptability =
      Suggestion::Acceptability::kSelectableAndAcceptable;
  suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  suggestion.payload = Suggestion::OpenGeminiPayload(query);
  suggestion.icon = Suggestion::Icon::kSpark;
  return suggestion;
}

Suggestion AtMemoryManager::CreateSearchAffordanceSuggestion(
    std::u16string query) {
  Suggestion affordance(std::move(query),
                        SuggestionType::kAtMemorySearchAffordance);
  affordance.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_AT_MEMORY_SEARCH_AFFORDANCE_SUBTITLE))}};
  affordance.icon = Suggestion::Icon::kSpark;
  return affordance;
}

Suggestion AtMemoryManager::CreateAiDisclosureSuggestion() const {
  Suggestion suggestion(SuggestionType::kAtMemoryAiDisclosure);
  suggestion.acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;
  suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  return suggestion;
}

void AtMemoryManager::CancelPendingQueries() {
  query_weak_ptr_factory_.InvalidateWeakPtrs();
  is_searching_ = false;
}

void AtMemoryManager::SendSuggestions(std::vector<Suggestion> suggestions) {
  MaybeAppendPersonalContextNotice(suggestions);
  if (update_callback_) {
    update_callback_.Run(std::move(suggestions), trigger_source_);
  }
}

void AtMemoryManager::ShowFetchingSuggestion() {
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(CreateFetchingSuggestion());
  SendSuggestions(std::move(suggestions));
}

void AtMemoryManager::ClearSuggestions() {
  SendSuggestions({});
}

void AtMemoryManager::OnSearchResultsReceived(const std::u16string& query,
                                              MemorySearchResults result) {
  if (!IsAtMemoryTriggerSource(trigger_source_) || !update_callback_ ||
      !is_searching_) {
    return;
  }

  bool expecting_more_data =
      result.status == MemorySearchStatus::kPartialResponseSuccess;
  if (!expecting_more_data) {
    CancelPendingQueries();
  }

  if (at_memory_metrics_recorder_) {
    at_memory_metrics_recorder_->OnQueryResponseReceived(result);
  }

  if (!result.entries.empty()) {
    std::erase_if(result.entries, [this](const MemorySearchResult& entry) {
      return ShouldEraseMemorySearchResult(
          entry.type, entry.sources, owner_->client(), is_context_secure_);
    });
    for (MemorySearchResult& entry : result.entries) {
      std::erase_if(entry.metadata_list, [this, &entry](
                                             const EntryMetadata& metadata) {
        return ShouldEraseMemorySearchResult(
            metadata.type, entry.sources, owner_->client(), is_context_secure_);
      });
    }

    if (!result.entries.empty()) {
      // If there are remaining results after filtering, return them.
      SendSuggestions(
          base::ToVector(result.entries, TransformResultIntoSuggestion));
      return;
    }
  }

  // When search returns no entries, show the appropriate special
  // suggestion based on the status.
  std::vector<Suggestion> suggestions;
  switch (result.status) {
    case MemorySearchStatus::kUnsupportedQuery:
      if (owner_->client().IsGlicEnabled()) {
        suggestions.push_back(CreateUnsupportedQuerySuggestion(query));
      } else {
        suggestions.push_back(CreateNoDataSuggestion());
      }
      break;
    case MemorySearchStatus::kFinalResponseSuccess:
      suggestions.push_back(CreateNoDataSuggestion());
      break;
    case MemorySearchStatus::kPartialResponseSuccess:
      break;
    case MemorySearchStatus::kNoConnectionFailure:
      suggestions.push_back(CreateNoConnectionSuggestion(query));
      break;
    case MemorySearchStatus::kInferenceFailure:
    case MemorySearchStatus::kInternalFailure:
      suggestions.push_back(CreateGenericErrorSuggestion());
      break;
  }
  SendSuggestions(std::move(suggestions));
}

IsAsync AtMemoryManager::FillIban(
    const std::variant<Iban::Guid, Iban::InstrumentId>& identifier,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    std::unique_ptr<AtMemoryMetricsRecorder> metrics) {
  Suggestion::Payload iban_payload;
  if (const Iban::Guid* guid = std::get_if<Iban::Guid>(&identifier)) {
    iban_payload = Suggestion::Guid(guid->value());
  } else {
    iban_payload = Suggestion::InstrumentId(
        std::get<Iban::InstrumentId>(identifier).value());
  }

  IbanAccessManager* iban_access_manager =
      owner_->client().GetPaymentsAutofillClient()->GetIbanAccessManager();
  if (!iban_access_manager) {
    return IsAsync(false);
  }

  if (metrics) {
    metrics->OnFetchPiiStarted(AtMemoryMetricsRecorder::FetchPiiSource::kIban);
  }

  return iban_access_manager->FetchValue(
      iban_payload,
      base::BindOnce(
          [](base::WeakPtr<AtMemoryManager> manager,
             const FormGlobalId& form_id, const FieldGlobalId& field_id,
             const Suggestion& suggestion,
             std::unique_ptr<AtMemoryMetricsRecorder> metrics,
             std::variant<Iban::Guid, Iban::InstrumentId> identifier,
             base::expected<std::u16string, IbanAccessManager::FailureReason>
                 unmasked_value) {
            if (!manager) {
              return;
            }
            manager->owner_->client().HideSuggestions(
                SuggestionHidingReason::kAcceptSuggestion,
                FillingProduct::kAtMemory);
            if (!unmasked_value.has_value()) {
              return;
            }
            if (metrics) {
              metrics->OnFetchPiiCompleted();
              metrics->MarkFilled();
            }
            PaymentsDataManager& pdm = manager->owner_->client()
                                           .GetPersonalDataManager()
                                           .payments_data_manager();
            if (const Iban* iban = std::visit(
                    absl::Overload{
                        [&pdm](const Iban::Guid& guid) {
                          return pdm.GetIbanByGUID(guid.value());
                        },
                        [&pdm](const Iban::InstrumentId& instrument_id) {
                          return pdm.GetIbanByInstrumentId(
                              instrument_id.value());
                        },
                    },
                    identifier)) {
              Iban mutable_iban = *iban;
              pdm.RecordUseOfIban(mutable_iban);
            }
            manager->owner_->FillOrPreviewField(
                mojom::ActionPersistence::kFill,
                mojom::FieldActionType::kReplaceAtMemoryTrigger, form_id,
                field_id, *unmasked_value, FillingProduct::kAtMemory,
                /*field_type_used=*/std::nullopt);
          },
          fill_weak_ptr_factory_.GetWeakPtr(), form_id, field_id, suggestion,
          std::move(metrics), identifier));
}

void AtMemoryManager::FillCreditCard(
    const std::string& guid,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    std::unique_ptr<AtMemoryMetricsRecorder> metrics) {
  CreditCardAccessManager* credit_card_access_manager =
      owner_->GetCreditCardAccessManager();
  if (!credit_card_access_manager) {
    return;
  }

  const PersonalDataManager& pdm = owner_->client().GetPersonalDataManager();
  const CreditCard* credit_card =
      pdm.payments_data_manager().GetCreditCardByGUID(guid);
  if (!credit_card) {
    return;
  }

  if (metrics) {
    metrics->OnFetchPiiStarted(
        AtMemoryMetricsRecorder::FetchPiiSource::kCreditCard);
  }

  // TODO(crbug.com/497795513): Consider caching fetched cards.
  credit_card_access_manager->FetchCreditCard(
      credit_card,
      base::BindOnce(
          [](base::WeakPtr<AtMemoryManager> manager,
             const FormGlobalId& form_id, const FieldGlobalId& field_id,
             const Suggestion& suggestion,
             std::unique_ptr<AtMemoryMetricsRecorder> metrics,
             const CreditCard& fetched_card) {
            if (!manager) {
              return;
            }
            if (metrics) {
              metrics->OnFetchPiiCompleted();
              metrics->MarkFilled();
            }
            manager->owner_->client()
                .GetPersonalDataManager()
                .payments_data_manager()
                .RecordUseOfCard(fetched_card);
            const Suggestion::AtMemoryPayload& payload =
                suggestion.GetPayload<Suggestion::AtMemoryPayload>();
            std::u16string fill_value;
            switch (payload.memory_data_type) {
              case MemoryDataType::kCreditCardNumber:
                fill_value = fetched_card.number();
                break;
              case MemoryDataType::kCreditCardSecurityCode:
                fill_value = fetched_card.cvc();
                break;
              default:
                NOTREACHED();
            }

            manager->owner_->FillOrPreviewField(
                mojom::ActionPersistence::kFill,
                mojom::FieldActionType::kReplaceAtMemoryTrigger, form_id,
                field_id, fill_value, FillingProduct::kAtMemory,
                /*field_type_used=*/std::nullopt);
          },
          fill_weak_ptr_factory_.GetWeakPtr(), form_id, field_id, suggestion,
          std::move(metrics)));
}

IsAsync AtMemoryManager::FillSensitivePersonalContextData(
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    std::unique_ptr<AtMemoryMetricsRecorder> metrics) {
  AtMemoryQueryService* query_service =
      owner_->client().GetAtMemoryQueryService();

  if (!query_service) {
    return IsAsync(false);
  }

  const Suggestion::AtMemoryPayload& payload =
      suggestion.GetPayload<Suggestion::AtMemoryPayload>();

  if (metrics) {
    metrics->OnFetchPiiStarted(
        AtMemoryMetricsRecorder::FetchPiiSource::kPersonalContext);
  }

  query_service->AuthenticateAndFetchPiiEntity(
      owner_->client(),
      GetAuthenticationMessage(
          owner_->client().GetLastCommittedPrimaryMainFrameOrigin()),
      payload.value, payload.memory_data_type,
      GetMetadataFromSuggestion(suggestion),
      base::BindOnce(&AtMemoryManager::OnSensitivePersonalContextDataFetched,
                     fill_weak_ptr_factory_.GetWeakPtr(), form_id, field_id,
                     std::move(metrics)));
  return IsAsync(true);
}

void AtMemoryManager::OnSensitivePersonalContextDataFetched(
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    std::unique_ptr<AtMemoryMetricsRecorder> metrics,
    AtMemoryQueryService::SpiiRetrievalResult result) {
  owner_->client().HideSuggestions(SuggestionHidingReason::kAcceptSuggestion,
                                   FillingProduct::kAtMemory);

  if (!result.has_value()) {
    if (metrics) {
      metrics->OnFetchPersonalContextPiiDataFailed(result.error());
    }
    owner_->client().ShowAtMemoryFetchFailureNotification();
    return;
  }
  if (metrics) {
    metrics->OnFetchPiiCompleted();
    metrics->MarkFilled();
  }

  owner_->FillOrPreviewField(mojom::ActionPersistence::kFill,
                             mojom::FieldActionType::kReplaceAtMemoryTrigger,
                             form_id, field_id, *result,
                             FillingProduct::kAtMemory,
                             /*field_type_used=*/std::nullopt);
}

IsAsync AtMemoryManager::FillSensitiveAutofillAiOrPersonalContextData(
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    std::unique_ptr<AtMemoryMetricsRecorder> metrics) {
  const Suggestion::AtMemoryPayload& payload =
      suggestion.GetPayload<Suggestion::AtMemoryPayload>();

  std::optional<AtMemoryDataType> data_type =
      ToAtMemoryDataType(payload.memory_data_type);
  CHECK(data_type && std::holds_alternative<AttributeType>(*data_type));

  if (payload.is_personal_context_sourced) {
    return FillSensitivePersonalContextData(form_id, field_id, suggestion,
                                            std::move(metrics));
  } else if (const EntityInstance::EntityId* entity_id =
                 std::get_if<EntityInstance::EntityId>(&payload.identifier);
             entity_id) {
    return FillSensitiveAutofillAiData(*entity_id, form_id, field_id,
                                       suggestion, *data_type,
                                       std::move(metrics));
  }
  NOTREACHED();
}

IsAsync AtMemoryManager::FillSensitiveAutofillAiData(
    const EntityInstance::EntityId& entity_id,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    const AtMemoryDataType& data_type,
    std::unique_ptr<AtMemoryMetricsRecorder> metrics) {
  EntityDataManager* entity_data_manager =
      owner_->client().GetEntityDataManager();
  CHECK(entity_data_manager);

  base::optional_ref<const EntityInstance> entity =
      entity_data_manager->GetEntityInstance(entity_id);
  if (!entity) {
    return IsAsync(false);
  }

  if (metrics) {
    metrics->OnFetchPiiStarted(
        AtMemoryMetricsRecorder::FetchPiiSource::kAutofillAi);
  }

  return IsAsync(owner_->GetAutofillAiAccessManager().FetchEntityInstance(
      *entity, /*will_fill_sensitive_info=*/true,
      base::BindOnce(&AtMemoryManager::OnAutofillAiFetched,
                     fill_weak_ptr_factory_.GetWeakPtr(), form_id, field_id,
                     suggestion, data_type, std::move(metrics))));
}

void AtMemoryManager::OnAutofillAiFetched(
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    const AtMemoryDataType& data_type,
    std::unique_ptr<AtMemoryMetricsRecorder> metrics,
    base::expected<EntityInstance, AutofillAiAccessManager::FailureReason>
        result,
    bool reauth_attempted) {
  owner_->client().HideSuggestions(SuggestionHidingReason::kAcceptSuggestion,
                                   FillingProduct::kAtMemory);
  if (!result.has_value()) {
    if (result.error() ==
        AutofillAiAccessManager::FailureReason::kFetchFailed) {
      owner_->client().ShowAutofillAiFetchEntityFailureNotification();
    }
    return;
  }

  const EntityInstance& fetched_entity = result.value();

  CHECK(std::holds_alternative<AttributeType>(data_type));
  AttributeType target_attribute_type = std::get<AttributeType>(data_type);

  std::optional<std::u16string> attribute_fill_value = GetAttributeFillValue(
      fetched_entity, target_attribute_type, form_id, field_id, *owner_);
  if (!attribute_fill_value) {
    return;
  }

  if (metrics) {
    metrics->OnFetchPiiCompleted();
    metrics->MarkFilled();
  }

  if (EntityDataManager* edm = owner_->client().GetEntityDataManager()) {
    edm->RecordEntityUsed(fetched_entity.guid(), base::Time::Now());
  }

  owner_->FillOrPreviewField(
      mojom::ActionPersistence::kFill,
      mojom::FieldActionType::kReplaceAtMemoryTrigger, form_id, field_id,
      std::move(*attribute_fill_value), FillingProduct::kAtMemory,
      /*field_type_used=*/std::nullopt);
}

}  // namespace autofill
