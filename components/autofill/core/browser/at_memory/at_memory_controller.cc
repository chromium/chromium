// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/accessibility_annotator/core/accessibility_query_service.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/at_memory/at_memory_funnel_metrics.h"
#include "components/autofill/core/browser/data_model/autofill_ai/from_accessibility_annotator.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

SuggestionType GetManageSuggestionType(
    accessibility_annotator::EntryType type) {
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
    accessibility_annotator::EntryType type,
    const std::variant<std::monostate, std::string, int64_t>& identifier) {
  if (type == accessibility_annotator::EntryType::kIban) {
    if (const std::string* guid_str = std::get_if<std::string>(&identifier)) {
      return Iban::Guid(*guid_str);
    }
    CHECK(std::holds_alternative<int64_t>(identifier));
    return Iban::InstrumentId(*std::get_if<int64_t>(&identifier));
  }
  if (type == accessibility_annotator::EntryType::kCreditCardNumber ||
      type == accessibility_annotator::EntryType::kCreditCardSecurityCode) {
    CHECK(std::holds_alternative<std::string>(identifier));
    return *std::get_if<std::string>(&identifier);
  }
  // TODO(crbug.com/497794390): Implement identifier support for other entry
  // types.
  return std::monostate();
}

Suggestion::Icon GetIconForEntryType(accessibility_annotator::EntryType type) {
  switch (type) {
    case accessibility_annotator::EntryType::kNameFull:
    case accessibility_annotator::EntryType::kAddressFull:
    case accessibility_annotator::EntryType::kAddressStreetAddress:
    case accessibility_annotator::EntryType::kAddressCity:
    case accessibility_annotator::EntryType::kAddressState:
    case accessibility_annotator::EntryType::kAddressZip:
    case accessibility_annotator::EntryType::kAddressCountry:
    case accessibility_annotator::EntryType::kPhone:
    case accessibility_annotator::EntryType::kCompanyName:
      return Suggestion::Icon::kAccount;
    case accessibility_annotator::EntryType::kEmail:
      return Suggestion::Icon::kEmail;
    case accessibility_annotator::EntryType::kIban:
    case accessibility_annotator::EntryType::kIbanNickname:
      return Suggestion::Icon::kIban;
    case accessibility_annotator::EntryType::kVehicle:
    case accessibility_annotator::EntryType::kVehicleMake:
    case accessibility_annotator::EntryType::kVehicleModel:
    case accessibility_annotator::EntryType::kVehicleYear:
    case accessibility_annotator::EntryType::kVehicleOwner:
    case accessibility_annotator::EntryType::kVehiclePlateNumber:
    case accessibility_annotator::EntryType::kVehiclePlateState:
    case accessibility_annotator::EntryType::kVehicleVin:
      return Suggestion::Icon::kVehicle;
    case accessibility_annotator::EntryType::kPassportFull:
    case accessibility_annotator::EntryType::kPassportName:
    case accessibility_annotator::EntryType::kPassportCountry:
    case accessibility_annotator::EntryType::kPassportNumber:
    case accessibility_annotator::EntryType::kPassportIssueDate:
    case accessibility_annotator::EntryType::kPassportExpirationDate:
      return Suggestion::Icon::kPassport;
    case accessibility_annotator::EntryType::kFlightReservationFull:
    case accessibility_annotator::EntryType::kFlightReservationFlightNumber:
    case accessibility_annotator::EntryType::kFlightReservationTicketNumber:
    case accessibility_annotator::EntryType::kFlightReservationConfirmationCode:
    case accessibility_annotator::EntryType::kFlightReservationPassengerName:
    case accessibility_annotator::EntryType::kFlightReservationDepartureAirport:
    case accessibility_annotator::EntryType::kFlightReservationArrivalAirport:
    case accessibility_annotator::EntryType::kFlightReservationDepartureDate:
    case accessibility_annotator::EntryType::kFlightReservationArrivalDate:
      return Suggestion::Icon::kFlight;
    case accessibility_annotator::EntryType::kNationalIdCardFull:
    case accessibility_annotator::EntryType::kNationalIdCardName:
    case accessibility_annotator::EntryType::kNationalIdCardCountry:
    case accessibility_annotator::EntryType::kNationalIdCardNumber:
    case accessibility_annotator::EntryType::kNationalIdCardIssueDate:
    case accessibility_annotator::EntryType::kNationalIdCardExpirationDate:
    case accessibility_annotator::EntryType::kDriversLicenseFull:
    case accessibility_annotator::EntryType::kDriversLicenseName:
    case accessibility_annotator::EntryType::kDriversLicenseState:
    case accessibility_annotator::EntryType::kDriversLicenseNumber:
    case accessibility_annotator::EntryType::kDriversLicenseIssueDate:
    case accessibility_annotator::EntryType::kDriversLicenseExpirationDate:
      return Suggestion::Icon::kIdCard;
    case accessibility_annotator::EntryType::kRedressNumberFull:
    case accessibility_annotator::EntryType::kRedressNumberName:
    case accessibility_annotator::EntryType::kRedressNumberNumber:
    case accessibility_annotator::EntryType::kKnownTravelerNumberFull:
    case accessibility_annotator::EntryType::kKnownTravelerNumberName:
    case accessibility_annotator::EntryType::kKnownTravelerNumberNumber:
    case accessibility_annotator::EntryType::kKnownTravelerNumberExpirationDate:
      return Suggestion::Icon::kPersonCheck;
    case accessibility_annotator::EntryType::kCreditCardNumber:
    case accessibility_annotator::EntryType::kCreditCardExpirationDate:
    case accessibility_annotator::EntryType::kCreditCardSecurityCode:
    case accessibility_annotator::EntryType::kCreditCardNameOnCard:
    case accessibility_annotator::EntryType::kCreditCardNickname:
      return Suggestion::Icon::kCardGeneric;
    case accessibility_annotator::EntryType::kOrderFull:
    case accessibility_annotator::EntryType::kOrderId:
    case accessibility_annotator::EntryType::kOrderAccount:
    case accessibility_annotator::EntryType::kOrderDate:
    case accessibility_annotator::EntryType::kOrderMerchantName:
    case accessibility_annotator::EntryType::kOrderMerchantDomain:
    case accessibility_annotator::EntryType::kOrderProductNames:
    case accessibility_annotator::EntryType::kOrderGrandTotal:
    case accessibility_annotator::EntryType::kShipmentFull:
    case accessibility_annotator::EntryType::kShipmentTrackingNumber:
    case accessibility_annotator::EntryType::kShipmentAssociatedOrderId:
    case accessibility_annotator::EntryType::kShipmentDeliveryAddress:
    case accessibility_annotator::EntryType::kShipmentDeliveryZipCode:
    case accessibility_annotator::EntryType::kShipmentCarrierName:
    case accessibility_annotator::EntryType::kShipmentCarrierDomain:
    case accessibility_annotator::EntryType::kShipmentEstimatedDeliveryDate:
    case accessibility_annotator::EntryType::kUnknown:
      return Suggestion::Icon::kNoIcon;
  }
  return Suggestion::Icon::kNoIcon;
}

