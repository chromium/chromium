// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_AUTOFILL_ISSUES_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_AUTOFILL_ISSUES_H_

#include <vector>

#include "components/autofill/core/common/form_field_data.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {
class WebFormControlElement;
class WebDocument;
class WebLocalFrame;
}  // namespace blink

// Responsible for processing form issues that are emitted to devtools.
namespace autofill::form_issues {

struct FormIssue {
  FormIssue(blink::mojom::GenericIssueErrorType type,
            int node,
            blink::WebString attribute)
      : issue_type(type),
        violating_node(node),
        violating_node_attribute(attribute) {}
  FormIssue(blink::mojom::GenericIssueErrorType type, int node)
      : issue_type(type), violating_node(node) {}

  blink::mojom::GenericIssueErrorType issue_type;
  int violating_node;
  blink::WebString violating_node_attribute;
};

// Given a `render_frame` and a list of `FormData` associated with it, emits
// the `FormIssue`(s) found.
// `web_local_frame` is used to find the input elements in the document.
// `forms` contains information about the parsed `FormFieldData` which holds
// label parsing details used to emit issues. See
// `CheckForLabelsWithIncorrectForAttribute()`.
// TODO(crbug.com/40249826): Once issues are only emitted when devtools is open,
// consider re-extracting labels inside `MaybeEmitFormIssuesToDevtools()` not to
// have to pass `forms`.
void MaybeEmitFormIssuesToDevtools(blink::WebLocalFrame& web_local_frame,
                                   base::span<const FormData> forms);

std::vector<FormIssue> GetFormIssuesForTesting(
    const blink::WebVector<blink::WebFormControlElement>& control_elements,
    std::vector<FormIssue> form_issues);

std::vector<FormIssue> CheckForLabelsWithIncorrectForAttributeForTesting(
    const blink::WebDocument& document,
    const std::vector<FormFieldData>& fields,
    std::vector<FormIssue> form_issues);

}  // namespace autofill::form_issues

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_AUTOFILL_ISSUES_H_
