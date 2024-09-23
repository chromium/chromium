// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/a11y_utils.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_input_element.h"

namespace autofill {

void SetAutofillSuggestionAvailability(
    const blink::WebInputElement& element,
    mojom::AutofillSuggestionAvailability suggestion_availability) {
  if (element) {
    auto to_blink_enum = [](mojom::AutofillSuggestionAvailability
                                suggestion_availability) {
      switch (suggestion_availability) {
        case mojom::AutofillSuggestionAvailability::kAutofillAvailable:
          return blink::WebAXAutofillSuggestionAvailability::kAutofillAvailable;
        case mojom::AutofillSuggestionAvailability::kAutocompleteAvailable:
          return blink::WebAXAutofillSuggestionAvailability::
              kAutocompleteAvailable;
        case mojom::AutofillSuggestionAvailability::kNoSuggestions:
          return blink::WebAXAutofillSuggestionAvailability::kNoSuggestions;
      }
      NOTREACHED();
    };
    blink::WebAXObject::FromWebNode(element)
        .HandleAutofillSuggestionAvailabilityChanged(
            to_blink_enum(suggestion_availability));
  }
}
}  // namespace autofill
