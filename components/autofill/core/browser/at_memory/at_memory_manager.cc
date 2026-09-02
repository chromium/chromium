// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_manager.h"

#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/adapters.h"
#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/at_memory/at_memory_enablement_util.h"
#include "components/autofill/core/browser/at_memory/at_memory_metrics_recorder.h"
#include "components/autofill/core/browser/at_memory/at_memory_persisted_state_manager.h"
#include "components/autofill/core/browser/at_memory/at_memory_search_state.h"
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
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"
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
#include "components/personal_context/first_run/personal_context_first_run_service.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/network_change_notifier.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// Duration between advances of the fetching suggestion message.
constexpr base::TimeDelta kFetchingMessageInterval = base::Seconds(3);

constexpr std::array<int, 3> kFetchingStringIds = std::to_array<int>({
    IDS_AUTOFILL_AT_MEMORY_FETCHING_FINDING_INFO_WITH_GEMINI,
    IDS_AUTOFILL_AT_MEMORY_FETCHING_REVIEWING_CONNECTED_APPS,
    IDS_AUTOFILL_AT_MEMORY_FETCHING_PUTTING_IT_TOGETHER,
});

// Returns the primary type name label for `entry`. For AutofillAi
// entities and attributes, this resolves to the Entity name.
std::u16string GetSuggestionLabelTypeName(const MemorySearchResult& entry) {
  if (std::optional<AttributeType> attribute_type =
          ToAttributeType(entry.type)) {
    return attribute_type->entity_type().GetNameForI18n();
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

  switch (GetMemoryDataTypeCategory(type)) {
    case MemoryDataTypeCategory::kIban: {
      if (const std::string* guid = std::get_if<std::string>(&identifier)) {
        return Iban::Guid(*guid);
      }
      if (const int64_t* instrument_id = std::get_if<int64_t>(&identifier)) {
        return Iban::InstrumentId(*instrument_id);
      }
      NOTREACHED();
    }
    case MemoryDataTypeCategory::kPassport:
    case MemoryDataTypeCategory::kDriversLicense:
    case MemoryDataTypeCategory::kNationalIdCard:
    case MemoryDataTypeCategory::kFlightReservation:
    case MemoryDataTypeCategory::kKnownTravelerNumber:
    case MemoryDataTypeCategory::kRedressNumber:
    case MemoryDataTypeCategory::kVehicle:
    case MemoryDataTypeCategory::kOrder:
    case MemoryDataTypeCategory::kShipment:
      return EntityInstance::EntityId(std::get<std::string>(identifier));
    case MemoryDataTypeCategory::kCreditCard:
      return std::get<std::string>(identifier);
    case MemoryDataTypeCategory::kContactInfo:
      return std::get<std::string>(identifier);
    case MemoryDataTypeCategory::kUnknown:
      return std::monostate();
  }
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
    suggestions.emplace_back(
        AtMemoryManager::CreateSourceAttributionSuggestion());
    suggestions.emplace_back(std::move(separator));
    suggestions.emplace_back(CreateManageEnhancedAutofillSuggestion());
  }
  return suggestions;
}

// Creates a suggestion to display when the query is supported, but yields no
// results.
Suggestion CreateNoDataSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_NO_DATA),
      SuggestionType::kAtMemorySearchResult);
  suggestion.acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;
  suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  suggestion.icon = Suggestion::Icon::kSadTab;
  return suggestion;
}



std::optional<std::u16string> GetAttributeFillValue(
    const EntityInstance& entity,
    AttributeType attribute_type,
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
  std::string debug_reason;
  const bool may_perform = MayPerformAtMemoryAction(
      *action, client,
      /*url=*/std::nullopt,
      RetrieveForFillingParams{.is_spii = IsSpiiMemoryDataType(type),
                               .sources = sources,
                               .is_context_secure = is_context_secure},
      &debug_reason);
  if (!may_perform) {
    LogAtMemorySuppression(*action, client.GetCurrentLogManager(),
                           debug_reason);
  }
  return !may_perform;
}

}  // namespace

