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
#include "base/strings/utf_string_conversions.h"
#include "components/accessibility_annotator/core/accessibility_query_service.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/autofill/core/browser/at_memory/at_memory_funnel_metrics.h"
#include "components/autofill/core/browser/data_model/autofill_ai/from_accessibility_annotator.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"

namespace autofill {

namespace {

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
