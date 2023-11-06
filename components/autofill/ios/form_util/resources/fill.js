// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//components/autofill/ios/form_util/resources/create_fill_namespace.js';
import '//components/autofill/ios/form_util/resources/fill_element_inference.js';
import '//components/autofill/ios/form_util/resources/fill_util.js';

import * as fillConstants from '//components/autofill/ios/form_util/resources/fill_constants.js';
import {inferLabelFromNext} from '//components/autofill/ios/form_util/resources/fill_element_inference.js';
import * as inferenceUtil from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';

// This file provides methods used to fill forms in JavaScript.

// Requires functions from form.js.

/**
 * @typedef {{
 *   name: string,
 *   value: string,
 *   unique_renderer_id: string,
 *   form_control_type: string,
 *   autocomplete_attributes: string,
 *   max_length: number,
 *   is_autofilled: boolean,
 *   is_checkable: boolean,
 *   is_focusable: boolean,
 *   should_autocomplete: boolean,
 *   role: number,
 *   placeholder_attribute: string,
 *   aria_label: string,
 *   aria_description: string,
 *   option_contents: Array<string>,
 *   option_values: Array<string>
 * }}
 */
let AutofillFormFieldData;

/**
 * @typedef {{
 *   name: string,
 *   unique_renderer_id: string,
 *   origin: string,
 *   action: string,
 *   fields: Array<AutofillFormFieldData>
 *   frame_id: string
 * }}
 */
let AutofillFormData;

/**
 * Extracts fields from |controlElements| with |extractMask| to |formFields|.
 * The extracted fields are also placed in |elementArray|.
 *
 * It is based on the logic in
 *     bool ExtractFieldsFromControlElements(
 *         const WebVector<WebFormControlElement>& control_elements,
 *         const FieldValueAndPropertiesMaskMap* field_value_and_properties_map,
 *         ExtractMask extract_mask,
 *         std::vector<std::unique_ptr<FormFieldData>>* form_fields,
 *         std::vector<bool>* fields_extracted,
 *         std::map<WebFormControlElement, FormFieldData*>* element_map)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc
 *
 * TODO(crbug.com/1030490): Make |elementArray| a Map.
 *
 * @param {Array<FormControlElement>} controlElements The control elements that
 *     will be processed.
 * @param {number} extractMask Mask controls what data is extracted from
 *     controlElements.
 * @param {Array<AutofillFormFieldData>} formFields The extracted form fields.
 * @param {Array<boolean>} fieldsExtracted Indicates whether the fields were
 *     extracted.
 * @param {Array<?AutofillFormFieldData>} elementArray The extracted form
 *     fields or null if a particular control has no corresponding field.
 * @return {boolean} Whether there are fields and not too many fields in the
 *     form.
 */
function extractFieldsFromControlElements_(
    controlElements, extractMask, formFields, fieldsExtracted, elementArray) {
  for (let i = 0; i < controlElements.length; ++i) {
    fieldsExtracted[i] = false;
    elementArray[i] = null;

    /** @type {FormControlElement} */
    const controlElement = controlElements[i];
    if (!__gCrWeb.fill.isAutofillableElement(controlElement)) {
      continue;
    }
    try {
      __gCrWeb.fill.setUniqueIDIfNeeded(controlElements[i]);
    } catch (e) {
    }

    // Create a new AutofillFormFieldData, fill it out and map it to the
    // field's name.
    const formField = new __gCrWeb['common'].JSONSafeObject();
    __gCrWeb.fill.webFormControlElementToFormField(
        controlElement, extractMask, formField);
    formFields.push(formField);
    elementArray[i] = formField;
    fieldsExtracted[i] = true;

    // To avoid overly expensive computation, we impose a maximum number of
    // allowable fields.
    if (formFields.length > fillConstants.MAX_EXTRACTABLE_FIELDS) {
      return false;
    }
  }

  return formFields.length > 0;
}

/**
 * Check if the node is visible.
 *
 * @param {Node} node The node to be processed.
 * @return {boolean} Whether the node is visible or not.
 */
__gCrWeb.fill.isVisibleNode = function(node) {
  if (!node) {
    return false;
  }

  if (node.nodeType === Node.ELEMENT_NODE) {
    const style = window.getComputedStyle(/** @type {Element} */ (node));
    if (style.visibility === 'hidden' || style.display === 'none') {
      return false;
    }
  }

  // Verify all ancestors are focusable.
  return !node.parentNode || __gCrWeb.fill.isVisibleNode(node.parentNode);
};

