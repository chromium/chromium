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
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/at_memory_query_service.h"
#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/at_memory/at_memory_funnel_metrics.h"
#include "components/autofill/core/browser/at_memory/at_memory_utils.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/autofill_ai/from_accessibility_annotator.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/autofill_ai/autofill_ai_access_manager.h"
#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

using MemoryDataType = accessibility_annotator::MemoryDataType;

SuggestionType GetManageSuggestionType(MemoryDataType type) {
  std::optional<AtMemoryDataType> data_type = ToAtMemoryDataType(type);
  if (data_type) {
    if (const auto* field_type = std::get_if<FieldType>(&*data_type)) {
      if (*field_type == IBAN_VALUE) {
        return SuggestionType::kManageIban;
      }
      return SuggestionType::kManageAddress;
    }
  }
  return SuggestionType::kManageAutofillAi;
}

std::u16string GetSourceDescriptionText(
    accessibility_annotator::MemoryEntrySourceType type) {
  int source_string_id = [type]() {
    switch (type) {
      case accessibility_annotator::MemoryEntrySourceType::kGmail:
        return IDS_AUTOFILL_AT_MEMORY_SOURCE_GMAIL;
      case accessibility_annotator::MemoryEntrySourceType::kCalendar:
        return IDS_AUTOFILL_AT_MEMORY_SOURCE_CALENDAR;
      case accessibility_annotator::MemoryEntrySourceType::kPhotos:
        return IDS_AUTOFILL_AT_MEMORY_SOURCE_PHOTOS;
      case accessibility_annotator::MemoryEntrySourceType::kAmbient:
        return IDS_AUTOFILL_AT_MEMORY_SOURCE_AMBIENT;
      case accessibility_annotator::MemoryEntrySourceType::kLiveTabs:
        return IDS_AUTOFILL_AT_MEMORY_SOURCE_LIVETABS;
      case accessibility_annotator::MemoryEntrySourceType::kAutofill:
        break;
    }
    NOTREACHED();
  }();
  return l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_AT_MEMORY_SOURCE_ATTRIBUTION_DESCRIPTION,
      l10n_util::GetStringUTF16(source_string_id));
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
    case MemoryDataType::kCreditCardNumber:
    case MemoryDataType::kCreditCardSecurityCode: {
      CHECK(std::holds_alternative<std::string>(identifier));
      return *std::get_if<std::string>(&identifier);
    }
    case MemoryDataType::kPassportFull:
    case MemoryDataType::kDriversLicenseFull:
    case MemoryDataType::kNationalIdCardFull:
    case MemoryDataType::kKnownTravelerNumberFull:
    case MemoryDataType::kRedressNumberFull:
    case MemoryDataType::kPassportNumber:
    case MemoryDataType::kDriversLicenseNumber:
    case MemoryDataType::kNationalIdCardNumber:
    case MemoryDataType::kKnownTravelerNumberNumber:
    case MemoryDataType::kRedressNumberNumber: {
      CHECK(std::holds_alternative<std::string>(identifier));
      return EntityInstance::EntityId(*std::get_if<std::string>(&identifier));
    }
    default:
      return std::monostate();
  }
}

