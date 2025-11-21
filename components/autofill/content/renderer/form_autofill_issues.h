// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_AUTOFILL_ISSUES_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_AUTOFILL_ISSUES_H_

#include <vector>

#include "components/autofill/core/common/form_field_data.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/platform/web_string.h"

namespace blink {
class WebFormControlElement;
class WebDocument;
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

// The default emitter used by EmitFormIssues().
//
// `EmitFormIssues(document, forms, &EmitToDevTools)` must be called only if
// `document.GetFrame()->IsInspectorConnected()`.
void EmitToDevTools(const blink::WebDocument& document,
                    blink::mojom::GenericIssueErrorType issue_type,
                    int violating_node,
                    blink::WebString violating_node_attribute);

// Emits DevTools issues about invalid or suboptimal forms (e.g., invalid
// autocomplete attributes).
//
// `forms` contains information about the parsed `FormFieldData` which holds
// label parsing details used to emit issues. See
// `CheckForLabelsWithIncorrectForAttribute()`.
//
// `emit` is called for each found issue. It is a parameter for unit testing.
//
// TODO(crbug.com/40249826): Now that issues are only emitted when devtools is
// open, consider re-extracting labels inside `EmitFormIssues()`
// to avoid having to pass `forms`.
void EmitFormIssues(
    const blink::WebDocument& document,
    base::span<const FormData> forms,
    base::FunctionRef<void(const blink::WebDocument& document,
                           blink::mojom::GenericIssueErrorType issue_type,
                           int violating_node,
                           blink::WebString violating_node_attribute)> emit);

std::vector<FormIssue> GetFormIssuesForTesting(
    const std::vector<blink::WebFormControlElement>& control_elements,
    std::vector<FormIssue> form_issues);

std::vector<FormIssue> CheckForLabelsWithIncorrectForAttributeForTesting(
    const blink::WebDocument& document,
    const std::vector<FormFieldData>& fields,
    std::vector<FormIssue> form_issues);

}  // namespace autofill::form_issues

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_AUTOFILL_ISSUES_H_
