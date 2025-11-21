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

// Wrapper for frequently used WebString constants.
template <const std::string_view& string>
const WebString& GetWebString() {
  static const base::NoDestructor<WebString> web_string(
      WebString::FromUTF8(string));
  return *web_string;
}

struct FormIssue {
  FormIssue(GenericIssueErrorType type, int node, blink::WebString attribute)
      : issue_type(type),
        violating_node(node),
        violating_node_attribute(attribute) {}
  FormIssue(GenericIssueErrorType type, int node)
      : issue_type(type), violating_node(node) {}

  GenericIssueErrorType issue_type;
  int violating_node;
  blink::WebString violating_node_attribute;
};

void MaybeAppendLabelWithoutControlDevtoolsIssue(
    WebLabelElement label,
    std::vector<FormIssue>& form_issues) {
  if (label.CorrespondingControl()) {
    return;
  }

  const WebString& for_attr = GetWebString<kFor>();
  if (!label.HasAttribute(for_attr)) {
    // Label has neither for attribute nor a control element was found.
    form_issues.emplace_back(
        GenericIssueErrorType::kFormLabelHasNeitherForNorNestedInputError,
        label.GetDomNodeId());
  }
}

void MaybeAppendAriaLabelledByDevtoolsIssue(
    const WebElement& element,
    std::vector<FormIssue>& form_issues) {
  const WebString& aria_label_attr = GetWebString<kAriaLabelledBy>();
  if (std::ranges::any_of(
          base::SplitStringPiece(element.GetAttribute(aria_label_attr).Utf16(),
                                 base::kWhitespaceUTF16, base::KEEP_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY),
          [&](const auto& id) {
            return !element.GetDocument().GetElementById(WebString(id));
          })) {
    form_issues.emplace_back(
        GenericIssueErrorType::kFormAriaLabelledByToNonExistingIdError,
        element.GetDomNodeId(), aria_label_attr);
  }
}

void MaybeAppendInputWithEmptyIdAndNameDevtoolsIssue(
    const WebFormControlElement& element,
    std::vector<FormIssue>& form_issues) {
  const WebString& name_attr = GetWebString<kName>();
  if (element.GetAttribute(name_attr).IsEmpty() &&
      element.GetIdAttribute().IsEmpty()) {
    form_issues.emplace_back(
        GenericIssueErrorType::kFormEmptyIdAndNameAttributesForInputError,
        element.GetDomNodeId());
  }
}

int GetShadowHostDOMNodeId(const WebFormControlElement& element) {
  WebElement host = element.OwnerShadowHost();
  if (!host) {
    return /*blink::kInvalidDOMNodeId*/ 0;
  }
  return host.GetDomNodeId();
}

void MaybeAppendDuplicateIdForInputDevtoolsIssue(
    base::span<const WebFormControlElement> elements,
    std::vector<FormIssue>& form_issues) {
  const WebString& id_attr = GetWebString<kId>();

  std::vector<WebFormControlElement> elements_with_id_attr;
  elements_with_id_attr.reserve(elements.size());
  for (const auto& element : elements) {
    if (!element.GetIdAttribute().IsEmpty()) {
      elements_with_id_attr.push_back(element);
    }
  }
  std::ranges::sort(elements_with_id_attr, [](const WebFormControlElement& a,
                                              const WebFormControlElement& b) {
    return std::forward_as_tuple(a.GetIdAttribute(),
                                 GetShadowHostDOMNodeId(a)) <
           std::forward_as_tuple(b.GetIdAttribute(), GetShadowHostDOMNodeId(b));
  });

  for (auto it = elements_with_id_attr.begin();
       (it = std::ranges::adjacent_find(
            it, elements_with_id_attr.end(),
            [](const WebFormControlElement& a, const WebFormControlElement& b) {
              return a.GetIdAttribute() == b.GetIdAttribute() &&
                     GetShadowHostDOMNodeId(a) == GetShadowHostDOMNodeId(b);
            })) != elements_with_id_attr.end();
       it++) {
    bool current_element_not_added =
        form_issues.empty() ||
        form_issues.back().issue_type !=
            GenericIssueErrorType::kFormDuplicateIdForInputError ||
        form_issues.back().violating_node != it->GetDomNodeId();

    if (current_element_not_added) {
      form_issues.emplace_back(
          GenericIssueErrorType::kFormDuplicateIdForInputError,
          it->GetDomNodeId(), id_attr);
    }
    form_issues.emplace_back(
        GenericIssueErrorType::kFormDuplicateIdForInputError,
        std::next(it)->GetDomNodeId(), id_attr);
  }
}

void MaybeAppendAutocompleteAttributeDevtoolsIssue(
    const WebElement& element,
    std::vector<FormIssue>& form_issues) {
  const WebString& autocomplete_attr = GetWebString<kAutocomplete>();
  std::string autocomplete_attribute =
      form_util::GetAutocompleteAttribute(element);
  if (element.HasAttribute(autocomplete_attr) &&
      autocomplete_attribute.empty()) {
    form_issues.emplace_back(
        GenericIssueErrorType::kFormAutocompleteAttributeEmptyError,
        element.GetDomNodeId(), autocomplete_attr);
  }

  if (IsAutocompleteTypeWrongButWellIntended(autocomplete_attribute)) {
    form_issues.emplace_back(
        GenericIssueErrorType::
            kFormInputHasWrongButWellIntendedAutocompleteValueError,
        element.GetDomNodeId(), autocomplete_attr);
  }
}