// static
Suggestion AtMemoryManager::TransformResultIntoSuggestion(
    const MemorySearchResult& entry,
    std::string_view app_locale) {
  const bool is_personal_context_sourced =
      !IsMemorySearchResultAutofillSourced(entry);
  Suggestion suggestion(
      MaybeObfuscateValue(entry.value, entry.type, is_personal_context_sourced),
      SuggestionType::kAtMemorySearchResult);
  suggestion.icon = GetSuggestionIcon(
      entry.type,
      /*is_autofill_only=*/entry.sources.size() == 1 &&
          entry.sources.front().type == MemoryEntrySourceType::kAutofill);

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
    std::u16string label_value = FormatMemoryDataTypeLabelValue(
        metadata.type, metadata.value, metadata.typed_value, app_locale);
    label_row.emplace_back(MaybeObfuscateValue(label_value, metadata.type,
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
  for (const MemoryEntrySource& source : entry.sources) {
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

// static
Suggestion AtMemoryManager::CreateSourceAttributionSuggestion() {
  Suggestion source_info(SuggestionType::kAtMemorySourceAttribution);
  source_info.minor_texts.emplace_back(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_AT_MEMORY_SOURCE_ATTRIBUTION_PERSONAL_INTELLIGENCE));
  source_info.acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;
  source_info.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  return source_info;
}

AtMemoryManager::AtMemoryManager(AutofillClient* client,
                                 history::HistoryService* history_service)
    : client_(CHECK_DEREF(client)), state_manager_(history_service) {}

AtMemoryManager::~AtMemoryManager() = default;

AtMemorySearchState AtMemoryManager::GetStateForField(
    const FieldGlobalId& field_id,
    const url::Origin& field_origin) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillAtMemorySearchStatefulness)) {
    if (const std::optional<AtMemorySearchState>& state =
            state_manager_.GetStateForField(field_id, field_origin)) {
      return *state;
    }
  } else {
    target_field_origin_ = field_origin;
  }
  return {.suggestions = GetEmptyQuerySuggestions()};
}

const url::Origin& AtMemoryManager::target_field_origin() const {
  return base::FeatureList::IsEnabled(
             features::kAutofillAtMemorySearchStatefulness)
             ? state_manager_.field_origin()
             : target_field_origin_;
}

void AtMemoryManager::OnPopupShown(
    BrowserAutofillManager& bam,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    AutofillSuggestionTriggerSource trigger_source,
    base::optional_ref<const AutofillSuggestionDelegate::SuggestionMetadata>
        parent_suggestion_metadata,
    UpdateSuggestionsCallback update_callback,
    ukm::SourceId ukm_source_id) {
  if (!IsAtMemoryTriggerSource(trigger_source)) {
    return;
  }
  if (!parent_suggestion_metadata && !popup_state_) {
    const auto [form, field] = bam.FindFormAndField(form_id, field_id);
    const FormSignature form_signature =
        form ? form->form_signature() : FormSignature(0);
    const FieldSignature field_signature =
        field ? field->GetFieldSignature() : FieldSignature(0);
    if (!base::FeatureList::IsEnabled(
            features::kAutofillAtMemorySearchStatefulness)) {
      target_field_origin_ = field ? field->origin() : url::Origin();
    }
    popup_state_.emplace();
    popup_state_->trigger_source = trigger_source;
    popup_state_->update_callback = std::move(update_callback);
    popup_state_->metrics_recorder = std::make_unique<AtMemoryMetricsRecorder>(
        client_->GetMqlsUploadService(), client_->GetUkmRecorder(),
        ukm_source_id, client_->GetLastCommittedPrimaryMainFrameURL(),
        client_->GetPageTitle(), field_id, form_signature, field_signature);
    // TODO(crbug.com/535486238): Restart `fetching_timer` if search is still
    // in progress when reopening the popup.
  }

  if (popup_state_ && popup_state_->metrics_recorder) {
    popup_state_->metrics_recorder->OnPopupShown(trigger_source,
                                                 parent_suggestion_metadata);
  }
}