/**
 * For each label element, get the corresponding form control element, use the
 * form control element along with |controlElements| and |elementArray| to find
 * the previously created AutofillFormFieldData and set the
 * AutofillFormFieldData's label to the label.firstChild().nodeValue() of the
 * label element.
 *
 * It is based on the logic in
 *     void MatchLabelsAndFields(
 *         const WebElementCollection& labels,
 *         std::map<WebFormControlElement, FormFieldData*>* element_map);
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc
 *
 * This differs in that it takes a formElement field, instead of calling
 * field_element.isFormControlElement().
 *
 * This also uses (|controlElements|, |elementArray|) because there is no
 * guaranteed Map support on iOS yet.
 * TODO(crbug.com/1030490): Make |elementArray| a Map.
 *
 * @param {NodeList} labels The labels to match.
 * @param {HTMLFormElement} formElement The form element being processed.
 * @param {Array<FormControlElement>} controlElements The control elements that
 *     were processed.
 * @param {Array<?AutofillFormFieldData>} elementArray The extracted fields.
 */
function matchLabelsAndFields_(
    labels, formElement, controlElements, elementArray) {
  for (let index = 0; index < labels.length; ++index) {
    const label = labels[index];
    const fieldElement = label.control;
    let fieldData = null;
    if (!fieldElement) {
      // Sometimes site authors will incorrectly specify the corresponding
      // field element's name rather than its id, so we compensate here.
      const elementName = label.htmlFor;
      if (!elementName) {
        continue;
      }
      // Look through the list for elements with this name. There can actually
      // be more than one. In this case, the label may not be particularly
      // useful, so just discard it.
      for (let elementIndex = 0; elementIndex < elementArray.length;
           ++elementIndex) {
        const currentFieldData = elementArray[elementIndex];
        if (currentFieldData && currentFieldData['name'] === elementName) {
          if (fieldData !== null) {
            fieldData = null;
            break;
          } else {
            fieldData = currentFieldData;
          }
        }
      }
    } else if (
        fieldElement.form !== formElement || fieldElement.type === 'hidden') {
      continue;
    } else {
      // Typical case: look up |fieldData| in |elementArray|.
      for (let elementIndex = 0; elementIndex < elementArray.length;
           ++elementIndex) {
        if (controlElements[elementIndex] === fieldElement) {
          fieldData = elementArray[elementIndex];
          break;
        }
      }
    }

    if (!fieldData) {
      continue;
    }

    if (!('label' in fieldData)) {
      fieldData['label'] = '';
    }
    let labelText = inferenceUtil.findChildText(label);
    if (labelText.length === 0 && !label.htmlFor) {
      labelText = inferLabelFromNext(fieldElement);
    }
    // Concatenate labels because some sites might have multiple label
    // candidates.
    if (fieldData['label'].length > 0 && labelText.length > 0) {
      fieldData['label'] += ' ';
    }
    fieldData['label'] += labelText;
  }
}

/**
 * Common function shared by webFormElementToFormData() and
 * unownedFormElementsAndFieldSetsToFormData(). Either pass in:
 * 1) |formElement|, |formControlElement| and an empty |fieldsets|.
 * or
 * 2) a non-empty |fieldsets|.
 *
 * It is based on the logic in
 *     bool FormOrFieldsetsToFormData(
 *         const blink::WebFormElement* form_element,
 *         const blink::WebFormControlElement* form_control_element,
 *         const std::vector<blink::WebElement>& fieldsets,
 *         const WebVector<WebFormControlElement>& control_elements,
 *         ExtractMask extract_mask,
 *         FormData* form,
 *         FormFieldData* field)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc
 *
 * @param {HTMLFormElement} formElement The form element that will be processed.
 * @param {FormControlElement} formControlElement A control element in
 *     formElement, the FormField of which will be returned in field.
 * @param {Array<Element>} fieldsets The fieldsets to look through if
 *     formElement and formControlElement are not specified.
 * @param {Array<FormControlElement>} controlElements The control elements that
 *     will be processed.
 * @param {number} extractMask Mask controls what data is extracted from
 *     formElement.
 * @param {Object} form Form to fill in the AutofillFormData
 *     information of formElement.
 * @param {?AutofillFormFieldData} field Field to fill in the form field
 *     information of formControlElement.
 * @return {boolean} Whether there are fields and not too many fields in the
 *     form.
 */