Suggestion TransformResultIntoSuggestion(
    const accessibility_annotator::MemorySearchResult& entry) {
  Suggestion suggestion(entry.value, SuggestionType::kAtMemorySearchResult);
  suggestion.icon = GetIconForEntryType(entry.type);
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
                                 ? GetEntryTypeNameForI18n(entry.type)
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
  suggestion.labels.emplace_back(std::move(label_row));
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
        metadata.type_name.empty() ? GetEntryTypeNameForI18n(metadata.type)
                                   : metadata.type_name;
    if (!child_type_name.empty()) {
      child.labels = {{Suggestion::Text(child_type_name)}};
    }
    Suggestion::AtMemoryPayload child_at_memory_payload(metadata.value,
                                                        metadata.type);
    child_at_memory_payload.entry_type = metadata.type;
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

// Creates a suggestion to display when the query is supported, but yields no
// results.
Suggestion CreateNoDataSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_NO_DATA),
      SuggestionType::kAtMemorySearchResult);
  suggestion.acceptability =
      Suggestion::Acceptability::kUnacceptableWithDeactivatedStyle;
  suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  return suggestion;
}

}  // namespace

AtMemoryController::AtMemoryController(BrowserAutofillManager* manager)
    : owner_(manager) {}

AtMemoryController::~AtMemoryController() = default;

void AtMemoryController::OnPopupShown(
    AutofillSuggestionTriggerSource trigger_source,
    UpdateSuggestionsCallback update_callback) {
  if (at_memory_funnel_metrics_ || !IsAtMemoryTriggerSource(trigger_source)) {
    return;
  }

  trigger_source_ = trigger_source;
  update_callback_ = std::move(update_callback);
  at_memory_funnel_metrics_ = std::make_unique<AtMemoryFunnelMetrics>();
  at_memory_funnel_metrics_->OnPopupShown(trigger_source);
}

bool AtMemoryController::OnFilterChanged(const std::u16string& filter) {
  if (!IsAtMemoryTriggerSource(trigger_source_)) {
    return false;
  }
  ExecuteQuery(filter, /*full_search=*/false);
  return true;
}