bool AtMemoryManager::OnFilterChanged(const std::u16string& filter) {
  if (!popup_state_ || !IsAtMemoryTriggerSource(popup_state_->trigger_source)) {
    return false;
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillAtMemorySearchStatefulness)) {
    state_manager_.OnFilterChanged(filter);
  }
  if (filter.empty()) {
    CancelPendingQueries();
    ShowEmptyQuerySuggestions();
    return true;
  }
  ShowQueryTypingSuggestions(filter);
  return true;
}

bool AtMemoryManager::OnSearchSubmitted(const std::u16string& filter) {
  if (!popup_state_ || !IsAtMemoryTriggerSource(popup_state_->trigger_source)) {
    return false;
  }
  if (popup_state_->metrics_recorder) {
    popup_state_->metrics_recorder->OnQuerySubmitted(filter);
  }
  ExecuteQuery(filter);
  return true;
}

void AtMemoryManager::OnPopupHidden() {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAtMemorySearchStatefulness)) {
    CancelPendingQueries();
    target_field_origin_ = url::Origin();
  }
  popup_state_.reset();
}

IsAsync AtMemoryManager::FillSearchResult(
    BrowserAutofillManager& bam,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    base::optional_ref<const AutofillSuggestionDelegate::SuggestionMetadata>
        metadata) {
  const Suggestion::AtMemoryPayload& payload =
      suggestion.GetPayload<Suggestion::AtMemoryPayload>();
  if (popup_state_ && popup_state_->metrics_recorder) {
    popup_state_->metrics_recorder->OnSuggestionAccepted(
        payload.memory_data_type, payload.sources_bitmask, metadata);
  }
  // Transfer ownership of the metrics session to the filling path.
  // Ensures that the metrics will be properly recorded once the suggestion
  // is filled or one of the async steps in between fails.
  std::unique_ptr<AtMemoryMetricsRecorder> metrics;
  if (popup_state_) {
    metrics = std::move(popup_state_->metrics_recorder);
  }

  IsAsync is_async = [&]() {
    switch (payload.memory_data_type) {
      case MemoryDataType::kIban: {
        std::visit(absl::Overload{
                       [&](const Iban::Guid& guid) {
                         FillIban(bam, guid, form_id, field_id, suggestion,
                                  std::move(metrics));
                       },
                       [&](const Iban::InstrumentId& instrument_id) {
                         FillIban(bam, instrument_id, form_id, field_id,
                                  suggestion, std::move(metrics));
                       },
                       [](std::monostate) { NOTREACHED(); },
                       [](const std::string&) { NOTREACHED(); },
                       [](const EntityInstance::EntityId&) { NOTREACHED(); }},
                   payload.identifier);
        return IsAsync(false);
      }
      case MemoryDataType::kCreditCardNumber:
      case MemoryDataType::kCreditCardSecurityCode: {
        CHECK(std::holds_alternative<std::string>(payload.identifier));
        FillCreditCard(bam, std::get<std::string>(payload.identifier), form_id,
                       field_id, suggestion, std::move(metrics));
        return IsAsync(false);
      }
      case MemoryDataType::kPassportNumber:
      case MemoryDataType::kDriversLicenseNumber:
      case MemoryDataType::kNationalIdCardNumber:
      case MemoryDataType::kKnownTravelerNumberNumber:
      case MemoryDataType::kRedressNumberNumber: {
        return FillSensitiveAutofillAiOrPersonalContextData(
            bam, form_id, field_id, suggestion, std::move(metrics));
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
        bam.FillOrPreviewField(
            mojom::ActionPersistence::kFill,
            mojom::FieldActionType::kReplaceSelectionForAtMemory, form_id,
            field_id, payload.value, FillingProduct::kAtMemory,
            /*field_type_used=*/std::nullopt);
        return IsAsync(false);
      }

      case MemoryDataType::kCreditCardExpirationDate:
      case MemoryDataType::kCreditCardNameOnCard: {
        RecordCreditCardUse(payload.identifier);
        if (metrics) {
          metrics->MarkFilled();
        }
        bam.FillOrPreviewField(
            mojom::ActionPersistence::kFill,
            mojom::FieldActionType::kReplaceSelectionForAtMemory, form_id,
            field_id, payload.value, FillingProduct::kAtMemory,
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
        bam.FillOrPreviewField(
            mojom::ActionPersistence::kFill,
            mojom::FieldActionType::kReplaceSelectionForAtMemory, form_id,
            field_id, payload.value, FillingProduct::kAtMemory,
            /*field_type_used=*/std::nullopt);
        return IsAsync(false);
      }

      case MemoryDataType::kCreditCardNickname:
      case MemoryDataType::kIbanNickname:
      case MemoryDataType::kUnknown: {
        if (metrics) {
          metrics->MarkFilled();
        }
        bam.FillOrPreviewField(
            mojom::ActionPersistence::kFill,
            mojom::FieldActionType::kReplaceSelectionForAtMemory, form_id,
            field_id, payload.value, FillingProduct::kAtMemory,
            /*field_type_used=*/std::nullopt);
        return IsAsync(false);
      }
    }
    NOTREACHED();
  }();

  if (base::FeatureList::IsEnabled(
          features::kAutofillAtMemorySearchStatefulness)) {
    state_manager_.OnSuggestionAccepted(suggestion);
  }

  return is_async;
}

void AtMemoryManager::RecordAddressProfileUse(
    const Suggestion::AtMemoryPayload::Identifier& identifier) {
  const std::string* guid = std::get_if<std::string>(&identifier);
  if (!guid || guid->empty()) {
    return;
  }

  AddressDataManager& adm =
      client_->GetPersonalDataManager().address_data_manager();
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
      client_->GetPersonalDataManager().payments_data_manager();
  if (const CreditCard* credit_card = pdm.GetCreditCardByGUID(*guid)) {
    pdm.RecordUseOfCard(*credit_card);
  }
}

void AtMemoryManager::RecordAutofillAiEntityUse(
    const Suggestion::AtMemoryPayload::Identifier& identifier) {
  if (EntityDataManager* edm = client_->GetEntityDataManager()) {
    if (const EntityInstance::EntityId* entity_id =
            std::get_if<EntityInstance::EntityId>(&identifier)) {
      if (!entity_id->value().empty()) {
        edm->RecordEntityUsed(*entity_id, base::Time::Now());
      }
    }
  }
}

bool AtMemoryManager::IsSearching() const {
  if (base::FeatureList::IsEnabled(
          features::kAutofillAtMemorySearchStatefulness)) {
    return state_manager_.IsSearching();
  }
  return popup_state_ && popup_state_->is_searching;
}

void AtMemoryManager::MaybeAppendPersonalContextNotice(
    std::vector<Suggestion>& suggestions) const {
  personal_context::PersonalContextFirstRunService* service =
      client_->GetPersonalContextFirstRunService();
  if (!service || !service->ShouldShowPersonalContextAtMemoryNotice()) {
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

void AtMemoryManager::MaybeAppendPreviouslyFilledSuggestions(
    std::vector<Suggestion>& suggestions) const {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAtMemoryPreviouslyFilled) ||
      state_manager_.previously_filled_suggestions().empty()) {
    return;
  }
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_PREVIOUSLY_FILLED),
      SuggestionType::kTitle);
  suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  suggestion.acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;
  suggestions.push_back(std::move(suggestion));
  base::Extend(suggestions,
               base::Reversed(state_manager_.previously_filled_suggestions()));
}