__gCrWeb.fill.formOrFieldsetsToFormData = function(
    formElement, formControlElement, fieldsets, controlElements, extractMask,
    form, field) {
  // This should be a map from a control element to the AutofillFormFieldData.
  // However, without Map support, it's just an Array of AutofillFormFieldData.
  const elementArray = [];

  // The extracted FormFields.
  const formFields = [];

  // A vector of booleans that indicate whether each element in
  // |controlElements| meets the requirements and thus will be in the resulting
  // |form|.
  const fieldsExtracted = [];

  if (!extractFieldsFromControlElements_(
          controlElements, extractMask, formFields, fieldsExtracted,
          elementArray)) {
    return false;
  }

  if (formElement) {
    // Loop through the label elements inside the form element. For each label
    // element, get the corresponding form control element, use the form control
    // element along with |controlElements| and |elementArray| to find the
    // previously created AutofillFormFieldData and set the
    // AutofillFormFieldData's label.
    const labels = formElement.getElementsByTagName('label');
    matchLabelsAndFields_(labels, formElement, controlElements, elementArray);
  } else {
    // Same as the if block, but for all the labels in fieldset
    for (let i = 0; i < fieldsets.length; ++i) {
      const labels = fieldsets[i].getElementsByTagName('label');
      matchLabelsAndFields_(labels, formElement, controlElements, elementArray);
    }
  }

  // Loop through the form control elements, extracting the label text from
  // the DOM.  We use the |fieldsExtracted| vector to make sure we assign the
  // extracted label to the correct field, as it's possible |form_fields| will
  // not contain all of the elements in |control_elements|.
  for (let i = 0, fieldIdx = 0;
       i < controlElements.length && fieldIdx < formFields.length; ++i) {
    // This field didn't meet the requirements, so don't try to find a label
    // for it.
    if (!fieldsExtracted[i]) {
      continue;
    }

    const controlElement = controlElements[i];
    const currentField = formFields[fieldIdx];
    if (!currentField['label']) {
      currentField['label'] =
          __gCrWeb.fill.inferLabelForElement(controlElement);
    }
    if (currentField['label'].length > fillConstants.MAX_DATA_LENGTH) {
      currentField['label'] =
          currentField['label'].substr(0, fillConstants.MAX_DATA_LENGTH);
    }

    if (controlElement === formControlElement) {
      field = formFields[fieldIdx];
    }
    ++fieldIdx;
  }

  form['fields'] = formFields;
  // Protect against custom implementation of Array.toJSON in host pages.
  form['fields'].toJSON = null;
  return true;
};

/**
 * Fills |form| with the form data object corresponding to the
 * |formElement|. If |field| is non-NULL, also fills |field| with the
 * FormField object corresponding to the |formControlElement|.
 * |extract_mask| controls what data is extracted.
 * Returns true if |form| is filled out. Returns false if there are no
 * fields or too many fields in the |form|.
 *
 * It is based on the logic in
 *     bool WebFormElementToFormData(
 *         const blink::WebFormElement& form_element,
 *         const blink::WebFormControlElement& form_control_element,
 *         ExtractMask extract_mask,
 *         FormData* form,
 *         FormFieldData* field)
 * in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.cc
 *
 * @param {HTMLFrameElement|Window} frame The window or frame where the
 *     formElement is in.
 * @param {HTMLFormElement} formElement The form element that will be processed.
 * @param {FormControlElement} formControlElement A control element in
 *     formElement, the FormField of which will be returned in field.
 * @param {number} extractMask Mask controls what data is extracted from
 *     formElement.
 * @param {Object} form Form to fill in the AutofillFormData
 *     information of formElement.
 * @param {?AutofillFormFieldData} field Field to fill in the form field
 *     information of formControlElement.
 * @return {boolean} Whether there are fields and not too many fields in the
 *     form.
 */