Suggestion::Icon GetIconForMemoryDataType(MemoryDataType type) {
  switch (type) {
    case MemoryDataType::kNameFull:
    case MemoryDataType::kAddressFull:
    case MemoryDataType::kAddressStreetAddress:
    case MemoryDataType::kAddressCity:
    case MemoryDataType::kAddressState:
    case MemoryDataType::kAddressZip:
    case MemoryDataType::kAddressCountry:
    case MemoryDataType::kPhone:
    case MemoryDataType::kCompanyName:
      return Suggestion::Icon::kAccount;
    case MemoryDataType::kEmail:
      return Suggestion::Icon::kEmail;
    case MemoryDataType::kIban:
    case MemoryDataType::kIbanNickname:
      return Suggestion::Icon::kIban;
    case MemoryDataType::kVehicle:
    case MemoryDataType::kVehicleMake:
    case MemoryDataType::kVehicleModel:
    case MemoryDataType::kVehicleYear:
    case MemoryDataType::kVehicleOwner:
    case MemoryDataType::kVehiclePlateNumber:
    case MemoryDataType::kVehiclePlateState:
    case MemoryDataType::kVehicleVin:
      return Suggestion::Icon::kVehicle;
    case MemoryDataType::kPassportFull:
    case MemoryDataType::kPassportName:
    case MemoryDataType::kPassportCountry:
    case MemoryDataType::kPassportNumber:
    case MemoryDataType::kPassportIssueDate:
    case MemoryDataType::kPassportExpirationDate:
      return Suggestion::Icon::kPassport;
    case MemoryDataType::kFlightReservationFull:
    case MemoryDataType::kFlightReservationFlightNumber:
    case MemoryDataType::kFlightReservationTicketNumber:
    case MemoryDataType::kFlightReservationConfirmationCode:
    case MemoryDataType::kFlightReservationPassengerName:
    case MemoryDataType::kFlightReservationDepartureAirport:
    case MemoryDataType::kFlightReservationArrivalAirport:
    case MemoryDataType::kFlightReservationDepartureDate:
    case MemoryDataType::kFlightReservationArrivalDate:
      return Suggestion::Icon::kFlight;
    case MemoryDataType::kNationalIdCardFull:
    case MemoryDataType::kNationalIdCardName:
    case MemoryDataType::kNationalIdCardCountry:
    case MemoryDataType::kNationalIdCardNumber:
    case MemoryDataType::kNationalIdCardIssueDate:
    case MemoryDataType::kNationalIdCardExpirationDate:
    case MemoryDataType::kDriversLicenseFull:
    case MemoryDataType::kDriversLicenseName:
    case MemoryDataType::kDriversLicenseState:
    case MemoryDataType::kDriversLicenseNumber:
    case MemoryDataType::kDriversLicenseIssueDate:
    case MemoryDataType::kDriversLicenseExpirationDate:
      return Suggestion::Icon::kIdCard;
    case MemoryDataType::kRedressNumberFull:
    case MemoryDataType::kRedressNumberName:
    case MemoryDataType::kRedressNumberNumber:
    case MemoryDataType::kKnownTravelerNumberFull:
    case MemoryDataType::kKnownTravelerNumberName:
    case MemoryDataType::kKnownTravelerNumberNumber:
    case MemoryDataType::kKnownTravelerNumberExpirationDate:
      return Suggestion::Icon::kPersonCheck;
    case MemoryDataType::kCreditCardNumber:
    case MemoryDataType::kCreditCardExpirationDate:
    case MemoryDataType::kCreditCardSecurityCode:
    case MemoryDataType::kCreditCardNameOnCard:
    case MemoryDataType::kCreditCardNickname:
      return Suggestion::Icon::kCardGeneric;
    case MemoryDataType::kOrderFull:
    case MemoryDataType::kOrderId:
    case MemoryDataType::kOrderAccount:
    case MemoryDataType::kOrderDate:
    case MemoryDataType::kOrderMerchantName:
    case MemoryDataType::kOrderMerchantDomain:
    case MemoryDataType::kOrderProductNames:
    case MemoryDataType::kOrderGrandTotal:
      return Suggestion::Icon::kOrder;
    case MemoryDataType::kShipmentFull:
    case MemoryDataType::kShipmentTrackingNumber:
    case MemoryDataType::kShipmentAssociatedOrderId:
    case MemoryDataType::kShipmentDeliveryAddress:
    case MemoryDataType::kShipmentDeliveryZipCode:
    case MemoryDataType::kShipmentCarrierName:
    case MemoryDataType::kShipmentCarrierDomain:
    case MemoryDataType::kShipmentEstimatedDeliveryDate:
    case MemoryDataType::kShipmentShippedDate:
    case MemoryDataType::kUnknown:
      return Suggestion::Icon::kNoIcon;
  }
  return Suggestion::Icon::kNoIcon;
}