bool AtMemoryController::OnSearchSubmitted(const std::u16string& filter) {
  if (!IsAtMemoryTriggerSource(trigger_source_)) {
    return false;
  }
  if (at_memory_funnel_metrics_) {
    at_memory_funnel_metrics_->OnQuerySubmitted();
  }
  ExecuteQuery(filter, /*full_search=*/true);
  return true;
}

void AtMemoryController::OnPopupHidden() {
  trigger_source_ = AutofillSuggestionTriggerSource::kUnspecified;
  update_callback_.Reset();
  if (at_memory_funnel_metrics_) {
    at_memory_funnel_metrics_->OnPopupHidden();
    at_memory_funnel_metrics_.reset();
  }
}

void AtMemoryController::FillOrPreviewSearchResult(
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    const FormFieldData& field,
    const Suggestion& suggestion) {
  const Suggestion::AtMemoryPayload& payload =
      suggestion.GetPayload<Suggestion::AtMemoryPayload>();

  switch (action_persistence) {
    case mojom::ActionPersistence::kPreview:
      owner_->FillOrPreviewField(
          action_persistence, mojom::FieldActionType::kReplaceAtMemoryTrigger,
          form, field, payload.value, FillingProduct::kAtMemory,
          /*field_type_used=*/std::nullopt);
      break;
    case mojom::ActionPersistence::kFill:
      switch (payload.entry_type) {
        case accessibility_annotator::EntryType::kIban: {
          CHECK(!std::holds_alternative<std::monostate>(payload.identifier));
          FillIban(payload.identifier, form, field, suggestion);
          break;
        }
        case accessibility_annotator::EntryType::kCreditCardNumber:
        case accessibility_annotator::EntryType::kCreditCardSecurityCode: {
          CHECK(std::holds_alternative<std::string>(payload.identifier));
          FillCreditCard(payload.identifier, form, field, suggestion);
          break;
        }

        default:
          if (at_memory_funnel_metrics_) {
            at_memory_funnel_metrics_->OnSuggestionAccepted();
          }
          owner_->FillOrPreviewField(
              action_persistence,
              mojom::FieldActionType::kReplaceAtMemoryTrigger, form, field,
              payload.value, FillingProduct::kAtMemory,
              /*field_type_used=*/std::nullopt);
          break;
      }
      break;
  }
}

bool AtMemoryController::IsSearching() const {
  return is_searching_;
}

void AtMemoryController::ExecuteQuery(const std::u16string& filter,
                                      bool full_search) {
  accessibility_annotator::AccessibilityQueryService* query_service =
      owner_->client().GetAccessibilityQueryService();
  if (!query_service || !IsAtMemoryTriggerSource(trigger_source_) ||
      !update_callback_) {
    return;
  }

  // Cancel stale updates from previous queries.
  query_weak_ptr_factory_.InvalidateWeakPtrs();

  if (filter.empty()) {
    is_searching_ = false;
    update_callback_.Run({}, trigger_source_);
    return;
  }

  is_searching_ = true;
  // Notify the UI that search has started. We repass the current suggestions
  // to prevent them from disappearing while the search is in progress.
  base::span<const Suggestion> current_suggestions =
      owner_->client().GetAutofillSuggestions();
  update_callback_.Run(base::ToVector(current_suggestions), trigger_source_);
  query_service->Query(
      filter, full_search,
      base::BindRepeating(&AtMemoryController::OnSearchResultsReceived,
                          query_weak_ptr_factory_.GetWeakPtr(), filter,
                          full_search));
}

// Creates a suggestion to offer to open Gemini in the sidebar when the query is
// unsupported.
Suggestion AtMemoryController::CreateUnsupportedQuerySuggestion(
    const std::u16string& query) {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_UNSUPPORTED_QUERY_TITLE),
      SuggestionType::kOpenGemini);
  suggestion.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_AT_MEMORY_UNSUPPORTED_QUERY_DESCRIPTION))}};
  suggestion.acceptability = Suggestion::Acceptability::kAcceptable;
  suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  suggestion.payload = Suggestion::OpenGeminiPayload(query);
  return suggestion;
}