__gCrWeb.fill.webFormElementToFormData = function(
    frame, formElement, formControlElement, extractMask, form, field) {
  if (!frame) {
    return false;
  }

  form['name'] = __gCrWeb.form.getFormIdentifier(formElement);
  form['origin'] = __gCrWeb.common.removeQueryAndReferenceFromURL(frame.origin);
  form['action'] = __gCrWeb.fill.getCanonicalActionForForm(formElement);

  // The raw name and id attributes, which may be empty.
  form['name_attribute'] = formElement.getAttribute('name') || '';
  form['id_attribute'] = formElement.getAttribute('id') || '';

  __gCrWeb.fill.setUniqueIDIfNeeded(formElement);
  form['unique_renderer_id'] = __gCrWeb.fill.getUniqueID(formElement);

  form['frame_id'] = frame.__gCrWeb.message.getFrameId();

  // Note different from form_autofill_util.cc version of this method, which
  // computes |form.action| using document.completeURL(form_element.action())
  // and falls back to formElement.action() if the computed action is invalid,
  // here the action returned by |absoluteURL_| is always valid, which is
  // computed by creating a <a> element, and we don't check if the action is
  // valid.

  const controlElements = __gCrWeb.form.getFormControlElements(formElement);

  return __gCrWeb.fill.formOrFieldsetsToFormData(
      formElement, formControlElement, [] /* fieldsets */, controlElements,
      extractMask, form, field);
};

/**
 * Fills out a FormField object from a given form control element.
 *
 * It is based on the logic in
 *     void WebFormControlElementToFormField(
 *         const blink::WebFormControlElement& element,
 *         ExtractMask extract_mask,
 *         FormFieldData* field);
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {FormControlElement} element The element to be processed.
 * @param {number} extractMask A bit field mask to extract data from |element|.
 *     See the documentation on variable EXTRACT_MASK_VALUE,
 *     EXTRACT_MASK_OPTION_TEXT and EXTRACT_MASK_OPTIONS.
 * @param {AutofillFormFieldData} field Field to fill in the element
 *     information.
 */
__gCrWeb.fill.webFormControlElementToFormField = function(
    element, extractMask, field) {
  if (!field || !element) {
    return;
  }
  // The label is not officially part of a form control element; however, the
  // labels for all form control elements are scraped from the DOM and set in
  // form data.
  field['identifier'] = __gCrWeb.form.getFieldIdentifier(element);
  field['name'] = __gCrWeb.form.getFieldName(element);

  // The raw name and id attributes, which may be empty.
  field['name_attribute'] = element.getAttribute('name') || '';
  field['id_attribute'] = element.getAttribute('id') || '';

  __gCrWeb.fill.setUniqueIDIfNeeded(element);
  field['unique_renderer_id'] = __gCrWeb.fill.getUniqueID(element);

  field['form_control_type'] = element.type;
  const autocompleteAttribute = element.getAttribute('autocomplete');
  if (autocompleteAttribute) {
    field['autocomplete_attribute'] = autocompleteAttribute;
  }
  if (field['autocomplete_attribute'] != null &&
      field['autocomplete_attribute'].length > fillConstants.MAX_DATA_LENGTH) {
    // Discard overly long attribute values to avoid DOS-ing the browser
    // process. However, send over a default string to indicate that the
    // attribute was present.
    field['autocomplete_attribute'] = 'x-max-data-length-exceeded';
  }

  const roleAttribute = element.getAttribute('role');
  if (roleAttribute && roleAttribute.toLowerCase() === 'presentation') {
    field['role'] = fillConstants.ROLE_ATTRIBUTE_PRESENTATION;
  }

  field['placeholder_attribute'] = element.getAttribute('placeholder') || '';
  if (field['placeholder_attribute'] != null &&
      field['placeholder_attribute'].length > fillConstants.MAX_DATA_LENGTH) {
    // Discard overly long attribute values to avoid DOS-ing the browser
    // process. However, send over a default string to indicate that the
    // attribute was present.
    field['placeholder_attribute'] = 'x-max-data-length-exceeded';
  }

  field['aria_label'] = __gCrWeb.fill.getAriaLabel(element);
  field['aria_description'] = __gCrWeb.fill.getAriaDescription(element);

  if (!__gCrWeb.fill.isAutofillableElement(element)) {
    return;
  }

  if (__gCrWeb.fill.isAutofillableInputElement(element) ||
      inferenceUtil.isTextAreaElement(element) ||
      __gCrWeb.fill.isSelectElement(element)) {
    field['is_autofilled'] = element['isAutofilled'];
    field['should_autocomplete'] = __gCrWeb.fill.shouldAutocomplete(element);
    field['is_focusable'] = !element.disabled && !element.readOnly &&
        element.tabIndex >= 0 && __gCrWeb.fill.isVisibleNode(element);
  }

  if (__gCrWeb.fill.isAutofillableInputElement(element)) {
    if (__gCrWeb.fill.isTextInput(element)) {
      field['max_length'] = element.maxLength;
      if (field['max_length'] === -1) {
        // Take default value as defined by W3C.
        field['max_length'] = 524288;
      }
    }
    field['is_checkable'] = __gCrWeb.fill.isCheckableElement(element);
  } else if (inferenceUtil.isTextAreaElement(element)) {
    // Nothing more to do in this case.
  } else if (extractMask & fillConstants.EXTRACT_MASK_OPTIONS) {
    __gCrWeb.fill.getOptionStringsFromElement(element, field);
  }

  if (!(extractMask & fillConstants.EXTRACT_MASK_VALUE)) {
    return;
  }

  let value = __gCrWeb.fill.value(element);

  if (__gCrWeb.fill.isSelectElement(element) &&
      (extractMask & fillConstants.EXTRACT_MASK_OPTION_TEXT)) {
    // Convert the |select_element| value to text if requested.
    const options = element.options;
    for (let index = 0; index < options.length; ++index) {
      const optionElement = options[index];
      if (__gCrWeb.fill.value(optionElement) === value) {
        value = optionElement.text;
        break;
      }
    }
  }

  // There is a constraint on the maximum data length in method
  // WebFormControlElementToFormField() in form_autofill_util.h in order to
  // prevent a malicious site from DOS'ing the browser: http://crbug.com/49332,
  // which isn't really meaningful here, but we need to follow the same logic to
  // get the same form signature wherever possible (to get the benefits of the
  // existing crowdsourced field detection corpus).
  if (value.length > fillConstants.MAX_DATA_LENGTH) {
    value = value.substr(0, fillConstants.MAX_DATA_LENGTH);
  }
  field['value'] = value;
};

