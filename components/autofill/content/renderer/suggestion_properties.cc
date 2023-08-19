// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/suggestion_properties.h"

#include "base/notreached.h"

using blink::WebFormControlElement;

namespace autofill {

// The following functions define properties of AutofillSuggestions based
// on the trigger source.
bool ShouldAutofillOnEmptyValues(
    AutofillSuggestionTriggerSource trigger_source) {
  switch (trigger_source) {
    case AutofillSuggestionTriggerSource::kFormControlElementClicked:
    case AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown:
    case AutofillSuggestionTriggerSource::kOpenTextDataListChooser:
    case AutofillSuggestionTriggerSource::
        kManualFallbackForAutocompleteUnrecognized:
      return true;
    case AutofillSuggestionTriggerSource::kTextFieldDidChange:
      return false;
    // `kShowCardsFromAccount`, `kPasswordManager`, `kAndroidWebView` and `kiOS`
    // are not used in the renderer code. As such, suggestion properties don't
    // apply to them.
    case AutofillSuggestionTriggerSource::kShowCardsFromAccount:
    case mojom::AutofillSuggestionTriggerSource::kPasswordManager:
    case mojom::AutofillSuggestionTriggerSource::kAndroidWebView:
    case mojom::AutofillSuggestionTriggerSource::kiOS:
    case AutofillSuggestionTriggerSource::kUnspecified:
      break;
  }
  NOTREACHED_NORETURN();
}

bool RequiresCaretAtEnd(AutofillSuggestionTriggerSource trigger_source) {
  switch (trigger_source) {
    case AutofillSuggestionTriggerSource::kTextFieldDidChange:
    case AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown:
      return true;
    case AutofillSuggestionTriggerSource::kFormControlElementClicked:
    case AutofillSuggestionTriggerSource::kOpenTextDataListChooser:
    case AutofillSuggestionTriggerSource::
        kManualFallbackForAutocompleteUnrecognized:
      return false;
    // `kShowCardsFromAccount`, `kPasswordManager`, `kAndroidWebView` and `kiOS`
    // are not used in the renderer code. As such, suggestion properties don't
    // apply to them.
    case AutofillSuggestionTriggerSource::kShowCardsFromAccount:
    case mojom::AutofillSuggestionTriggerSource::kPasswordManager:
    case mojom::AutofillSuggestionTriggerSource::kAndroidWebView:
    case mojom::AutofillSuggestionTriggerSource::kiOS:
    case AutofillSuggestionTriggerSource::kUnspecified:
      break;
  }
  NOTREACHED_NORETURN();
}

bool ShouldShowFullSuggestionListForPasswordManager(
    AutofillSuggestionTriggerSource trigger_source,
    const WebFormControlElement& element) {
  switch (trigger_source) {
    case AutofillSuggestionTriggerSource::kFormControlElementClicked:
      // Even if the user has not edited an input element, it may still contain
      // a default value filled by the website. In that case, don't elide
      // suggestions that don't have a common prefix with the default value.
      return element.IsAutofilled() || !element.UserHasEditedTheField();
    case AutofillSuggestionTriggerSource::kTextFieldDidChange:
    case AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown:
    case AutofillSuggestionTriggerSource::kOpenTextDataListChooser:
    case AutofillSuggestionTriggerSource::
        kManualFallbackForAutocompleteUnrecognized:
      return false;
    // `kShowCardsFromAccount`, `kPasswordManager`, `kAndroidWebView` and `kiOS`
    // are not used in the renderer code. As such, suggestion properties don't
    // apply to them.
    // `kPasswordManager` specifically is used to identify password manager
    // suggestions in the browser process. In the renderer, the logic triggering
    // suggestions through Blink events is shared. Thus, the return values for
    // `kFormControlElementClicked` etc. matter for the password manager in the
    // renderer.
    case AutofillSuggestionTriggerSource::kShowCardsFromAccount:
    case mojom::AutofillSuggestionTriggerSource::kPasswordManager:
    case mojom::AutofillSuggestionTriggerSource::kAndroidWebView:
    case mojom::AutofillSuggestionTriggerSource::kiOS:
    case AutofillSuggestionTriggerSource::kUnspecified:
      break;
  }
  NOTREACHED_NORETURN();
}

}  // namespace autofill
