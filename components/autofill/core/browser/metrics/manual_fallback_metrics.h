// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_MANUAL_FALLBACK_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_MANUAL_FALLBACK_METRICS_H_

#include <string_view>
#include "components/autofill/core/browser/filling_product.h"

namespace autofill::autofill_metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutofillAddNewAddressPromptOutcome {
  kSaved = 0,
  kCanceled = 1,
  kMaxValue = kCanceled
};

// Called when the "Add new address" prompt (triggered from the context menu
// when there are no addresses saved) gets a decision from the user.
void LogAddNewAddressPromptOutcome(AutofillAddNewAddressPromptOutcome outcome);

// Metrics logger when autofill is triggered from either an unclassified field
// or a field that does not match the target `FillingProduct`, for instance when
// an user uses address fallback on a field classified as credit card. Like
// other form event loggers, the lifetime of this class is attached to that of
// the BrowserAutofillManager. It collects events until it is destroyed, at
// which point metrics are emitted. Context menu related tests are in
// autofill_context_menu_manager_browsertest.cc.
class ManualFallbackEventLogger {
 public:
  // Emits metrics before destruction.
  ~ManualFallbackEventLogger();

  // Called when a suggestion is shown on an unclassified field or a field that
  // does not match the `target_filling_product`.
  void OnDidShowSuggestions(FillingProduct target_filling_product);

  // Called when a suggestion is triggered from an unclassified field or a field
  // that does not match the `target_filling_product`
  void OnDidFillSuggestion(FillingProduct target_filling_product);

  // Called when context menu was opened on a qualifying field.
  // `target_filling_product` indicates which of the available options was
  // shown.
  void ContextMenuEntryShown(FillingProduct target_filling_product);

  // Called when a fallback option was accepted (not just hovered).
  // `target_filling_product` specifies which of the available options was
  // chosen.
  void ContextMenuEntryAccepted(FillingProduct target_filling_product);

 private:
  enum class ContextMenuEntryState { kNotShown = 0, kShown = 1, kAccepted = 2 };
  enum class SuggestionState { kNotShown = 0, kShown = 1, kFilled = 2 };

  // Tries to change `old_state `to `new_state`. The context menu
  // state should always be updated in the following order: `kNotShown` ->
  // `kShown` -> `kAccepted`. Jumping over states (i.e. `kNotShown` ->
  // 'kAccepted`) is not allowed. Trying to "decrease" the state (i.e.
  // `kAccepted` -> `kShown`) is not possible.
  // Note that a user can accept a context menu entry and then, on the same
  // page, open another context menu. In this scenario, the code will try to
  // change the state from `kAccepted` to `kShown`. This method will be called,
  // but will not make the change.
  static void UpdateContextMenuEntryState(ContextMenuEntryState new_state,
                                          ContextMenuEntryState& old_state);

  // Updates the `SuggestionState` corresponding to `filling_product` to
  // `new_state`.
  void UpdateSuggestionStateForFillingProduct(FillingProduct filling_product,
                                              SuggestionState new_state);

  // If according to the `state` the context menu was used, emits into the
  // `bucket` (address or credit_card) whether an entry was accepted or not.
  void EmitExplicitlyTriggeredMetric(ContextMenuEntryState state,
                                     std::string_view bucket);

  // If suggestions for an unclassified field or a field that has a different
  // classification from the target `FillingProduct` were shown, emits whether
  // they were filled based on their respective `suggestion_state_`.
  // `bucket` defines the `FillingProduct`, i.e. address or credit_cards.
  void EmitFillAfterSuggestionMetric(SuggestionState suggestion_state,
                                     std::string_view bucket);

  // For addresses and credit cards filling, tracks if the manual fallback
  // context menu entry was shown or accepted.
  ContextMenuEntryState address_context_menu_state_ =
      ContextMenuEntryState::kNotShown;
  ContextMenuEntryState credit_card_context_menu_state_ =
      ContextMenuEntryState::kNotShown;

  // Tracks if address suggestions were shown/filled.
  SuggestionState address_suggestions_state_ = SuggestionState::kNotShown;
  // Tracks if credit card suggestions were shown/filled.
  SuggestionState credit_card_suggestions_state_ = SuggestionState::kNotShown;
};

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_MANUAL_FALLBACK_METRICS_H_