/**
 * Returns a serialized version of |form| to send to the host on form
 * submission.
 * The result string is similar to the result of calling |extractForms| filtered
 * on |form| (that is why a list is returned).
 *
 * @param {FormElement} form The form to serialize.
 * @return {string} a JSON encoded version of |form|
 */
__gCrWeb.fill.autofillSubmissionData = function(form) {
  const formData = new __gCrWeb['common'].JSONSafeObject();
  const extractMask =
      fillConstants.EXTRACT_MASK_VALUE | fillConstants.EXTRACT_MASK_OPTIONS;
  __gCrWeb['fill'].webFormElementToFormData(
      window, form, null, extractMask, formData, null);
  return __gCrWeb.stringify([formData]);
};

/**
 * Get all form control elements from |elements| that are not part of a form.
 * Also append the fieldsets encountered that are not part of a form to
 * |fieldsets|.
 *
 * It is based on the logic in:
 *     std::vector<WebFormControlElement>
 *     GetUnownedAutofillableFormFieldElements(
 *         const WebElementCollection& elements,
 *         std::vector<WebElement>* fieldsets);
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * In the C++ version, |fieldsets| can be NULL, in which case we do not try to
 * append to it.
 *
 * @param {Array<!FormControlElement>} elements elements to look through.
 * @param {Array<Element>} fieldsets out param for unowned fieldsets.
 * @return {Array<FormControlElement>} The elements that are not part of a form.
 */
__gCrWeb.fill.getUnownedAutofillableFormFieldElements = function(
    elements, fieldsets) {
  const unownedFieldsetChildren = [];
  for (let i = 0; i < elements.length; ++i) {
    if (__gCrWeb.form.isFormControlElement(elements[i])) {
      if (!elements[i].form) {
        unownedFieldsetChildren.push(elements[i]);
      }
    }

    if (__gCrWeb.fill.hasTagName(elements[i], 'fieldset') &&
        !__gCrWeb.fill.isElementInsideFormOrFieldSet(elements[i])) {
      fieldsets.push(elements[i]);
    }
  }
  return __gCrWeb.fill.extractAutofillableElementsFromSet(
      unownedFieldsetChildren);
};


