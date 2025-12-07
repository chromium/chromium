// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_autofill_issues.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_label_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebLabelElement;
using blink::WebLocalFrame;
using blink::WebString;
using blink::mojom::GenericIssueErrorType;

namespace autofill::form_issues {

namespace {

constexpr size_t kMaxNumberOfDevtoolsIssuesEmitted = 100;

constexpr std::string_view kFor = "for";
constexpr std::string_view kAriaLabelledBy = "aria-labelledby";
constexpr std::string_view kName = "name";
constexpr std::string_view kId = "id";
constexpr std::string_view kLabel = "label";
constexpr std::string_view kAutocomplete = "autocomplete";

using EmitCallback =
    base::FunctionRef<void(const WebDocument& document,
                           GenericIssueErrorType issue_type,
                           int violating_node,
                           WebString violating_node_attribute)>;

void EmitLabelWithoutControlDevtoolsIssue(const WebDocument& document,
                                          WebLabelElement label,
                                          EmitCallback emit) {
  if (label.CorrespondingControl()) {
    return;
  }

  const WebString for_attr = WebString::FromUTF8(kFor);
  if (!label.HasAttribute(for_attr)) {
    // Label has neither for attribute nor a control element was found.
    emit(document,
         GenericIssueErrorType::kFormLabelHasNeitherForNorNestedInputError,
         label.GetDomNodeId(), {});
  }
}

void EmitAriaLabelledByDevtoolsIssue(const WebDocument& document,
                                     const WebElement& element,
                                     EmitCallback emit) {
  const WebString aria_label_attr = WebString::FromUTF8(kAriaLabelledBy);
  if (std::ranges::any_of(
          base::SplitStringPiece(element.GetAttribute(aria_label_attr).Utf16(),
                                 base::kWhitespaceUTF16, base::KEEP_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY),
          [&](const auto& id) {
            return !element.GetDocument().GetElementById(WebString(id));
          })) {
    emit(document,
         GenericIssueErrorType::kFormAriaLabelledByToNonExistingIdError,
         element.GetDomNodeId(), aria_label_attr);
  }
}

void EmitInputWithEmptyIdAndNameDevtoolsIssue(
    const WebDocument& document,
    const WebFormControlElement& element,
    EmitCallback emit) {
  const WebString name_attr = WebString::FromUTF8(kName);
  if (element.GetAttribute(name_attr).IsEmpty() &&
      element.GetIdAttribute().IsEmpty()) {
    emit(document,
         GenericIssueErrorType::kFormEmptyIdAndNameAttributesForInputError,
         element.GetDomNodeId(), {});
  }
}

int GetShadowHostDOMNodeId(const WebFormControlElement& element) {
  WebElement host = element.OwnerShadowHost();
  if (!host) {
    return /*blink::kInvalidDOMNodeId*/ 0;
  }
  return host.GetDomNodeId();
}

void EmitDuplicateIdForInputDevtoolsIssue(
    const WebDocument& document,
    std::vector<WebFormControlElement> elements,
    EmitCallback emit) {
  const WebString id_attr = WebString::FromUTF8(kId);

  std::erase_if(elements, [](const WebFormControlElement& element) {
    return element.GetIdAttribute().IsEmpty();
  });
  std::ranges::sort(elements, [](const WebFormControlElement& a,
                                 const WebFormControlElement& b) {
    return std::forward_as_tuple(a.GetIdAttribute(),
                                 GetShadowHostDOMNodeId(a)) <
           std::forward_as_tuple(b.GetIdAttribute(), GetShadowHostDOMNodeId(b));
  });

  int previous_violating_node = 0;
  for (auto it = elements.begin();
       (it = std::ranges::adjacent_find(
            it, elements.end(),
            [](const WebFormControlElement& a, const WebFormControlElement& b) {
              return a.GetIdAttribute() == b.GetIdAttribute() &&
                     GetShadowHostDOMNodeId(a) == GetShadowHostDOMNodeId(b);
            })) != elements.end();
       it++) {
    if (previous_violating_node != it->GetDomNodeId()) {
      emit(document, GenericIssueErrorType::kFormDuplicateIdForInputError,
           it->GetDomNodeId(), id_attr);
    }
    emit(document, GenericIssueErrorType::kFormDuplicateIdForInputError,
         std::next(it)->GetDomNodeId(), id_attr);
    previous_violating_node = std::next(it)->GetDomNodeId();
  }
}

void EmitAutocompleteAttributeDevtoolsIssue(const WebDocument& document,
                                            const WebElement& element,
                                            EmitCallback emit) {
  const WebString autocomplete_attr = WebString::FromUTF8(kAutocomplete);
  std::string autocomplete_attribute =
      form_util::GetAutocompleteAttribute(element);
  if (element.HasAttribute(autocomplete_attr) &&
      autocomplete_attribute.empty()) {
    emit(document, GenericIssueErrorType::kFormAutocompleteAttributeEmptyError,
         element.GetDomNodeId(), autocomplete_attr);
  }

  if (IsAutocompleteTypeWrongButWellIntended(autocomplete_attribute)) {
    emit(document,
         GenericIssueErrorType::
             kFormInputHasWrongButWellIntendedAutocompleteValueError,
         element.GetDomNodeId(), autocomplete_attr);
  }
}

void EmitInputAssignedAutocompleteValueToIdOrNameAttributesDevtoolsIssue(
    const WebDocument& document,
    const WebFormControlElement& element,
    EmitCallback emit) {
  const WebString autocomplete_attr = WebString::FromUTF8(kAutocomplete);
  if (element.HasAttribute(autocomplete_attr)) {
    return;
  }

  auto ParsedHtmlAttributeValueToAutocompleteHasFieldType =
      [](const std::string& attribute_value) {
        std::optional<AutocompleteParsingResult>
            parsed_attribute_to_autocomplete =
                ParseAutocompleteAttribute(attribute_value);
        if (!parsed_attribute_to_autocomplete) {
          return false;
        }

        return parsed_attribute_to_autocomplete->field_type !=
                   HtmlFieldType::kUnspecified &&
               parsed_attribute_to_autocomplete->field_type !=
                   HtmlFieldType::kUnrecognized;
      };

  const WebString name_attr = WebString::FromUTF8(kName);
  bool name_attr_matches_autocomplete =
      ParsedHtmlAttributeValueToAutocompleteHasFieldType(
          element.GetAttribute(name_attr).Utf8());
  bool id_attr_matches_autocomplete =
      ParsedHtmlAttributeValueToAutocompleteHasFieldType(
          element.GetIdAttribute().Utf8());

  if (name_attr_matches_autocomplete || id_attr_matches_autocomplete) {
    WebString attribute_with_autocomplete_value =
        id_attr_matches_autocomplete ? WebString::FromUTF8(kId) : name_attr;
    emit(document,
         GenericIssueErrorType::
             kFormInputAssignedAutocompleteValueToIdOrNameAttributeError,
         element.GetDomNodeId(), attribute_with_autocomplete_value);

    return;
  }
}

void EmitFormControlIssues(const WebDocument& document,
                           std::vector<WebFormControlElement> elements,
                           EmitCallback emit) {
  if (elements.size() == 0) {
    return;
  }

  const WebString label_attr = WebString::FromUTF8(kLabel);
  WebElementCollection labels =
      elements[0].GetDocument().GetElementsByHTMLTagName(label_attr);
  CHECK(labels);

  for (WebElement item = labels.FirstItem(); item; item = labels.NextItem()) {
    WebLabelElement label = item.To<WebLabelElement>();
    EmitLabelWithoutControlDevtoolsIssue(document, label, emit);
  }

  for (const WebFormControlElement& element : elements) {
    EmitAriaLabelledByDevtoolsIssue(document, element, emit);
    EmitAutocompleteAttributeDevtoolsIssue(document, element, emit);
    EmitInputWithEmptyIdAndNameDevtoolsIssue(document, element, emit);
    EmitInputAssignedAutocompleteValueToIdOrNameAttributesDevtoolsIssue(
        document, element, emit);
  }

  EmitDuplicateIdForInputDevtoolsIssue(document, std::move(elements), emit);
}

// Checks for issues with the "for" attribute on <label> elements. This needs to
// be called after label extraction. Similar to `GetFormIssues` it returns a
// vector that is the union of `form_issues` and the new issues found.
void CheckForLabelsWithIncorrectForAttribute(
    const WebDocument& document,
    base::span<const FormFieldData> fields,
    EmitCallback emit) {
  const WebString for_attr = WebString::FromUTF8(kFor);
  const WebString label_attr = WebString::FromUTF8(kLabel);

  std::set<std::u16string> elements_whose_name_match_a_label_for_attr;
  for (const FormFieldData& field : fields) {
    if (field.label_source() == FormFieldData::LabelSource::kForName) {
      elements_whose_name_match_a_label_for_attr.insert(field.name_attribute());
    }
  }

  WebElementCollection labels = document.GetElementsByHTMLTagName(label_attr);
  for (WebElement item = labels.FirstItem(); item; item = labels.NextItem()) {
    WebLabelElement label = item.To<WebLabelElement>();
    if (label.CorrespondingControl() || !label.HasAttribute(for_attr)) {
      continue;
    }

    if (elements_whose_name_match_a_label_for_attr.contains(
            label.GetAttribute(for_attr).Utf16())) {
      // Add a DevTools issue informing the developer that the `label`'s for-
      // attribute is pointing to the name of a field, even though the ID
      // should be used.
      emit(document, GenericIssueErrorType::kFormLabelForNameError,
           label.GetDomNodeId(), for_attr);
    } else {
      // Label has for attribute but no labellable element whose id OR name
      // matches it.
      // This issue is not emitted in case an element has a name that matches
      // it, in this case we emit kFormLabelForNameError to educate developers
      // that labels should be linked to element ids.
      emit(document,
           GenericIssueErrorType::kFormLabelForMatchesNonExistingIdError,
           label.GetDomNodeId(), for_attr);
    }
  }
}

}  // namespace

void EmitToDevTools(const WebDocument& document,
                    GenericIssueErrorType issue_type,
                    int violating_node,
                    WebString violating_node_attribute) {
  WebLocalFrame& frame = CHECK_DEREF(document.GetFrame());
  CHECK(frame.IsInspectorConnected());
  frame.AddGenericIssue(issue_type, violating_node, violating_node_attribute);
}

void EmitFormIssues(const WebDocument& document,
                    base::span<const FormData> forms,
                    EmitCallback emit) {
  size_t counter = 0;
  auto emit_limited = [&emit, &counter](const WebDocument& document,
                                        GenericIssueErrorType issue_type,
                                        int violating_node,
                                        WebString violating_node_attribute) {
    if (++counter > kMaxNumberOfDevtoolsIssuesEmitted) {
      return;
    }
    emit(document, issue_type, violating_node, violating_node_attribute);
  };

  // Get issues from forms input elements.
  for (const WebFormElement& form_element : document.GetTopLevelForms()) {
    EmitFormControlIssues(
        document,
        form_util::GetOwnedAutofillableFormControls(document, form_element),
        emit_limited);
  }
  // Get issues from input elements that belong to no form.
  EmitFormControlIssues(
      document,
      form_util::GetOwnedAutofillableFormControls(document, WebFormElement()),
      emit_limited);
  // Look for fields that after parsed were found to have labels incorrectly
  // used.
  for (const FormData& form : forms) {
    CheckForLabelsWithIncorrectForAttribute(document, form.fields(),
                                            emit_limited);
  }
}

}  // namespace autofill::form_issues