void AtMemoryManager::ExecuteQuery(const std::u16string& filter) {
  AtMemoryQueryService* query_service = client_->GetAtMemoryQueryService();
  if (!query_service || !popup_state_ ||
      !IsAtMemoryTriggerSource(popup_state_->trigger_source) ||
      !popup_state_->update_callback) {
    return;
  }

  // Cancel stale updates from previous queries.
  // At any point in time, there can be only one pending query.
  CancelPendingQueries();

  if (filter.empty()) {
    ShowEmptyQuerySuggestions();
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillAtMemorySearchStatefulness)) {
    state_manager_.OnFilterSubmitted(filter);
  } else {
    popup_state_->is_searching = true;
  }
  popup_state_->fetching_string_index = 0;
  ShowFetchingStateSuggestions();
  popup_state_->fetching_timer.Start(
      FROM_HERE, kFetchingMessageInterval,
      base::BindRepeating(&AtMemoryManager::AdvanceFetchingSuggestion,
                          query_weak_ptr_factory_.GetWeakPtr()));
  query_service->Query(
      filter, client_->GetLastCommittedPrimaryMainFrameURL(),
      client_->GetPageTitle(),
      base::BindRepeating(&AtMemoryManager::OnSearchResultsReceived,
                          query_weak_ptr_factory_.GetWeakPtr(), filter));
}

