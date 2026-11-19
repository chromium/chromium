// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_suggestion_controller_ios.h"

#import <algorithm>
#import <utility>

#import "components/autofill/core/browser/metrics/autofill_metrics.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#import "components/autofill/core/browser/ui/popup_interaction.h"
#import "components/autofill/core/common/unique_ids.h"

namespace autofill {

AutofillClient::SuggestionUiSessionId GenerateSuggestionUiSessionId() {
  static AutofillClient::SuggestionUiSessionId::Generator generator;
  return generator.GenerateNextId();
}

AutofillSuggestionControllerIOS::AutofillSuggestionControllerIOS() = default;

AutofillSuggestionControllerIOS::~AutofillSuggestionControllerIOS() {
  if (is_showing_) {
    Hide(SuggestionHidingReason::kViewDestroyed);
  }
}

AutofillSuggestionControllerIOS::UiSessionId
AutofillSuggestionControllerIOS::Show(
    std::vector<Suggestion> suggestions,
    AutofillSuggestionTriggerSource trigger_source,
    base::WeakPtr<AutofillSuggestionDelegate> delegate,
    FormGlobalId form_id,
    FieldGlobalId field_id,
    UiSessionId session_id) {
  if (suggestions.empty()) {
    Hide(SuggestionHidingReason::kNoSuggestions);
    return session_id;
  }

  const bool is_same_field =
      is_showing_ && form_id_ == form_id && field_id_ == field_id;
  const bool is_same_session_target =
      is_same_field && delegate_.get() == delegate.get();
  if (is_showing_ && !is_same_session_target) {
    AutofillMetrics::LogAutofillSuggestionHidingReason(
        suggestions_filling_product_, SuggestionHidingReason::kViewDestroyed);
    if (delegate_) {
      // Close the previous session on its delegate before it is replaced. The
      // UI isn't hidden as it immediately shows the new suggestions.
      delegate_->OnSuggestionsHidden(SuggestionHidingReason::kViewDestroyed);
    }
  }

  ui_session_id_ = session_id;
  delegate_ = delegate;
  form_id_ = form_id;
  field_id_ = field_id;
  is_showing_ = true;

  SetSuggestionsAndNotifyDelegate(std::move(suggestions), trigger_source);

  AutofillMetrics::LogPopupInteraction(suggestions_filling_product_,
                                       /*popup_level=*/0,
                                       PopupInteraction::kPopupShown);
  return session_id;
}

void AutofillSuggestionControllerIOS::UpdateSuggestions(
    std::vector<Suggestion> suggestions,
    FillingProduct main_filling_product,
    AutofillSuggestionTriggerSource trigger_source) {
  if (!is_showing_ || !delegate_) {
    return;
  }
  // Dismiss the suggestions when an update arrives for a different filling
  // product than the one being shown, because the showing suggestions no
  // longer match the delegate's state.
  if (main_filling_product != suggestions_filling_product_) {
    Hide(SuggestionHidingReason::kStaleData);
    return;
  }
  if (suggestions.empty()) {
    Hide(SuggestionHidingReason::kNoSuggestions);
    return;
  }
  SetSuggestionsAndNotifyDelegate(std::move(suggestions), trigger_source);
}

void AutofillSuggestionControllerIOS::Hide(SuggestionHidingReason reason) {
  if (!is_showing_) {
    return;
  }

  if (delegate_) {
    delegate_->OnSuggestionsHidden(reason);
  }

  AutofillMetrics::LogAutofillSuggestionHidingReason(
      suggestions_filling_product_, reason);

  is_showing_ = false;
  ui_session_id_.reset();
  suggestions_.clear();
  suggestions_filling_product_ = FillingProduct::kNone;
  trigger_source_ = AutofillSuggestionTriggerSource::kUnspecified;
  form_id_ = FormGlobalId();
  field_id_ = FieldGlobalId();
  delegate_.reset();
}

bool AutofillSuggestionControllerIOS::IsShowingForFrame(
    LocalFrameToken frame_token) const {
  if (!is_showing_ || frame_token.is_empty()) {
    return false;
  }
  // Match both the field's frame and its owning form's frame so cross-frame
  // forms (where a child-frame field belongs to an ancestor frame's form) are
  // considered showing for either frame.
  return frame_token == field_id_.frame_token ||
         frame_token == form_id_.frame_token;
}

FillingProduct AutofillSuggestionControllerIOS::GetMainFillingProduct() const {
  return suggestions_filling_product_;
}

std::optional<AutofillSuggestionControllerIOS::UiSessionId>
AutofillSuggestionControllerIOS::GetUiSessionId() const {
  return is_showing_ ? ui_session_id_ : std::nullopt;
}

base::WeakPtr<AutofillSuggestionDelegate>
AutofillSuggestionControllerIOS::GetDelegate() const {
  return delegate_;
}

void AutofillSuggestionControllerIOS::SetSuggestionsAndNotifyDelegate(
    std::vector<Suggestion> suggestions,
    AutofillSuggestionTriggerSource trigger_source) {
  suggestions_ = std::move(suggestions);
  OrderSuggestions();
  trigger_source_ = trigger_source;
  UpdateFillingProduct();
  if (delegate_) {
    delegate_->OnSuggestionsShown(
        suggestions_, AutofillSuggestionDelegate::SuggestionUiMetadata{});
  }
}

void AutofillSuggestionControllerIOS::UpdateFillingProduct() {
  if (delegate_) {
    suggestions_filling_product_ = delegate_->GetMainFillingProduct();
  } else if (!suggestions_.empty()) {
    suggestions_filling_product_ =
        GetFillingProductFromSuggestionType(suggestions_[0].type);
  } else {
    suggestions_filling_product_ = FillingProduct::kNone;
  }
}

void AutofillSuggestionControllerIOS::OrderSuggestions() {
  // Moves the Undo suggestion, if any, to the front of `suggestions_` so it
  // appears first in the keyboard accessory.
  std::ranges::stable_partition(suggestions_, [](const Suggestion& suggestion) {
    return suggestion.type == SuggestionType::kUndo;
  });
}

}  // namespace autofill
