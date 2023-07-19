// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FALLBACK_AUTOCOMPLETE_UNRECOGNIZED_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FALLBACK_AUTOCOMPLETE_UNRECOGNIZED_METRICS_H_

namespace autofill::autofill_metrics {

// The lifetime of this class is attached to that of the
// `AutofillContextMenuManager`. As such, a separate instance exists per
// right-click.
// Tested by autofill_context_menu_manager_unittest.cc.
class AutocompleteUnrecognizedFallbackMetricLogger {
 public:
  // Called when context menu was opened on a qualifying field.
  // `field_has_ac_unrecognized` indicates if the field that was right-clicked
  // on has an unrecognized autocomplete attribute (when
  // `kAutofillFallForAutocompleteUnrecognizedOnAllAddressField` is enabled,
  // this is not necessarily true).
  void ContextMenuEntryShown(bool field_has_ac_unrecognized);

  // Called when the fallback entry was accepted (not just hovered).
  void ContextMenuEntryAccepted();

  // Called when the context menu closes, independently of whether the fallback
  // entry was shown in the context menu.
  void ContextMenuClosed();

 private:
  enum class State {
    kFallbackNotShown = 0,
    kShownOnAutocompleteRecognizedField = 1,
    kShownOnAutocompleteUnrecognizedField = 2,
  } state_ = State::kFallbackNotShown;

  bool was_accepted_ = false;
};

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FALLBACK_AUTOCOMPLETE_UNRECOGNIZED_METRICS_H_