// Creates a suggestion to offer to open Gemini in the sidebar when the query is
// unsupported.
Suggestion AtMemoryManager::CreateUnsupportedQuerySuggestion(
    const std::u16string& query) {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_UNSUPPORTED_QUERY_TITLE),
      SuggestionType::kAtMemoryOpenGemini);
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

// static
void AtMemoryManager::MaybeAppendAiDisclosure(
    std::vector<Suggestion>& suggestions) {
  // Do not append Ai Disclosure, if Personal Context Notice is there.
  if (std::ranges::contains(suggestions, SuggestionType::kPersonalContextNotice,
                            &Suggestion::type)) {
    return;
  }
  // Append separator
  suggestions.emplace_back(SuggestionType::kSeparator);
  suggestions.back().filtration_policy = Suggestion::FiltrationPolicy::kStatic;

  // Append Ai Disclosure
  suggestions.emplace_back(SuggestionType::kAtMemoryAiDisclosure);
  suggestions.back().acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;
  suggestions.back().filtration_policy = Suggestion::FiltrationPolicy::kStatic;
}

Suggestion AtMemoryManager::CreateFetchingSuggestion(size_t index) {
  size_t string_index = index % kFetchingStringIds.size();
  Suggestion suggestion(
      l10n_util::GetStringUTF16(kFetchingStringIds[string_index]),
      SuggestionType::kAtMemoryFetching);
  suggestion.acceptability =
      Suggestion::Acceptability::kSelectableButUnacceptable;
  suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  return suggestion;
}

Suggestion AtMemoryManager::CreateGenericErrorSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_GENERIC_ERROR),
      SuggestionType::kAtMemoryGenericError);
  suggestion.acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;
  suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  suggestion.icon = Suggestion::Icon::kSadTab;
  return suggestion;
}

Suggestion AtMemoryManager::CreateNoConnectionSuggestion(std::u16string query) {
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

void AtMemoryManager::CancelPendingQueries() {
  if (popup_state_) {
    popup_state_->fetching_timer.Stop();
    popup_state_->fetching_string_index = 0;
  }
  query_weak_ptr_factory_.InvalidateWeakPtrs();
  if (base::FeatureList::IsEnabled(
          features::kAutofillAtMemorySearchStatefulness)) {
    state_manager_.StopSearching();
  } else if (popup_state_) {
    popup_state_->is_searching = false;
  }
}

void AtMemoryManager::SendSuggestions(std::vector<Suggestion> suggestions) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillAtMemorySearchStatefulness)) {
    state_manager_.OnSuggestionsChanged(suggestions);
  }
  if (popup_state_ && popup_state_->update_callback) {
    popup_state_->update_callback.Run(std::move(suggestions),
                                      popup_state_->trigger_source);
  }
}

void AtMemoryManager::AdvanceFetchingSuggestion() {
  CHECK(popup_state_);
  popup_state_->fetching_string_index++;
  ShowFetchingStateSuggestions();
}

void AtMemoryManager::ShowFetchingStateSuggestions() {
  CHECK(popup_state_);
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(
      CreateFetchingSuggestion(popup_state_->fetching_string_index));
  MaybeAppendPersonalContextNotice(suggestions);
  SendSuggestions(std::move(suggestions));
}

std::vector<Suggestion> AtMemoryManager::GetEmptyQuerySuggestions() const {
  std::vector<Suggestion> suggestions;
  MaybeAppendPersonalContextNotice(suggestions);
  MaybeAppendPreviouslyFilledSuggestions(suggestions);
  return suggestions;
}

