// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FALLBACK_AUTOCOMPLETE_UNRECOGNIZED_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FALLBACK_AUTOCOMPLETE_UNRECOGNIZED_METRICS_H_

#include <string_view>

namespace autofill::autofill_metrics {

// Metrics logger for autocomplete=unrecognized fallback related events.
// Like other form event loggers, the lifetime of this class is attached to that
// of BrowserAutofillManager. It collects events until it is destroyed, at
// which point metrics are emitted.
// Context menu related tests are in
// autofill_context_menu_manager_browsertest.cc.
class AutocompleteUnrecognizedFallbackEventLogger {
 public:
  // Emits metrics before destruction.
  ~AutocompleteUnrecognizedFallbackEventLogger();

  // Called when a suggestion is shown on an ac=unrecognized field.
  void OnDidShowSuggestions();

  // Called when a suggestion triggered from an ac=unrecognized field is filled.
  void OnDidFillFormFillingSuggestion();

  // Called when context menu was opened on a qualifying field.
  // `address_field_has_ac_unrecognized` indicates if the field that was right
  // clicked on has an unrecognized autocomplete attribute (when
  // `kAutofillFallForAutocompleteUnrecognizedOnAllAddressField` is enabled,
  // this is not necessarily true).
  // Generally, this is only relevant for address fields, since no manual
  // fallbacks exist for payment forms.
  void ContextMenuEntryShown(bool address_field_has_ac_unrecognized);

  // Called when the fallback entry was accepted (not just hovered).
  // `field_has_ac_unrecognized`'s meaning matches `ContextMenuEntryShown()`.
  void ContextMenuEntryAccepted(bool address_field_has_ac_unrecognized);

 private:
  enum class ContextMenuEntryState { kNotShown = 0, kShown = 1, kAccepted = 2 };
  enum class SuggestionState { kNotShown = 0, kShown = 1, kFilled = 2 };

  // If the context menu was used according to the `state`, emits whether the
  // entry was accepted or not into the explicit triggering metric of the given
  // `bucket` (ac recognized or unrecognized).
  void EmitExplicitlyTriggeredMetric(ContextMenuEntryState state,
                                     std::string_view bucket);

  // If suggestions for an ac=unrecognized field were shown, emits whether they
  // were excepted based on the `suggestion_state_`.
  void EmitFillAfterSuggestionMetric();

  // Tracks if the manual fallback context menu entry was shown or accepted.
  // Since the metric is split by the triggering field's autocomplete attribute,
  // this is tracked twice.
  ContextMenuEntryState ac_unrecognized_context_menu_state_ =
      ContextMenuEntryState::kNotShown;
  ContextMenuEntryState ac_recognized_context_menu_state_ =
      ContextMenuEntryState::kNotShown;

  // Tracks if suggestions on an ac=unrecognized field were shown/filled.
  SuggestionState suggestion_state_ = SuggestionState::kNotShown;
};

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FALLBACK_AUTOCOMPLETE_UNRECOGNIZED_METRICS_H_