/**
 * Fills |form| with the form data object corresponding to the unowned elements
 * and fieldsets in the document.
 * |extract_mask| controls what data is extracted.
 * Returns true if |form| is filled out. Returns false if there are no fields or
 * too many fields in the |form|.
 *
 * It is based on the logic in
 *     bool UnownedFormElementsAndFieldSetsToFormData(
 *         const std::vector<blink::WebElement>& fieldsets,
 *         const std::vector<blink::WebFormControlElement>& control_elements,
 *         const GURL& origin,
 *         ExtractMask extract_mask,
 *         FormData* form)
 * and
 *     bool UnownedCheckoutFormElementsAndFieldSetsToFormData(
 *         const std::vector<blink::WebElement>& fieldsets,
 *         const std::vector<blink::WebFormControlElement>& control_elements,
 *         const blink::WebFormControlElement* element,
 *         const blink::WebDocument& document,
 *         ExtractMask extract_mask,
 *         FormData* form,
 *         FormFieldData* field)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc
 *
 * @param {HTMLFrameElement|Window} frame The window or frame where the
 *     formElement is in.
 * @param {Array<Element>} fieldsets The fieldsets to look through.
 * @param {Array<FormControlElement>} controlElements The control elements that
 *     will be processed.
 * @param {number} extractMask Mask controls what data is extracted from
 *     formElement.
 * @param {bool} restrictUnownedFieldsToFormlessCheckout whether forms made of
 *     unowned fields (i.e., not within a <form> tag) should be restricted to
 *     those that appear to be in a checkout flow.
 * @param {AutofillFormData} form Form to fill in the AutofillFormData
 *     information of formElement.
 * @return {boolean} Whether there are fields and not too many fields in the
 *     form.
 */
__gCrWeb.fill.unownedFormElementsAndFieldSetsToFormData = function(
    frame, fieldsets, controlElements, extractMask,
    restrictUnownedFieldsToFormlessCheckout, form) {
  if (!frame) {
    return false;
  }
  form['name'] = '';
  form['origin'] = __gCrWeb.common.removeQueryAndReferenceFromURL(frame.origin);
  form['action'] = '';
  form['is_form_tag'] = false;

  if (!restrictUnownedFieldsToFormlessCheckout) {
    return __gCrWeb.fill.formOrFieldsetsToFormData(
        null /* formElement*/, null /* formControlElement */, fieldsets,
        controlElements, extractMask, form, null /* field */);
  }


  const title = document.title.toLowerCase();
  const path = document.location.pathname.toLowerCase();
  // The keywords are defined in
  // UnownedCheckoutFormElementsAndFieldSetsToFormData in
  // components/autofill/content/renderer/form_autofill_util.cc
  const keywords =
      ['payment', 'checkout', 'address', 'delivery', 'shipping', 'wallet'];

  const count = keywords.length;
  for (let index = 0; index < count; index++) {
    const keyword = keywords[index];
    if (title.includes(keyword) || path.includes(keyword)) {
      return __gCrWeb.fill.formOrFieldsetsToFormData(
          null /* formElement*/, null /* formControlElement */, fieldsets,
          controlElements, extractMask, form, null /* field */);
    }
  }

  // Since it's not a checkout flow, only add fields that have a non-"off"
  // autocomplete attribute to the formless autofill.
  const controlElementsWithAutocomplete = [];
  for (let index = 0; index < controlElements.length; index++) {
    if (controlElements[index].hasAttribute('autocomplete') &&
        controlElements[index].getAttribute('autocomplete') !== 'off') {
      controlElementsWithAutocomplete.push(controlElements[index]);
    }
  }

  if (controlElementsWithAutocomplete.length === 0) {
    return false;
  }
  return __gCrWeb.fill.formOrFieldsetsToFormData(
      null /* formElement*/, null /* formControlElement */, fieldsets,
      controlElementsWithAutocomplete, extractMask, form, null /* field */);
};


/**
 * Returns the auto-fillable form control elements in |formElement|.
 *
 * It is based on the logic in:
 *     std::vector<blink::WebFormControlElement>
 *     ExtractAutofillableElementsFromSet(
 *         const WebVector<WebFormControlElement>& control_elements);
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {Array<FormControlElement>} controlElements Set of control elements.
 * @return {Array<FormControlElement>} The array of autofillable elements.
 */
__gCrWeb.fill.extractAutofillableElementsFromSet = function(controlElements) {
  const autofillableElements = [];
  for (let i = 0; i < controlElements.length; ++i) {
    const element = controlElements[i];
    if (!__gCrWeb.fill.isAutofillableElement(element)) {
      continue;
    }
    autofillableElements.push(element);
  }
  return autofillableElements;
};