void AtMemoryManager::ShowEmptyQuerySuggestions() {
  std::vector<Suggestion> suggestions = GetEmptyQuerySuggestions();
  SendSuggestions(std::move(suggestions));
}

void AtMemoryManager::ShowQueryTypingSuggestions(const std::u16string& query) {
  std::vector<Suggestion> suggestions;
  if (net::NetworkChangeNotifier::IsOffline()) {
    suggestions.push_back(CreateNoConnectionSuggestion(query));
  } else {
    suggestions.push_back(CreateSearchAffordanceSuggestion(query));
  }
  MaybeAppendPersonalContextNotice(suggestions);
  MaybeAppendAiDisclosure(suggestions);
  SendSuggestions(std::move(suggestions));
}

void AtMemoryManager::ShowResultsRetrievedStateSuggestions(
    const MemorySearchResults& result) {
  std::vector<Suggestion> suggestions;
  const std::string_view app_locale = client_->GetAppLocale();
  suggestions =
      base::ToVector(result.entries, [&](const MemorySearchResult& entry) {
        return TransformResultIntoSuggestion(entry, app_locale);
      });
  MaybeAppendPersonalContextNotice(suggestions);
  SendSuggestions(std::move(suggestions));
}

void AtMemoryManager::ShowNoResultsStateSuggestions(
    const std::u16string& query,
    const MemorySearchResults& result) {
  std::vector<Suggestion> suggestions;
  switch (result.status) {
    case MemorySearchStatus::kUnsupportedQuery:
      if (client_->IsGlicEnabled()) {
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
  MaybeAppendPersonalContextNotice(suggestions);
  SendSuggestions(std::move(suggestions));
}

void AtMemoryManager::OnSearchResultsReceived(const std::u16string& query,
                                              MemorySearchResults result) {
  if ((!popup_state_ && !base::FeatureList::IsEnabled(
                            features::kAutofillAtMemorySearchStatefulness)) ||
      !IsSearching()) {
    return;
  }

  bool expecting_more_data =
      result.status == MemorySearchStatus::kPartialResponseSuccess;
  if (!expecting_more_data) {
    CancelPendingQueries();
  }

  // TODO(crbug.com/535486238): Handle metrics recording when background query
  // finishes with the popup closed.
  if (popup_state_ && popup_state_->metrics_recorder) {
    popup_state_->metrics_recorder->OnQueryResponseReceived(result);
  }

  const bool is_context_secure = client_->IsContextSecure();
  if (!result.entries.empty()) {
    std::erase_if(result.entries,
                  [this, is_context_secure](const MemorySearchResult& entry) {
                    return ShouldEraseMemorySearchResult(
                        entry.type, entry.sources, *client_, is_context_secure);
                  });
    for (MemorySearchResult& entry : result.entries) {
      std::erase_if(entry.metadata_list, [this, &entry, is_context_secure](
                                             const EntryMetadata& metadata) {
        return ShouldEraseMemorySearchResult(metadata.type, entry.sources,
                                             *client_, is_context_secure);
      });
    }

    if (!result.entries.empty()) {
      ShowResultsRetrievedStateSuggestions(result);
      return;
    }
  }

  // When search returns no entries, show the appropriate special
  // suggestion based on the status.
  ShowNoResultsStateSuggestions(query, result);
}

void AtMemoryManager::FillIban(
    BrowserAutofillManager& bam,
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
      client_->GetPaymentsAutofillClient()->GetIbanAccessManager();
  if (!iban_access_manager) {
    return;
  }

  if (metrics) {
    metrics->OnFetchPiiStarted(AtMemoryMetricsRecorder::FetchPiiSource::kIban);
  }

  iban_access_manager->FetchValue(
      iban_payload,
      base::BindOnce(
          [](base::WeakPtr<AtMemoryManager> manager,
             base::WeakPtr<BrowserAutofillManager> bam,
             const FormGlobalId& form_id, const FieldGlobalId& field_id,
             const Suggestion& suggestion,
             std::unique_ptr<AtMemoryMetricsRecorder> metrics,
             std::variant<Iban::Guid, Iban::InstrumentId> identifier,
             base::expected<std::u16string, IbanAccessManager::FailureReason>
                 unmasked_value) {
            if (!manager || !bam) {
              return;
            }
            if (!unmasked_value.has_value()) {
              return;
            }
            if (metrics) {
              metrics->OnFetchPiiCompleted();
              metrics->MarkFilled();
            }
            PaymentsDataManager& pdm =
                manager->client_->GetPersonalDataManager()
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
            bam->FillOrPreviewField(
                mojom::ActionPersistence::kFill,
                mojom::FieldActionType::kReplaceSelectionForAtMemory, form_id,
                field_id, *unmasked_value, FillingProduct::kAtMemory,
                /*field_type_used=*/std::nullopt);
          },
          fill_weak_ptr_factory_.GetWeakPtr(),
          bam.GetBrowserAutofillManagerWeakPtr(), form_id, field_id, suggestion,
          std::move(metrics), identifier));
}

void AtMemoryManager::FillCreditCard(
    BrowserAutofillManager& bam,
    const std::string& guid,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    std::unique_ptr<AtMemoryMetricsRecorder> metrics) {
  CreditCardAccessManager* credit_card_access_manager =
      bam.GetCreditCardAccessManager();
  if (!credit_card_access_manager) {
    return;
  }

  const PersonalDataManager& pdm = client_->GetPersonalDataManager();
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
             base::WeakPtr<BrowserAutofillManager> bam,
             const FormGlobalId& form_id, const FieldGlobalId& field_id,
             const Suggestion& suggestion,
             std::unique_ptr<AtMemoryMetricsRecorder> metrics,
             const CreditCard& fetched_card) {
            if (!manager || !bam) {
              return;
            }
            if (metrics) {
              metrics->OnFetchPiiCompleted();
              metrics->MarkFilled();
            }
            manager->client_->GetPersonalDataManager()
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
            bam->FillOrPreviewField(
                mojom::ActionPersistence::kFill,
                mojom::FieldActionType::kReplaceSelectionForAtMemory, form_id,
                field_id, fill_value, FillingProduct::kAtMemory,
                /*field_type_used=*/std::nullopt);
          },
          fill_weak_ptr_factory_.GetWeakPtr(),
          bam.GetBrowserAutofillManagerWeakPtr(), form_id, field_id, suggestion,
          std::move(metrics)));
}