void AtMemoryController::OnSearchResultsReceived(
    const std::u16string& query,
    bool full_search,
    accessibility_annotator::MemorySearchResults result) {
  if (!IsAtMemoryTriggerSource(trigger_source_) || !update_callback_ ||
      !is_searching_) {
    return;
  }

  bool expecting_more_data =
      result.status ==
      accessibility_annotator::MemorySearchStatus::kPartialResponseSuccess;
  if (!expecting_more_data) {
    is_searching_ = false;
  }

  // For incremental search or if there are results, just return the results
  // as-is.
  if (!full_search || !result.entries.empty()) {
    update_callback_.Run(
        base::ToVector(result.entries, TransformResultIntoSuggestion),
        trigger_source_);
    return;
  }

  // When full search returns no entries, show the appropriate special
  // suggestion based on the status.
  std::vector<Suggestion> suggestions;
  switch (result.status) {
    case accessibility_annotator::MemorySearchStatus::kUnsupportedQuery:
      suggestions.push_back(CreateUnsupportedQuerySuggestion(query));
      break;
    case accessibility_annotator::MemorySearchStatus::kFinalResponseSuccess:
      suggestions.push_back(CreateNoDataSuggestion());
      break;
    case accessibility_annotator::MemorySearchStatus::kPartialResponseSuccess:
    case accessibility_annotator::MemorySearchStatus::kInferenceFailure:
    case accessibility_annotator::MemorySearchStatus::kDataFetchFailure:
    case accessibility_annotator::MemorySearchStatus::kInternalFailure:
      break;
  }
  update_callback_.Run(std::move(suggestions), trigger_source_);
}

void AtMemoryController::FillIban(
    const Suggestion::AtMemoryPayload::Identifier& identifier,
    const FormData& form,
    const FormFieldData& field,
    const Suggestion& suggestion) {
  Suggestion::Payload iban_payload;
  if (const Iban::Guid* guid = std::get_if<Iban::Guid>(&identifier)) {
    iban_payload = Suggestion::Guid(guid->value());
  } else {
    CHECK(std::holds_alternative<Iban::InstrumentId>(identifier));
    iban_payload = Suggestion::InstrumentId(
        std::get<Iban::InstrumentId>(identifier).value());
  }

  IbanAccessManager* iban_access_manager =
      owner_->client().GetPaymentsAutofillClient()->GetIbanAccessManager();
  if (!iban_access_manager) {
    return;
  }

  iban_access_manager->FetchValue(
      iban_payload,
      base::BindOnce(
          [](base::WeakPtr<AtMemoryController> controller, const FormData& form,
             const FormFieldData& field, const Suggestion& suggestion,
             const std::u16string& unmasked_value) {
            if (!controller) {
              return;
            }
            if (controller->at_memory_funnel_metrics_) {
              controller->at_memory_funnel_metrics_->OnSuggestionAccepted();
            }
            controller->owner_->FillOrPreviewField(
                mojom::ActionPersistence::kFill,
                mojom::FieldActionType::kReplaceAtMemoryTrigger, form, field,
                unmasked_value, FillingProduct::kAtMemory,
                /*field_type_used=*/std::nullopt);
          },
          fill_weak_ptr_factory_.GetWeakPtr(), form, field, suggestion));
}

void AtMemoryController::FillCreditCard(
    const Suggestion::AtMemoryPayload::Identifier& identifier,
    const FormData& form,
    const FormFieldData& field,
    const Suggestion& suggestion) {
  CHECK(std::holds_alternative<std::string>(identifier));
  const std::string& guid = std::get<std::string>(identifier);

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

  // TODO(crbug.com/497795513): Consider caching fetched cards.
  credit_card_access_manager->FetchCreditCard(
      credit_card,
      base::BindOnce(
          [](base::WeakPtr<AtMemoryController> controller, const FormData& form,
             const FormFieldData& field, const Suggestion& suggestion,
             const CreditCard& fetched_card) {
            if (!controller) {
              return;
            }
            if (controller->at_memory_funnel_metrics_) {
              controller->at_memory_funnel_metrics_->OnSuggestionAccepted();
            }
            const Suggestion::AtMemoryPayload& payload =
                suggestion.GetPayload<Suggestion::AtMemoryPayload>();
            std::u16string fill_value;
            switch (payload.entry_type) {
              case accessibility_annotator::EntryType::kCreditCardNumber:
                fill_value = fetched_card.number();
                break;
              case accessibility_annotator::EntryType::kCreditCardSecurityCode:
                fill_value = fetched_card.cvc();
                break;
              default:
                NOTREACHED();
            }

            controller->owner_->FillOrPreviewField(
                mojom::ActionPersistence::kFill,
                mojom::FieldActionType::kReplaceAtMemoryTrigger, form, field,
                fill_value, FillingProduct::kAtMemory,
                /*field_type_used=*/std::nullopt);
          },
          fill_weak_ptr_factory_.GetWeakPtr(), form, field, suggestion));
}

}  // namespace autofill
