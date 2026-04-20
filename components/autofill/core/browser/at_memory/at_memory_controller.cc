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
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
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

Suggestion TransformResultIntoSuggestion(
    const accessibility_annotator::MemorySearchResult& entry) {
  Suggestion suggestion(entry.value, SuggestionType::kAtMemorySearchResult);
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
  suggestion.payload = Suggestion::AtMemoryPayload(
      entry.value,
      entry.is_obfuscated ? entry.reveal_callback : base::NullCallback());
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
    child.payload =
        Suggestion::AtMemoryPayload(metadata.value, base::NullCallback());
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

}  // namespace

AtMemoryController::AtMemoryController(BrowserAutofillManager& manager)
    : manager_(manager) {}

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
  const std::u16string& replacement =
      suggestion.GetPayload<Suggestion::AtMemoryPayload>().value;

  if (action_persistence == mojom::ActionPersistence::kFill &&
      at_memory_funnel_metrics_) {
    at_memory_funnel_metrics_->OnSuggestionAccepted();
  }

  manager_->FillOrPreviewField(
      action_persistence, mojom::FieldActionType::kReplaceAtMemoryTrigger, form,
      field, replacement, suggestion.type, /*field_type_used=*/std::nullopt);
}

void AtMemoryController::ExecuteQuery(const std::u16string& filter,
                                      bool full_search) {
  accessibility_annotator::AccessibilityQueryService* query_service =
      manager_->client().GetAccessibilityQueryService();
  if (!query_service || !IsAtMemoryTriggerSource(trigger_source_) ||
      !update_callback_) {
    return;
  }

  if (filter.empty()) {
    update_callback_.Run({}, trigger_source_);
    return;
  }

  query_service->Query(
      filter, full_search,
      base::BindRepeating(&AtMemoryController::OnSearchResultsReceived,
                          weak_ptr_factory_.GetWeakPtr()));
}

void AtMemoryController::OnSearchResultsReceived(
    accessibility_annotator::MemorySearchResults result) {
  if (!IsAtMemoryTriggerSource(trigger_source_) || !update_callback_) {
    return;
  }

  update_callback_.Run(
      base::ToVector(result.entries, TransformResultIntoSuggestion),
      trigger_source_);
}

}  // namespace autofill
