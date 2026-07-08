// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/at_memory_bottom_sheet_delegate_android.h"

#include <utility>

#include "base/check_deref.h"
#include "base/numerics/safe_conversions.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"

namespace autofill {

AtMemoryBottomSheetDelegateAndroid::AtMemoryBottomSheetDelegateAndroid(
    AutofillClient* client,
    base::WeakPtr<AutofillSuggestionDelegate> delegate,
    std::vector<Suggestion> suggestions)
    : client_(&CHECK_DEREF(client)),
      delegate_(delegate),
      suggestions_(std::move(suggestions)) {}

AtMemoryBottomSheetDelegateAndroid::~AtMemoryBottomSheetDelegateAndroid() =
    default;

void AtMemoryBottomSheetDelegateAndroid::OnDismissed() {
  if (client_) {
    client_->HideSuggestions(SuggestionHidingReason::kUserAborted,
                             FillingProduct::kAtMemory);
  }
}

void AtMemoryBottomSheetDelegateAndroid::OnQuerySubmitted(
    const std::u16string& query) {
  if (delegate_) {
    delegate_->OnSearchSubmitted(query);
  }
}

void AtMemoryBottomSheetDelegateAndroid::OnQueryTextChanged(
    const std::u16string& query) {
  if (delegate_) {
    delegate_->OnFilterChanged(query);
  }
}

void AtMemoryBottomSheetDelegateAndroid::OnSuggestionSelected(int position) {
  if (position < 0 ||
      base::checked_cast<size_t>(position) >= suggestions_.size()) {
    return;
  }

  // Use a copy instead of a reference here. Under certain circumstances,
  // `DidAcceptSuggestion()` can trigger focus changes or hiding that
  // destroys `this` or overwrites `suggestions_`, invalidating the reference.
  Suggestion suggestion = suggestions_[position];
  if (delegate_) {
    delegate_->DidAcceptSuggestion(
        suggestion, AutofillSuggestionDelegate::SuggestionMetadata{
                        .multi_index = {static_cast<size_t>(position)}});
  }
}

void AtMemoryBottomSheetDelegateAndroid::OnChildSuggestionsShown(
    int parent_position) {
  if (parent_position < 0 ||
      base::checked_cast<size_t>(parent_position) >= suggestions_.size()) {
    return;
  }

  const Suggestion& parent_suggestion = suggestions_[parent_position];
  if (delegate_) {
    delegate_->OnSuggestionsShown(
        parent_suggestion.children,
        AutofillSuggestionDelegate::SuggestionMetadata{
            .multi_index = {static_cast<size_t>(parent_position)}});
  }
}

void AtMemoryBottomSheetDelegateAndroid::OnChildSuggestionSelected(
    int parent_position,
    int child_position) {
  if (parent_position < 0 ||
      base::checked_cast<size_t>(parent_position) >= suggestions_.size()) {
    return;
  }

  const Suggestion& parent_suggestion = suggestions_[parent_position];
  if (child_position < 0 || base::checked_cast<size_t>(child_position) >=
                                parent_suggestion.children.size()) {
    return;
  }

  // Use a copy instead of a reference here. Under certain circumstances,
  // `DidAcceptSuggestion()` can trigger focus changes or hiding that
  // destroys `this` or overwrites `suggestions_`, invalidating the reference.
  Suggestion suggestion = parent_suggestion.children[child_position];
  if (delegate_) {
    delegate_->DidAcceptSuggestion(
        suggestion, AutofillSuggestionDelegate::SuggestionMetadata{
                        .multi_index = {static_cast<size_t>(parent_position),
                                        static_cast<size_t>(child_position)}});
  }
}

bool AtMemoryBottomSheetDelegateAndroid::IsSearching() const {
  return delegate_ && delegate_->IsSearching();
}

}  // namespace autofill