IsAsync AtMemoryManager::FillSensitivePersonalContextData(
    BrowserAutofillManager& bam,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    std::unique_ptr<AtMemoryMetricsRecorder> metrics) {
  AtMemoryQueryService* query_service = client_->GetAtMemoryQueryService();

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
      *client_,
      GetAuthenticationMessage(
          GetTargetFieldOrigin(target_field_origin(), *client_)),
      payload.value, payload.memory_data_type,
      GetMetadataFromSuggestion(suggestion),
      base::BindOnce(&AtMemoryManager::OnSensitivePersonalContextDataFetched,
                     fill_weak_ptr_factory_.GetWeakPtr(),
                     bam.GetBrowserAutofillManagerWeakPtr(), form_id, field_id,
                     std::move(metrics)));
  return IsAsync(true);
}

void AtMemoryManager::OnSensitivePersonalContextDataFetched(
    base::WeakPtr<BrowserAutofillManager> bam,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    std::unique_ptr<AtMemoryMetricsRecorder> metrics,
    AtMemoryQueryService::SpiiRetrievalResult result) {
  if (!bam) {
    return;
  }
  client_->HideSuggestions(SuggestionHidingReason::kAcceptSuggestion,
                           FillingProduct::kAtMemory);
  if (!result.has_value()) {
    if (metrics) {
      metrics->OnFetchPersonalContextPiiDataFailed(result.error());
    }
    std::optional<std::u16string> message_override;
    if (result.error() ==
        AtMemoryQueryService::SpiiRetrievalFailureReason::kReauthInProgress) {
      message_override = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AT_MEMORY_REAUTH_IN_PROGRESS_ERROR_NOTIFICATION);
    }
    client_->ShowAtMemoryFetchFailureNotification(std::move(message_override));
    return;
  }
  if (metrics) {
    metrics->OnFetchPiiCompleted();
    metrics->MarkFilled();
  }
    bam->FillOrPreviewField(
        mojom::ActionPersistence::kFill,
        mojom::FieldActionType::kReplaceSelectionForAtMemory, form_id, field_id,
        *result, FillingProduct::kAtMemory,
        /*field_type_used=*/std::nullopt);
}

