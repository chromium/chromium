// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_cache.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/page_form_analyser_logger.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_select_element.h"
#include "ui/base/l10n/l10n_util.h"

using blink::WebAutofillState;
using blink::WebConsoleMessage;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebSelectElement;
using blink::WebString;
using blink::WebVector;

namespace autofill {

namespace {

static const char* kSupportedAutocompleteTypes[] = {"given-name",
                                                    "additional-name",
                                                    "family-name",
                                                    "name",
                                                    "honorific-suffix",
                                                    "email",
                                                    "tel-local",
                                                    "tel-area-code",
                                                    "tel-country-code",
                                                    "tel-national",
                                                    "tel",
                                                    "tel-extension",
                                                    "street-address",
                                                    "address-line1",
                                                    "address-line2",
                                                    "address-line3",
                                                    "address-level1",
                                                    "address-level2",
                                                    "address-level3",
                                                    "postal-code",
                                                    "country-name",
                                                    "cc-name",
                                                    "cc-given-name",
                                                    "cc-family-name",
                                                    "cc-number",
                                                    "cc-exp-month",
                                                    "cc-exp-year",
                                                    "cc-exp",
                                                    "cc-type",
                                                    "cc-csc",
                                                    "organization"};

// For a given |type| (a string representation of enum values), return the
// appropriate autocomplete value that should be suggested to the website
// developer.
const char* MapTypePredictionToAutocomplete(base::StringPiece type) {
  if (type == "NAME_FIRST")
    return kSupportedAutocompleteTypes[0];
  if (type == "NAME_MIDDLE")
    return kSupportedAutocompleteTypes[1];
  if (type == "NAME_LAST")
    return kSupportedAutocompleteTypes[2];
  if (type == "NAME_FULL")
    return kSupportedAutocompleteTypes[3];
  if (type == "NAME_SUFFIX")
    return kSupportedAutocompleteTypes[4];
  if (type == "EMAIL_ADDRESS")
    return kSupportedAutocompleteTypes[5];
  if (type == "PHONE_HOME_NUMBER")
    return kSupportedAutocompleteTypes[6];
  if (type == "PHONE_HOME_CITY_CODE")
    return kSupportedAutocompleteTypes[7];
  if (type == "PHONE_HOME_COUNTRY_CODE")
    return kSupportedAutocompleteTypes[8];
  if (type == "PHONE_HOME_CITY_AND_NUMBER")
    return kSupportedAutocompleteTypes[9];
  if (type == "PHONE_HOME_WHOLE_NUMBER")
    return kSupportedAutocompleteTypes[10];
  if (type == "PHONE_HOME_EXTENSION")
    return kSupportedAutocompleteTypes[11];
  if (type == "ADDRESS_HOME_STREET_ADDRESS")
    return kSupportedAutocompleteTypes[12];
  if (type == "ADDRESS_HOME_LINE1")
    return kSupportedAutocompleteTypes[13];
  if (type == "ADDRESS_HOME_LINE2")
    return kSupportedAutocompleteTypes[14];
  if (type == "ADDRESS_HOME_LINE3")
    return kSupportedAutocompleteTypes[15];
  if (type == "ADDRESS_HOME_CITY")
    return kSupportedAutocompleteTypes[16];
  if (type == "ADDRESS_HOME_STATE")
    return kSupportedAutocompleteTypes[17];
  if (type == "ADDRESS_HOME_DEPENDENT_LOCALITY")
    return kSupportedAutocompleteTypes[18];
  if (type == "ADDRESS_HOME_ZIP")
    return kSupportedAutocompleteTypes[19];
  if (type == "ADDRESS_HOME_COUNTRY")
    return kSupportedAutocompleteTypes[20];
  if (type == "CREDIT_CARD_NAME_FULL")
    return kSupportedAutocompleteTypes[21];
  if (type == "CREDIT_CARD_NAME_FIRST")
    return kSupportedAutocompleteTypes[22];
  if (type == "CREDIT_CARD_NAME_LAST")
    return kSupportedAutocompleteTypes[23];
  if (type == "CREDIT_CARD_NUMBER")
    return kSupportedAutocompleteTypes[24];
  if (type == "CREDIT_CARD_EXP_MONTH")
    return kSupportedAutocompleteTypes[25];
  if (type == "CREDIT_CARD_EXP_2_DIGIT_YEAR" ||
      type == "CREDIT_CARD_EXP_4_DIGIT_YEAR")
    return kSupportedAutocompleteTypes[26];
  if (type == "CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR" ||
      type == "CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR")
    return kSupportedAutocompleteTypes[27];
  if (type == "CREDIT_CARD_TYPE")
    return kSupportedAutocompleteTypes[28];
  if (type == "CREDIT_CARD_VERIFICATION_CODE")
    return kSupportedAutocompleteTypes[29];
  if (type == "COMPANY_NAME")
    return kSupportedAutocompleteTypes[30];
  return "";
}

void LogDeprecationMessages(const WebFormControlElement& element) {
  std::string autocomplete_attribute =
      element.GetAttribute("autocomplete").Utf8();

  static const char* const deprecated[] = {"region", "locality"};
  for (const char* str : deprecated) {
    if (autocomplete_attribute.find(str) == std::string::npos)
      continue;
    std::string msg = base::StrCat(
        {"autocomplete='", str,
         "' is deprecated and will soon be ignored. See http://goo.gl/YjeSsW"});
    WebConsoleMessage console_message = WebConsoleMessage(
        blink::mojom::ConsoleMessageLevel::kWarning, WebString::FromASCII(msg));
    element.GetDocument().GetFrame()->AddMessageToConsole(console_message);
  }
}

// Determines whether the form is interesting enough to be sent to the browser
// for further operations.
bool IsFormInteresting(const FormData& form, size_t num_editable_elements) {
  if (form.fields.empty())
    return false;

  // If the form has at least one field with an autocomplete attribute, it is a
  // candidate for autofill.
  bool all_fields_are_passwords = true;
  for (const FormFieldData& field : form.fields) {
    if (!field.autocomplete_attribute.empty())
      return true;
    if (field.form_control_type != "password")
      all_fields_are_passwords = false;
  }

  // If there are no autocomplete attributes, the form needs to have at least
  // the required number of editable fields for the prediction routines to be a
  // candidate for autofill.
  return num_editable_elements >= MinRequiredFieldsForHeuristics() ||
         num_editable_elements >= MinRequiredFieldsForQuery() ||
         num_editable_elements >= MinRequiredFieldsForUpload() ||
         (all_fields_are_passwords &&
          num_editable_elements >=
              kRequiredFieldsForFormsWithOnlyPasswordFields);
}

}  // namespace

FormCache::FormCache(WebLocalFrame* frame) : frame_(frame) {}
FormCache::~FormCache() = default;

std::vector<FormData> FormCache::ExtractNewForms() {
  std::vector<FormData> forms;
  WebDocument document = frame_->GetDocument();
  if (document.IsNull())
    return forms;

  initial_checked_state_.clear();
  initial_select_values_.clear();
  WebVector<WebFormElement> web_forms;
  document.Forms(web_forms);

  std::set<uint32_t> observed_unique_renderer_ids;

  // Log an error message for deprecated attributes, but only the first time
  // the form is parsed.
  bool log_deprecation_messages = parsed_forms_.empty();

  const form_util::ExtractMask extract_mask =
      static_cast<form_util::ExtractMask>(form_util::EXTRACT_VALUE |
                                          form_util::EXTRACT_OPTIONS);

  size_t num_fields_seen = 0;
  for (const WebFormElement& form_element : web_forms) {
    std::vector<WebFormControlElement> control_elements =
        form_util::ExtractAutofillableElementsInForm(form_element);

    size_t num_editable_elements =
        ScanFormControlElements(control_elements, log_deprecation_messages);
    if (num_editable_elements == 0)
      continue;

    FormData form;
    if (!WebFormElementToFormData(form_element, WebFormControlElement(),
                                  nullptr, extract_mask, &form, nullptr)) {
      continue;
    }

    for (const auto& field : form.fields)
      observed_unique_renderer_ids.insert(field.unique_renderer_id);

    num_fields_seen += form.fields.size();
    if (num_fields_seen > form_util::kMaxParseableFields) {
      PruneInitialValueCaches(observed_unique_renderer_ids);
      return forms;
    }

    if (!base::Contains(parsed_forms_, form) &&
        IsFormInteresting(form, num_editable_elements)) {
      for (auto it = parsed_forms_.begin(); it != parsed_forms_.end(); ++it) {
        if (it->SameFormAs(form)) {
          parsed_forms_.erase(it);
          break;
        }
      }

      SaveInitialValues(control_elements);
      forms.push_back(form);
      parsed_forms_.insert(form);
    }
  }

  // Look for more parseable fields outside of forms.
  std::vector<WebElement> fieldsets;
  std::vector<WebFormControlElement> control_elements =
      form_util::GetUnownedAutofillableFormFieldElements(document.All(),
                                                         &fieldsets);

  size_t num_editable_elements =
      ScanFormControlElements(control_elements, log_deprecation_messages);
  if (num_editable_elements == 0) {
    PruneInitialValueCaches(observed_unique_renderer_ids);
    return forms;
  }

  FormData synthetic_form;
  if (!UnownedCheckoutFormElementsAndFieldSetsToFormData(
          fieldsets, control_elements, nullptr, document, extract_mask,
          &synthetic_form, nullptr)) {
    PruneInitialValueCaches(observed_unique_renderer_ids);
    return forms;
  }

  for (const auto& field : synthetic_form.fields)
    observed_unique_renderer_ids.insert(field.unique_renderer_id);

  num_fields_seen += synthetic_form.fields.size();
  if (num_fields_seen > form_util::kMaxParseableFields) {
    PruneInitialValueCaches(observed_unique_renderer_ids);
    return forms;
  }

  if (!base::Contains(parsed_forms_, synthetic_form) &&
      IsFormInteresting(synthetic_form, num_editable_elements)) {
    SaveInitialValues(control_elements);
    forms.push_back(synthetic_form);
    parsed_forms_.insert(synthetic_form);
    parsed_forms_.erase(synthetic_form_);
    synthetic_form_ = synthetic_form;
  }

  PruneInitialValueCaches(observed_unique_renderer_ids);
  return forms;
}

void FormCache::Reset() {
  synthetic_form_ = FormData();
  parsed_forms_.clear();
  initial_select_values_.clear();
  initial_checked_state_.clear();
}

bool FormCache::ClearSectionWithElement(const WebFormControlElement& element) {
  WebFormElement form_element = element.Form();
  std::vector<WebFormControlElement> control_elements =
      form_element.IsNull()
          ? form_util::GetUnownedAutofillableFormFieldElements(
                element.GetDocument().All(), nullptr)
          : form_util::ExtractAutofillableElementsInForm(form_element);

  for (WebFormControlElement& control_element : control_elements) {
    // Don't modify the value of disabled fields.
    if (!control_element.IsEnabled())
      continue;

    // Don't clear field that was not autofilled
    if (!control_element.IsAutofilled())
      continue;

    if (control_element.AutofillSection() != element.AutofillSection())
      continue;

    control_element.SetAutofillState(WebAutofillState::kNotFilled);

    WebInputElement* input_element = ToWebInputElement(&control_element);
    if (form_util::IsTextInput(input_element) ||
        form_util::IsMonthInput(input_element)) {
      input_element->SetAutofillValue(blink::WebString());

      // Clearing the value in the focused node (above) can cause selection
      // to be lost. We force selection range to restore the text cursor.
      if (element == *input_element) {
        int length = input_element->Value().length();
        input_element->SetSelectionRange(length, length);
      }
    } else if (form_util::IsTextAreaElement(control_element)) {
      control_element.SetAutofillValue(blink::WebString());
    } else if (form_util::IsSelectElement(control_element)) {
      WebSelectElement select_element = control_element.To<WebSelectElement>();

      auto initial_value_iter = initial_select_values_.find(
          select_element.UniqueRendererFormControlId());
      if (initial_value_iter != initial_select_values_.end() &&
          select_element.Value().Utf16() != initial_value_iter->second) {
        select_element.SetAutofillValue(
            blink::WebString::FromUTF16(initial_value_iter->second));
      }
    } else {
      WebInputElement input_element = control_element.To<WebInputElement>();
      DCHECK(form_util::IsCheckableElement(&input_element));
      auto checkable_element_it = initial_checked_state_.find(
          input_element.UniqueRendererFormControlId());
      if (checkable_element_it != initial_checked_state_.end() &&
          input_element.IsChecked() != checkable_element_it->second) {
        input_element.SetChecked(checkable_element_it->second, true);
      }
    }
  }

  return true;
}

bool FormCache::ShowPredictions(const FormDataPredictions& form,
                                bool attach_predictions_to_dom) {
  DCHECK_EQ(form.data.fields.size(), form.fields.size());

  std::vector<WebFormControlElement> control_elements;

  // First check the synthetic form.
  bool found_synthetic_form = false;
  if (form.data.SameFormAs(synthetic_form_)) {
    found_synthetic_form = true;
    WebDocument document = frame_->GetDocument();
    control_elements = form_util::GetUnownedAutofillableFormFieldElements(
        document.All(), nullptr);
  }

  if (!found_synthetic_form) {
    // Find the real form by searching through the WebDocuments.
    bool found_form = false;
    WebVector<WebFormElement> web_forms;
    frame_->GetDocument().Forms(web_forms);

    for (const WebFormElement& form_element : web_forms) {
      // To match two forms, we look for the form's name and the number of
      // fields on that form. (Form names may not be unique.)
      // Note: WebString() == WebString(string16()) does not evaluate to |true|
      // -- WebKit distinguishes between a "null" string (lhs) and an "empty"
      // string (rhs). We don't want that distinction, so forcing to string16.
      base::string16 element_name = form_util::GetFormIdentifier(form_element);
      if (element_name == form.data.name) {
        found_form = true;
        control_elements =
            form_util::ExtractAutofillableElementsInForm(form_element);
        if (control_elements.size() == form.fields.size())
          break;
      }
    }

    if (!found_form)
      return false;
  }

  if (control_elements.size() != form.fields.size()) {
    // Keep things simple.  Don't show predictions for forms that were modified
    // between page load and the server's response to our query.
    return false;
  }

  PageFormAnalyserLogger logger(frame_);
  for (size_t i = 0; i < control_elements.size(); ++i) {
    WebFormControlElement& element = control_elements[i];

    const FormFieldData& field_data = form.data.fields[i];
    if (element.NameForAutofill().Utf16() != field_data.name) {
      // Keep things simple.  Don't show predictions for elements whose names
      // were modified between page load and the server's response to our query.
      continue;
    }
    const FormFieldDataPredictions& field = form.fields[i];

    // Possibly add a console warning for this field regarding the usage of
    // autocomplete attributes.
    const std::string predicted_autocomplete_attribute =
        MapTypePredictionToAutocomplete(field.overall_type);
    if (ShouldShowAutocompleteConsoleWarnings(
            predicted_autocomplete_attribute,
            element.GetAttribute("autocomplete").Utf8())) {
      logger.Send(
          base::StringPrintf("Input elements should have autocomplete "
                             "attributes (suggested: autocomplete='%s', "
                             "confirm at https://goo.gl/6KgkJg)",
                             predicted_autocomplete_attribute.c_str()),
          PageFormAnalyserLogger::kVerbose, element);
    }

    // If the flag is enabled, attach the prediction to the field.
    if (attach_predictions_to_dom) {
      constexpr size_t kMaxLabelSize = 100;
      const base::string16 truncated_label = field_data.label.substr(
          0, std::min(field_data.label.length(), kMaxLabelSize));

      std::string title =
          base::StrCat({"overall type: ", field.overall_type,             //
                        "\nserver type: ", field.server_type,             //
                        "\nheuristic type: ", field.heuristic_type,       //
                        "\nlabel: ", base::UTF16ToUTF8(truncated_label),  //
                        "\nparseable name: ", field.parseable_name,       //
                        "\nsection: ", field.section,                     //
                        "\nfield signature: ", field.signature,           //
                        "\nform signature: ", form.signature});

      // Set this debug string to the title so that a developer can easily debug
      // by hovering the mouse over the input field.
      element.SetAttribute("title", WebString::FromUTF8(title));

      // Set the same debug string to an attribute that does not get mangled if
      // Google Translate is triggered for the site. This is useful for
      // automated processing of the data.
      element.SetAttribute("autofill-information", WebString::FromUTF8(title));

      element.SetAttribute("autofill-prediction",
                           WebString::FromUTF8(field.overall_type));
    }
  }
  logger.Flush();

  return true;
}

size_t FormCache::ScanFormControlElements(
    const std::vector<WebFormControlElement>& control_elements,
    bool log_deprecation_messages) {
  size_t num_editable_elements = 0;
  for (const WebFormControlElement& element : control_elements) {
    if (log_deprecation_messages)
      LogDeprecationMessages(element);

    // Save original values of <select> elements so we can restore them
    // when |ClearFormWithNode()| is invoked.
    if (form_util::IsSelectElement(element) ||
        form_util::IsTextAreaElement(element)) {
      ++num_editable_elements;
    } else {
      const WebInputElement input_element = element.ToConst<WebInputElement>();
      if (!form_util::IsCheckableElement(&input_element))
        ++num_editable_elements;
    }
  }
  return num_editable_elements;
}

void FormCache::SaveInitialValues(
    const std::vector<WebFormControlElement>& control_elements) {
  for (const WebFormControlElement& element : control_elements) {
    if (form_util::IsSelectElement(element)) {
      const WebSelectElement select_element =
          element.ToConst<WebSelectElement>();
      initial_select_values_.insert(
          std::make_pair(select_element.UniqueRendererFormControlId(),
                         select_element.Value().Utf16()));
    } else {
      const WebInputElement* input_element = ToWebInputElement(&element);
      if (form_util::IsCheckableElement(input_element)) {
        initial_checked_state_.insert(
            std::make_pair(input_element->UniqueRendererFormControlId(),
                           input_element->IsChecked()));
      }
    }
  }
}

bool FormCache::ShouldShowAutocompleteConsoleWarnings(
    const std::string& predicted_autocomplete,
    const std::string& actual_autocomplete) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillShowAutocompleteConsoleWarnings)) {
    return false;
  }

  // If we have no better prediction, do not show.
  if (predicted_autocomplete.empty())
    return false;

  // We should show a warning if the actual autocomplete attribute is empty,
  // or we recognize the autocomplete attribute, but we think it's the wrong
  // one.
  if (actual_autocomplete.empty())
    return true;

  // An autocomplete attribute can be multiple strings (e.g. "shipping name").
  // Look at all the tokens.
  for (base::StringPiece actual : base::SplitStringPiece(
           actual_autocomplete, " ", base::WhitespaceHandling::TRIM_WHITESPACE,
           base::SplitResult::SPLIT_WANT_NONEMPTY)) {
    // If we recognize the value but it's not correct, show a warning.
    if (base::Contains(kSupportedAutocompleteTypes, actual) &&
        actual != predicted_autocomplete) {
      return true;
    }
  }
  return false;
}

void FormCache::PruneInitialValueCaches(
    const std::set<uint32_t>& ids_to_retain) {
  auto should_not_retain = [&ids_to_retain](const auto& p) {
    return !base::Contains(ids_to_retain, p.first);
  };
  base::EraseIf(initial_select_values_, should_not_retain);
  base::EraseIf(initial_checked_state_, should_not_retain);
}

}  // namespace autofill