Suggestion TransformResultIntoSuggestion(
    const accessibility_annotator::MemorySearchResult& entry) {
  Suggestion suggestion(entry.value, SuggestionType::kAtMemorySearchResult);
  suggestion.icon = GetIconForMemoryDataType(entry.type);
  if (suggestion.icon == Suggestion::Icon::kNoIcon && !entry.sources.empty()) {
    switch (entry.sources.front().type) {
      case accessibility_annotator::MemoryEntrySourceType::kGmail:
        suggestion.icon = Suggestion::Icon::kGmail;
        break;
      case accessibility_annotator::MemoryEntrySourceType::kPhotos:
        suggestion.icon = Suggestion::Icon::kGooglePhotos;
        break;
      case accessibility_annotator::MemoryEntrySourceType::kCalendar:
        suggestion.icon = Suggestion::Icon::kGoogleCalendar;
        break;
      case accessibility_annotator::MemoryEntrySourceType::kAmbient:
      case accessibility_annotator::MemoryEntrySourceType::kLiveTabs:
      case accessibility_annotator::MemoryEntrySourceType::kAutofill:
        break;
    }
  }
  // Label row: [type_name, metadata[0].value, ...]
  std::vector<Suggestion::Text> label_row;
  std::u16string type_name = entry.type_name.empty()
                                 ? GetMemoryDataTypeNameForI18n(entry.type)
                                 : entry.type_name;
  if (!type_name.empty()) {
    label_row.emplace_back(std::move(type_name));
  }
  for (const accessibility_annotator::EntryMetadata& metadata :
       entry.metadata_list) {
    if (!label_row.empty()) {
      label_row.emplace_back(u"\u2022");  // Bullet (•)
    }
    label_row.emplace_back(metadata.value);
  }
  if (!label_row.empty()) {
    suggestion.labels.emplace_back(std::move(label_row));
  }
  Suggestion::AtMemoryPayload at_memory_payload(entry.value, entry.type);
  at_memory_payload.identifier =
      GetPayloadIdentifier(entry.type, entry.identifier);
  suggestion.payload = std::move(at_memory_payload);
  suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;

  // Metadata are displayed as nested results in the flyout menu.
  for (const accessibility_annotator::EntryMetadata& metadata :
       entry.metadata_list) {
    Suggestion child =
        Suggestion(metadata.value, SuggestionType::kAtMemorySearchResult);
    std::u16string child_type_name =
        metadata.type_name.empty() ? GetMemoryDataTypeNameForI18n(metadata.type)
                                   : metadata.type_name;
    if (!child_type_name.empty()) {
      child.labels = {{Suggestion::Text(child_type_name)}};
    }
    Suggestion::AtMemoryPayload child_at_memory_payload(metadata.value,
                                                        metadata.type);
    child_at_memory_payload.memory_data_type = metadata.type;
    child_at_memory_payload.identifier =
        GetPayloadIdentifier(metadata.type, entry.identifier);
    child.payload = std::move(child_at_memory_payload);
    suggestion.children.push_back(std::move(child));
  }

  const accessibility_annotator::MemoryEntrySource* source =
      entry.sources.empty() ? nullptr : &entry.sources.front();
  if (source) {
    if (!suggestion.children.empty()) {
      Suggestion source_child(SuggestionType::kSeparator);
      source_child.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
      suggestion.children.push_back(std::move(source_child));
    }

    switch (source->type) {
      case accessibility_annotator::MemoryEntrySourceType::kGmail:
      case accessibility_annotator::MemoryEntrySourceType::kCalendar:
      case accessibility_annotator::MemoryEntrySourceType::kPhotos:
      case accessibility_annotator::MemoryEntrySourceType::kAmbient:
      case accessibility_annotator::MemoryEntrySourceType::kLiveTabs: {
        Suggestion source_info(
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_AT_MEMORY_SOURCE_ATTRIBUTION_TITLE),
            SuggestionType::kAtMemorySearchResult);
        source_info.labels = {
            {Suggestion::Text(GetSourceDescriptionText(source->type))}};
        source_info.acceptability = Suggestion::Acceptability::kUnacceptable;
        source_info.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
        suggestion.children.push_back(std::move(source_info));
        break;
      }
      case accessibility_annotator::MemoryEntrySourceType::kAutofill: {
        Suggestion manage_information(
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_AI_MANAGE_SUGGESTION_MAIN_TEXT),
            GetManageSuggestionType(entry.type));
        manage_information.icon = Suggestion::Icon::kSettings;
        manage_information.filtration_policy =
            Suggestion::FiltrationPolicy::kStatic;
        suggestion.children.push_back(std::move(manage_information));
        break;
      }
    }
  }

  return suggestion;
}