void MaybeAppendInputAssignedAutocompleteValueToIdOrNameAttributesDevtoolsIssue(
    const WebFormControlElement& element,
    std::vector<FormIssue>& form_issues) {
  const WebString& autocomplete_attr = GetWebString<kAutocomplete>();
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

  const WebString& name_attr = GetWebString<kName>();
  bool name_attr_matches_autocomplete =
      ParsedHtmlAttributeValueToAutocompleteHasFieldType(
          element.GetAttribute(name_attr).Utf8());
  bool id_attr_matches_autocomplete =
      ParsedHtmlAttributeValueToAutocompleteHasFieldType(
          element.GetIdAttribute().Utf8());

  if (name_attr_matches_autocomplete || id_attr_matches_autocomplete) {
    WebString attribute_with_autocomplete_value =
        id_attr_matches_autocomplete ? GetWebString<kId>() : name_attr;
    form_issues.emplace_back(
        GenericIssueErrorType::
            kFormInputAssignedAutocompleteValueToIdOrNameAttributeError,
        element.GetDomNodeId(), attribute_with_autocomplete_value);

    return;
  }
}

void AppendFormIssuesInternal(base::span<const WebFormControlElement> elements,
                              std::vector<FormIssue>& form_issues) {
  if (elements.size() == 0) {
    return;
  }

  const WebString& label_attr = GetWebString<kLabel>();
  WebElementCollection labels =
      elements[0].GetDocument().GetElementsByHTMLTagName(label_attr);
  CHECK(labels);

  for (WebElement item = labels.FirstItem(); item; item = labels.NextItem()) {
    WebLabelElement label = item.To<WebLabelElement>();
    MaybeAppendLabelWithoutControlDevtoolsIssue(label, form_issues);
  }

  MaybeAppendDuplicateIdForInputDevtoolsIssue(elements, form_issues);
  for (const WebFormControlElement& element : elements) {
    MaybeAppendAriaLabelledByDevtoolsIssue(element, form_issues);
    MaybeAppendAutocompleteAttributeDevtoolsIssue(element, form_issues);
    MaybeAppendInputWithEmptyIdAndNameDevtoolsIssue(element, form_issues);
    MaybeAppendInputAssignedAutocompleteValueToIdOrNameAttributesDevtoolsIssue(
        element, form_issues);
  }
}

// Looks for form issues in `control_elements`, e.g., inputs with duplicate ids
// and returns a vector that is the union of `form_issues` and the new issues
// found.
std::vector<FormIssue> GetFormIssues(
    base::span<const WebFormControlElement> control_elements,
    std::vector<FormIssue> form_issues) {
  AppendFormIssuesInternal(control_elements, form_issues);
  return form_issues;
}

// Checks for issues with the "for" attribute on <label> elements. This needs to
// be called after label extraction. Similar to `GetFormIssues` it returns a
// vector that is the union of `form_issues` and the new issues found.
std::vector<FormIssue> CheckForLabelsWithIncorrectForAttribute(
    const WebDocument& document,
    base::span<const FormFieldData> fields,
    std::vector<FormIssue> form_issues) {
  const WebString& for_attr = GetWebString<kFor>();
  const WebString& label_attr = GetWebString<kLabel>();

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
      form_issues.emplace_back(GenericIssueErrorType::kFormLabelForNameError,
                               label.GetDomNodeId(), for_attr);
    } else {
      // Label has for attribute but no labellable element whose id OR name
      // matches it.
      // This issue is not emitted in case an element has a name that matches
      // it, in this case we emit kFormLabelForNameError to educate developers
      // that labels should be linked to element ids.
      form_issues.emplace_back(
          GenericIssueErrorType::kFormLabelForMatchesNonExistingIdError,
          label.GetDomNodeId(), for_attr);
    }
  }
  return form_issues;
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

void EmitFormIssues(
    const WebDocument& document,
    base::span<const FormData> forms,
    base::FunctionRef<void(const blink::WebDocument& document,
                           GenericIssueErrorType issue_type,
                           int violating_node,
                           WebString violating_node_attribute)> emit) {
  std::vector<FormIssue> form_issues;
  // Get issues from forms input elements.
  for (const WebFormElement& form_element : document.GetTopLevelForms()) {
    form_issues = GetFormIssues(
        form_util::GetOwnedAutofillableFormControls(document, form_element),
        std::move(form_issues));
  }
  // Get issues from input elements that belong to no form.
  form_issues = GetFormIssues(
      form_util::GetOwnedAutofillableFormControls(document, WebFormElement()),
      std::move(form_issues));
  // Look for fields that after parsed were found to have labels incorrectly
  // used.
  for (const FormData& form : forms) {
    form_issues = CheckForLabelsWithIncorrectForAttribute(
        document, form.fields(), std::move(form_issues));
  }
  if (form_issues.size() > kMaxNumberOfDevtoolsIssuesEmitted) {
    form_issues.erase(form_issues.begin() + kMaxNumberOfDevtoolsIssuesEmitted,
                      form_issues.end());
  }
  for (const FormIssue& form_issue : form_issues) {
    emit(document, form_issue.issue_type, form_issue.violating_node,
         form_issue.violating_node_attribute);
  }
}

}  // namespace autofill::form_issues
