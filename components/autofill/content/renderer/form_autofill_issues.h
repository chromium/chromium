// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_AUTOFILL_ISSUES_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_AUTOFILL_ISSUES_H_

#include "components/autofill/core/common/form_field_data.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_autofill_client.h"

namespace blink {
class WebFormControlElement;
class WebDocument;
class WebLocalFrame;
}  // namespace blink

// Responsible for processing form issues that are emitted to devtools.
// TODO(crbug.com/1399414): `GetFormIssues()` and
// `CheckForLabelsWithIncorrectForAttribute()` are not used outside this class.
// Move them inside the unnamed namespace.
namespace autofill::form_issues {

// Looks for form issues in `control_elements`, e.g., inputs with duplicate ids
// and returns a vector that is the union of `form_issues` and the new issues
// found.
std::vector<blink::WebAutofillClient::FormIssue> GetFormIssues(
    const blink::WebVector<blink::WebFormControlElement>& control_elements,
    std::vector<blink::WebAutofillClient::FormIssue> form_issues);

// Method specific to find issues regarding label `for` attribute. This needs to
// be called after label extraction. Similar to `GetFormIssues` it returns
// a vector that is the union of `form_issues` and the new issues found.
std::vector<blink::WebAutofillClient::FormIssue>
CheckForLabelsWithIncorrectForAttribute(
    const blink::WebDocument& document,
    const std::vector<FormFieldData>& fields,
    std::vector<blink::WebAutofillClient::FormIssue> form_issues);

// Given a `render_frame` and a list of `FormData` associated with it, emits
// the `FormIssue`(s) found.
// `web_local_frame` is used to find the input elements in the document.
// `forms` contains information about the parsed `FormFieldData` which holds
// label parsing details used to emit issues. See
// `CheckForLabelsWithIncorrectForAttribute()`.
// TODO(crbug.com/1399414): Once issues are only emitted when devtools is open,
// consider re-extracting labels inside `MaybeEmitFormIssuesToDevtools()` not to
// have to pass `forms`.
void MaybeEmitFormIssuesToDevtools(blink::WebLocalFrame& web_local_frame,
                                   base::span<const FormData> forms);

}  // namespace autofill::form_issues

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_AUTOFILL_ISSUES_H_