bool IsSpiiMemoryDataType(MemoryDataType type) {
  switch (type) {
    case MemoryDataType::kIban:
    case MemoryDataType::kCreditCardNumber:
    case MemoryDataType::kCreditCardSecurityCode:
    case MemoryDataType::kPassportNumber:
    case MemoryDataType::kPassportFull:
    case MemoryDataType::kNationalIdCardNumber:
    case MemoryDataType::kNationalIdCardFull:
    case MemoryDataType::kDriversLicenseNumber:
    case MemoryDataType::kDriversLicenseFull:
    case MemoryDataType::kRedressNumberNumber:
    case MemoryDataType::kRedressNumberFull:
    case MemoryDataType::kKnownTravelerNumberFull:
    case MemoryDataType::kKnownTravelerNumberNumber:
      return true;
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
    case MemoryDataType::kIbanNickname:
    case MemoryDataType::kVehicle:
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
    case MemoryDataType::kFlightReservationFull:
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
    case MemoryDataType::kNationalIdCardIssueDate:
    case MemoryDataType::kNationalIdCardExpirationDate:
    case MemoryDataType::kDriversLicenseName:
    case MemoryDataType::kDriversLicenseState:
    case MemoryDataType::kDriversLicenseIssueDate:
    case MemoryDataType::kDriversLicenseExpirationDate:
    case MemoryDataType::kRedressNumberName:
    case MemoryDataType::kKnownTravelerNumberName:
    case MemoryDataType::kKnownTravelerNumberExpirationDate:
    case MemoryDataType::kCreditCardExpirationDate:
    case MemoryDataType::kCreditCardNameOnCard:
    case MemoryDataType::kCreditCardNickname:
    case MemoryDataType::kOrderFull:
    case MemoryDataType::kOrderId:
    case MemoryDataType::kOrderAccount:
    case MemoryDataType::kOrderDate:
    case MemoryDataType::kOrderMerchantName:
    case MemoryDataType::kOrderMerchantDomain:
    case MemoryDataType::kOrderProductNames:
    case MemoryDataType::kOrderGrandTotal:
    case MemoryDataType::kShipmentFull:
    case MemoryDataType::kShipmentTrackingNumber:
    case MemoryDataType::kShipmentAssociatedOrderId:
    case MemoryDataType::kShipmentDeliveryAddress:
    case MemoryDataType::kShipmentDeliveryZipCode:
    case MemoryDataType::kShipmentCarrierName:
    case MemoryDataType::kShipmentCarrierDomain:
    case MemoryDataType::kShipmentEstimatedDeliveryDate:
    case MemoryDataType::kShipmentShippedDate:
    case MemoryDataType::kUnknown:
      return false;
  }
}

// Creates a suggestion to display when the query is supported, but yields no
// results.
Suggestion CreateNoDataSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_NO_DATA),
      SuggestionType::kAtMemorySearchResult);
  suggestion.acceptability =
      Suggestion::Acceptability::kUnacceptable;
  suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  suggestion.icon = Suggestion::Icon::kSadTab;
  return suggestion;
}

// Creates a suggestion to display when AtMemory search fails to connect to the
// server.
Suggestion CreateNoConnectionSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_NO_CONNECTION),
      SuggestionType::kAtMemoryNoConnection);
  suggestion.acceptability =
      Suggestion::Acceptability::kUnacceptable;
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
  suggestion.acceptability = Suggestion::Acceptability::kUnacceptable;
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
  const FormStructure* form_structure = manager.FindCachedFormById(form_id);
  const AutofillField* autofill_field =
      form_structure ? form_structure->GetFieldById(field_id) : nullptr;
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

}  // namespace

AtMemoryManager::AtMemoryManager(BrowserAutofillManager* manager)
    : owner_(manager) {}

AtMemoryManager::~AtMemoryManager() = default;

void AtMemoryManager::OnPopupShown(
    AutofillSuggestionTriggerSource trigger_source,
    bool is_context_secure,
    UpdateSuggestionsCallback update_callback) {
  if (at_memory_funnel_metrics_ || !IsAtMemoryTriggerSource(trigger_source)) {
    return;
  }

  trigger_source_ = trigger_source;
  is_context_secure_ = is_context_secure;
  update_callback_ = std::move(update_callback);
  at_memory_funnel_metrics_ = std::make_unique<AtMemoryFunnelMetrics>(
      owner_->client().GetMqlsUploadService(),
      owner_->client().GetLastCommittedPrimaryMainFrameURL(),
      owner_->client().GetPageTitle());
  at_memory_funnel_metrics_->OnPopupShown(trigger_source);
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
  suggestions.push_back(CreateSearchAffordanceSuggestion(filter));
  SendSuggestions(std::move(suggestions));
  return true;
}

