// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_SUGGESTION_PROPERTIES_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_SUGGESTION_PROPERTIES_H_

#include "components/autofill/core/common/aliases.h"
#include "third_party/blink/public/web/web_form_control_element.h"

namespace autofill {

// The following functions define properties of Autofill suggestions based on
// the trigger source and the triggering WebFormControlElement.
// They are only applicable for trigger sources that trigger through the
// renderer. When suggestions are updated, the suggestion properties are
// irrelevant, since the decision to show suggestions was already made.

// Specifies if suggestions should be shown when the triggering element contains
// no text.
bool ShouldAutofillOnEmptyValues(
    AutofillSuggestionTriggerSource trigger_source);

// Returns whether to query Autofill (i.e. call `AskForValuesToFill`) for values
// that exceed `autofill::kMaxStringLength`.
bool ShouldAutofillOnLongValues(AutofillSuggestionTriggerSource trigger_source);

// Specifies if suggestions should be shown when the caret is not after the last
// character of the triggering element.
bool RequiresCaretAtEnd(AutofillSuggestionTriggerSource trigger_source);

// Specifies if all suggestions should be shown and none should be elided
// because of the current value of triggering element.
// This is only used by the password manager.
bool ShouldShowFullSuggestionListForPasswordManager(
    AutofillSuggestionTriggerSource trigger_source,
    const blink::WebFormControlElement& element);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_SUGGESTION_PROPERTIES_H_
