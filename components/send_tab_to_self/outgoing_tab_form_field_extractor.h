// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_OUTGOING_TAB_FORM_FIELD_EXTRACTOR_H_
#define COMPONENTS_SEND_TAB_TO_SELF_OUTGOING_TAB_FORM_FIELD_EXTRACTOR_H_

#include <iosfwd>

#include "components/send_tab_to_self/page_context.h"

namespace autofill {
class AutofillManager;
}  // namespace autofill

namespace url {
class Origin;
}  // namespace url

namespace send_tab_to_self {

// Helper function to extract form field values from an AutofillManager
// into a FormFieldInfo, restricted to a specific `origin`.
PageContext::FormFieldInfo ExtractOutgoingTabFormFields(
    autofill::AutofillManager& manager,
    const url::Origin& origin);

// Similar to ExtractOutgoingTabFormFields, but allows injecting an ostream for
// detailed insights of the extraction process.
PageContext::FormFieldInfo ExtractOutgoingTabFormFieldsForTesting(
    autofill::AutofillManager& manager,
    const url::Origin& origin,
    std::ostream& os);

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_OUTGOING_TAB_FORM_FIELD_EXTRACTOR_H_