bool AtMemoryManager::OnSearchSubmitted(const std::u16string& filter) {
  if (!IsAtMemoryTriggerSource(trigger_source_)) {
    return false;
  }
  if (at_memory_funnel_metrics_) {
    at_memory_funnel_metrics_->OnQuerySubmitted(filter);
  }
  ExecuteQuery(filter);
  return true;
}

void AtMemoryManager::OnPopupHidden() {
  trigger_source_ = AutofillSuggestionTriggerSource::kUnspecified;
  update_callback_.Reset();
  if (at_memory_funnel_metrics_) {
    at_memory_funnel_metrics_.reset();
  }
  CancelPendingQueries();
  is_context_secure_ = false;
}

void AtMemoryManager::FillOrPreviewSearchResult(
    mojom::ActionPersistence action_persistence,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion) {
  const Suggestion::AtMemoryPayload& payload =
      suggestion.GetPayload<Suggestion::AtMemoryPayload>();

  switch (action_persistence) {
    case mojom::ActionPersistence::kPreview:
      owner_->FillOrPreviewField(
          action_persistence, mojom::FieldActionType::kReplaceAtMemoryTrigger,
          form_id, field_id, payload.value, FillingProduct::kAtMemory,
          /*field_type_used=*/std::nullopt);
      break;
    case mojom::ActionPersistence::kFill: {
      if (at_memory_funnel_metrics_) {
        at_memory_funnel_metrics_->OnSuggestionAccepted();
      }
      // Transfer ownership of the metrics session to the filling path.
      // Ensures that the metrics will be properly recorded once the suggestion
      // is filled or one of the async steps in between fails.
      std::unique_ptr<AtMemoryFunnelMetrics> metrics =
          std::move(at_memory_funnel_metrics_);
      switch (payload.memory_data_type) {
        case MemoryDataType::kIban: {
          std::visit(absl::Overload{
                         [&](const Iban::Guid& guid) {
                           FillIban(guid, form_id, field_id, suggestion,
                                    std::move(metrics));
                         },
                         [&](const Iban::InstrumentId& instrument_id) {
                           FillIban(instrument_id, form_id, field_id,
                                    suggestion, std::move(metrics));
                         },
                         [](std::monostate) { NOTREACHED(); },
                         [](const std::string&) { NOTREACHED(); },
                         [](const EntityInstance::EntityId&) { NOTREACHED(); }},
                     payload.identifier);
          break;
        }
        case MemoryDataType::kCreditCardNumber:
        case MemoryDataType::kCreditCardSecurityCode: {
          CHECK(std::holds_alternative<std::string>(payload.identifier));
          FillCreditCard(std::get<std::string>(payload.identifier), form_id,
                         field_id, suggestion, std::move(metrics));
          break;
        }
        case MemoryDataType::kPassportFull:
        case MemoryDataType::kDriversLicenseFull:
        case MemoryDataType::kNationalIdCardFull:
        case MemoryDataType::kKnownTravelerNumberFull:
        case MemoryDataType::kRedressNumberFull:
        case MemoryDataType::kPassportNumber:
        case MemoryDataType::kDriversLicenseNumber:
        case MemoryDataType::kNationalIdCardNumber:
        case MemoryDataType::kKnownTravelerNumberNumber:
        case MemoryDataType::kRedressNumberNumber: {
          CHECK(std::holds_alternative<EntityInstance::EntityId>(
              payload.identifier));
          std::optional<AtMemoryDataType> data_type =
              ToAtMemoryDataType(payload.memory_data_type);
          CHECK(data_type &&
                (std::holds_alternative<AttributeType>(*data_type) ||
                 std::holds_alternative<EntityType>(*data_type)));
          FillSensitiveAutofillAiData(
              std::get<EntityInstance::EntityId>(payload.identifier), form_id,
              field_id, suggestion, *data_type, std::move(metrics));
          break;
        }

        default: {
          if (metrics) {
            metrics->MarkFilled();
          }
          owner_->FillOrPreviewField(
              action_persistence,
              mojom::FieldActionType::kReplaceAtMemoryTrigger, form_id,
              field_id, payload.value, FillingProduct::kAtMemory,
              /*field_type_used=*/std::nullopt);
          break;
        }
      }
      break;
    }
  }
}

