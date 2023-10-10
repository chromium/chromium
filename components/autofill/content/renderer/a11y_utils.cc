// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/a11y_utils.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_input_element.h"

namespace autofill {

void SetAutofillState(const blink::WebInputElement& element,
                      mojom::AutofillState state) {
  if (!element.IsNull()) {
    auto to_blink_enum = [](mojom::AutofillState state) {
      switch (state) {
        case mojom::AutofillState::kAutofillAvailable:
          return blink::WebAXAutofillState::kAutofillAvailable;
        case mojom::AutofillState::kAutocompleteAvailable:
          return blink::WebAXAutofillState::kAutocompleteAvailable;
        case mojom::AutofillState::kNoSuggestions:
          return blink::WebAXAutofillState::kNoSuggestions;
      }
      NOTREACHED_NORETURN();
    };
    blink::WebAXObject::FromWebNode(element).HandleAutofillStateChanged(
        to_blink_enum(state));
  }
}
}  // namespace autofill
