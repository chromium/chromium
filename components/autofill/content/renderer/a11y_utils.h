// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_A11Y_UTILS_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_A11Y_UTILS_H_

#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "third_party/blink/public/web/web_input_element.h"

namespace autofill {

// Sets corresponding blink's AutofillSuggestionAvailability enum value on an
// `element`.
void SetAutofillSuggestionAvailability(
    const blink::WebInputElement& element,
    mojom::AutofillSuggestionAvailability suggestion_availability);

}  // namespace autofill

#endif  //  COMPONENTS_AUTOFILL_CONTENT_RENDERER_A11Y_UTILS_H_