IsAsync AtMemoryManager::FillSensitiveAutofillAiOrPersonalContextData(
    BrowserAutofillManager& bam,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    std::unique_ptr<AtMemoryMetricsRecorder> metrics) {
  const Suggestion::AtMemoryPayload& payload =
      suggestion.GetPayload<Suggestion::AtMemoryPayload>();

  if (payload.is_personal_context_sourced) {
    return FillSensitivePersonalContextData(bam, form_id, field_id, suggestion,
                                            std::move(metrics));
  } else if (const EntityInstance::EntityId* entity_id =
                 std::get_if<EntityInstance::EntityId>(&payload.identifier);
             entity_id) {
    std::optional<AttributeType> attribute_type =
        ToAttributeType(payload.memory_data_type);
    if (!attribute_type) {
      return IsAsync(false);
    }
    return FillSensitiveAutofillAiData(bam, *entity_id, form_id, field_id,
                                       suggestion, *attribute_type,
                                       std::move(metrics));
  }
  NOTREACHED();
}

IsAsync AtMemoryManager::FillSensitiveAutofillAiData(
    BrowserAutofillManager& bam,
    const EntityInstance::EntityId& entity_id,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    AttributeType attribute_type,
    std::unique_ptr<AtMemoryMetricsRecorder> metrics) {
  EntityDataManager* entity_data_manager = client_->GetEntityDataManager();
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

  // TODO(crbug.com/c/536814322): Show loading dialog on Android after
  // successful authentication.
  return IsAsync(bam.GetAutofillAiAccessManager().FetchEntityInstance(
      *entity, /*will_fill_sensitive_info=*/true,
      GetTargetFieldOrigin(target_field_origin(), *client_), base::DoNothing(),
      base::BindOnce(&AtMemoryManager::OnAutofillAiFetched,
                     fill_weak_ptr_factory_.GetWeakPtr(),
                     bam.GetBrowserAutofillManagerWeakPtr(), form_id, field_id,
                     suggestion, attribute_type, std::move(metrics))));
}

void AtMemoryManager::OnAutofillAiFetched(
    base::WeakPtr<BrowserAutofillManager> bam,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const Suggestion& suggestion,
    AttributeType attribute_type,
    std::unique_ptr<AtMemoryMetricsRecorder> metrics,
    base::expected<EntityInstance, AutofillAiAccessManager::FailureReason>
        result,
    bool reauth_attempted,
    bool did_fetch_from_server) {
  if (!bam) {
    return;
  }
  client_->HideSuggestions(SuggestionHidingReason::kAcceptSuggestion,
                           FillingProduct::kAtMemory);
  if (!result.has_value()) {
    if (result.error() ==
        AutofillAiAccessManager::FailureReason::kFetchFailed) {
      client_->ShowAutofillAiFetchEntityFailureNotification();
    }
    return;
  }

  const EntityInstance& fetched_entity = result.value();

  std::optional<std::u16string> attribute_fill_value = GetAttributeFillValue(
      fetched_entity, attribute_type, form_id, field_id, *bam);
  if (!attribute_fill_value) {
    return;
  }

  if (metrics) {
    metrics->OnFetchPiiCompleted();
    metrics->MarkFilled();
  }

  if (EntityDataManager* edm = client_->GetEntityDataManager()) {
    edm->RecordEntityUsed(fetched_entity.guid(), base::Time::Now());
  }

  bam->FillOrPreviewField(mojom::ActionPersistence::kFill,
                          mojom::FieldActionType::kReplaceSelectionForAtMemory,
                          form_id, field_id, std::move(*attribute_fill_value),
                          FillingProduct::kAtMemory,
                          /*field_type_used=*/std::nullopt);
}

}  // namespace autofill