bool AtMemoryManager::IsSearching() const {
  return is_searching_;
}

void AtMemoryManager::MaybeAppendPersonalContextNotice(
    std::vector<Suggestion>& suggestions) const {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return;
#else
  if (personal_context::features::
          IsPersonalContextFirstRunNoticePhase2Enabled()) {
    if (!owner_->client().ShouldShowPersonalContextAutofillNotice()) {
      return;
    }
    if (!suggestions.empty() &&
        suggestions.back().type == SuggestionType::kPersonalContextNotice) {
      return;
    }
    Suggestion& suggestion =
        suggestions.emplace_back(SuggestionType::kPersonalContextNotice);
    suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  }
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

void AtMemoryManager::ExecuteQuery(const std::u16string& filter) {
  accessibility_annotator::AtMemoryQueryService* query_service =
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
  ClearSuggestions();
  query_service->Query(
      filter,
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
  suggestion.acceptability = Suggestion::Acceptability::kAcceptable;
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

void AtMemoryManager::ClearSuggestions() {
  SendSuggestions({});
}

void AtMemoryManager::OnSearchResultsReceived(
    const std::u16string& query,
    accessibility_annotator::MemorySearchResults result) {
  if (!IsAtMemoryTriggerSource(trigger_source_) || !update_callback_ ||
      !is_searching_) {
    return;
  }

  // If the context is insecure or the device doesn't support OS reauth, filter
  // out any SPII entries and metadata from the results.
  if (!is_context_secure_ || !owner_->client().SupportsDeviceReauth()) {
    std::erase_if(result.entries,
                  [](const accessibility_annotator::MemorySearchResult& entry) {
                    return IsSpiiMemoryDataType(entry.type);
                  });
    for (accessibility_annotator::MemorySearchResult& entry : result.entries) {
      std::erase_if(entry.metadata_list,
                    [](const accessibility_annotator::EntryMetadata& metadata) {
                      return IsSpiiMemoryDataType(metadata.type);
                    });
    }
  }

  bool expecting_more_data =
      result.status ==
      accessibility_annotator::MemorySearchStatus::kPartialResponseSuccess;
  if (!expecting_more_data) {
    CancelPendingQueries();
  }

  // If there are results, just return the results as-is.
  if (!result.entries.empty()) {
    SendSuggestions(
        base::ToVector(result.entries, TransformResultIntoSuggestion));
    return;
  }

  // When search returns no entries, show the appropriate special
  // suggestion based on the status.
  std::vector<Suggestion> suggestions;
  switch (result.status) {
    case accessibility_annotator::MemorySearchStatus::kUnsupportedQuery:
      if (owner_->client().IsGlicEnabled()) {
        suggestions.push_back(CreateUnsupportedQuerySuggestion(query));
      } else {
        suggestions.push_back(CreateNoDataSuggestion());
      }
      break;
    case accessibility_annotator::MemorySearchStatus::kFinalResponseSuccess:
      suggestions.push_back(CreateNoDataSuggestion());
      break;
    case accessibility_annotator::MemorySearchStatus::kPartialResponseSuccess:
      break;
    case accessibility_annotator::MemorySearchStatus::kNoConnectionFailure:
      suggestions.push_back(CreateNoConnectionSuggestion());
      break;
    case accessibility_annotator::MemorySearchStatus::kInferenceFailure:
    case accessibility_annotator::MemorySearchStatus::kInternalFailure:
      suggestions.push_back(CreateGenericErrorSuggestion());
      break;
  }
  SendSuggestions(std::move(suggestions));
}

void AtMemoryManager::FillIban(
    const std::variant<Iban::Guid, Iban::InstrumentId>& identifier,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    std::unique_ptr<AtMemoryFunnelMetrics> metrics) {
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
    return;
  }

  if (metrics) {
    metrics->OnFetchStarted();
  }

  iban_access_manager->FetchValue(
      iban_payload,
      base::BindOnce(
          [](base::WeakPtr<AtMemoryManager> manager,
             const FormGlobalId& form_id, const FieldGlobalId& field_id,
             const Suggestion& suggestion,
             std::unique_ptr<AtMemoryFunnelMetrics> metrics,
             const std::u16string& unmasked_value) {
            if (!manager) {
              return;
            }
            if (metrics) {
              metrics->OnFetchCompleted();
              metrics->MarkFilled();
            }
            manager->owner_->FillOrPreviewField(
                mojom::ActionPersistence::kFill,
                mojom::FieldActionType::kReplaceAtMemoryTrigger, form_id,
                field_id, unmasked_value, FillingProduct::kAtMemory,
                /*field_type_used=*/std::nullopt);
          },
          fill_weak_ptr_factory_.GetWeakPtr(), form_id, field_id, suggestion,
          std::move(metrics)));
}

void AtMemoryManager::FillCreditCard(
    const std::string& guid,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    std::unique_ptr<AtMemoryFunnelMetrics> metrics) {
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
    metrics->OnFetchStarted();
  }

  // TODO(crbug.com/497795513): Consider caching fetched cards.
  credit_card_access_manager->FetchCreditCard(
      credit_card,
      base::BindOnce(
          [](base::WeakPtr<AtMemoryManager> manager,
             const FormGlobalId& form_id, const FieldGlobalId& field_id,
             const Suggestion& suggestion,
             std::unique_ptr<AtMemoryFunnelMetrics> metrics,
             const CreditCard& fetched_card) {
            if (!manager) {
              return;
            }
            if (metrics) {
              metrics->OnFetchCompleted();
              metrics->MarkFilled();
            }
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

void AtMemoryManager::FillSensitiveAutofillAiData(
    const EntityInstance::EntityId& entity_id,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    const AtMemoryDataType& data_type,
    std::unique_ptr<AtMemoryFunnelMetrics> metrics) {
  EntityDataManager* entity_data_manager =
      owner_->client().GetEntityDataManager();
  CHECK(entity_data_manager);

  base::optional_ref<const EntityInstance> entity =
      entity_data_manager->GetEntityInstance(entity_id);
  if (!entity) {
    return;
  }

  if (metrics) {
    metrics->OnFetchStarted();
  }

  owner_->GetAutofillAiAccessManager().FetchEntityInstance(
      *entity, /*will_fill_sensitive_info=*/true,
      base::BindOnce(&AtMemoryManager::OnAutofillAiFetched,
                     fill_weak_ptr_factory_.GetWeakPtr(), form_id, field_id,
                     suggestion, data_type, std::move(metrics)));
}

void AtMemoryManager::OnAutofillAiFetched(
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    const AtMemoryDataType& data_type,
    std::unique_ptr<AtMemoryFunnelMetrics> metrics,
    base::expected<EntityInstance, AutofillAiAccessManager::FailureReason>
        result,
    bool reauth_attempted) {
  if (!result.has_value()) {
    if (result.error() ==
        AutofillAiAccessManager::FailureReason::kFetchFailed) {
      owner_->client().ShowAutofillAiFetchFromWalletFailureNotification();
    }
    return;
  }

  const EntityInstance& fetched_entity = result.value();

  std::optional<AttributeType> target_attribute_type;
  if (std::holds_alternative<AttributeType>(data_type)) {
    target_attribute_type = std::get<AttributeType>(data_type);
  } else {
    CHECK(std::holds_alternative<EntityType>(data_type));
    target_attribute_type = GetPrimaryAttributeType(fetched_entity);
  }

  if (!target_attribute_type) {
    return;
  }

  std::optional<std::u16string> attribute_fill_value = GetAttributeFillValue(
      fetched_entity, *target_attribute_type, form_id, field_id, *owner_);
  if (!attribute_fill_value) {
    return;
  }

  if (metrics) {
    metrics->OnFetchCompleted();
    metrics->MarkFilled();
  }

  owner_->FillOrPreviewField(
      mojom::ActionPersistence::kFill,
      mojom::FieldActionType::kReplaceAtMemoryTrigger, form_id, field_id,
      std::move(*attribute_fill_value), FillingProduct::kAtMemory,
      /*field_type_used=*/std::nullopt);
}

}  // namespace autofill
