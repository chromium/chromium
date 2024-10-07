// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//components/autofill/ios/form_util/resources/fill_util.js';

import {registerChildFrame} from '//components/autofill/ios/form_util/resources/child_frame_registration_lib.js';
import * as fillConstants from '//components/autofill/ios/form_util/resources/fill_constants.js';
import {inferLabelFromNext} from '//components/autofill/ios/form_util/resources/fill_element_inference.js';
import * as inferenceUtil from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';
import type * as fillUtil from '//components/autofill/ios/form_util/resources/fill_util.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

// This file provides methods used to fill forms in JavaScript.

// Requires functions from form.ts.

declare global {
  // Defines an additional property, `__gcrweb`, on the Window object.
  // This definition is needed in order to call into gCrWeb inside an iframe.
  interface Window {
    __gCrWeb: any;
  }
}

/**
 * Extracts fields from |controlElements| to |formFields|.
 * The extracted fields are also placed in |elementArray|.
 *
 * TODO(crbug.com/40661883): Make |elementArray| a Map.
 *
 * @param controlElements The control elements that
 *     will be processed.
 * @param iframeElements The iframe elements that
 *     will be processed.
 * @param formFields The extracted form fields.
 * @param childFrames The extracted child
 *     frames.
 * @param fieldsExtracted Indicates whether the fields were
 *     extracted.
 * @param elementArray The extracted form
 *     fields or null if a particular control has no corresponding field.
 * @return Whether the form contains fields but not too many of them, or the
 *     form contains iframes.
 */
function extractFieldsFromControlElements(
    controlElements: fillConstants.FormControlElement[],
    iframeElements: HTMLIFrameElement[],
    formFields: fillUtil.AutofillFormFieldData[],
    childFrames: fillUtil.FrameTokenWithPredecessor[],
    fieldsExtracted: boolean[],
    elementArray: Array<fillUtil.AutofillFormFieldData|null>): boolean {
  for (const _i of iframeElements) {
    childFrames.push({token: '', predecessor: -1});
  }

  if (!elementArray) {
    elementArray =
        new Array<fillUtil.AutofillFormFieldData|null>(controlElements.length);
  }

  for (let i = 0; i < controlElements.length; ++i) {
    fieldsExtracted[i] = false;
    elementArray[i] = null;

    const controlElement = controlElements[i];
    if (!gCrWeb.fill.isAutofillableElement(controlElement)) {
      continue;
    }

    // Create a new AutofillFormFieldData, fill it out and map it to the
    // field's name.
    const formField = new gCrWeb['common'].JSONSafeObject();
    gCrWeb.fill.webFormControlElementToFormField(controlElement, formField);
    formFields.push(formField);
    elementArray[i] = formField;
    fieldsExtracted[i] = true;

    // TODO(crbug.com/40266126): This loop should also track which control
    // element appears immediately before the frame, so its index can be
    // set as the frame predecessor.

    // To avoid overly expensive computation, we impose a maximum number of
    // allowable fields.
    if (formFields.length > fillConstants.MAX_EXTRACTABLE_FIELDS) {
      childFrames.length = 0;
      formFields.length = 0;
      return false;
    }
  }

  return formFields.length > 0 || childFrames.length > 0;
}

/**
 * Check if the node is visible.
 *
 * @param node The node to be processed.
 * @return Whether the node is visible or not.
 */
gCrWeb.fill.isVisibleNode = function(node: Node): boolean {
  if (!node) {
    return false;
  }

  if (node.nodeType === Node.ELEMENT_NODE) {
    const style = window.getComputedStyle(node as Element);
    if (style.visibility === 'hidden' || style.display === 'none') {
      return false;
    }
  }

  // Verify all ancestors are focusable.
  return !node.parentNode || gCrWeb.fill.isVisibleNode(node.parentNode);
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
 * TODO(crbug.com/40661883): Make |elementArray| a Map.
 *
 * @param labels The labels to match.
 * @param formElement The form element being processed.
 * @param controlElements The control elements that
 *     were processed.
 * @param elementArray The extracted fields.
 */
function matchLabelsAndFields(
    labels: HTMLCollectionOf<HTMLLabelElement>,
    formElement: HTMLFormElement|null,
    controlElements: fillConstants.FormControlElement[],
    elementArray: fillUtil.AutofillFormFieldData[]) {
  for (let index = 0; index < labels.length; ++index) {
    const label = labels[index]!;
    const fieldElement = label!.control as fillConstants.FormControlElement;
    let fieldData: fillUtil.AutofillFormFieldData|null = null;
    if (!fieldElement) {
      // Sometimes site authors will incorrectly specify the corresponding
      // field element's name rather than its id, so we compensate here.
      const elementName = label!.htmlFor;
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
        fieldElement.form !== formElement || fieldElement.hidden) {
      continue;
    } else {
      // Typical case: look up |fieldData| in |elementArray|.
      for (let elementIndex = 0; elementIndex < elementArray.length;
           ++elementIndex) {
        if (controlElements[elementIndex] === (fieldElement as any)) {
          fieldData = elementArray[elementIndex]!;
          break;
        }
      }
    }

    if (!fieldData) {
      continue;
    }

    if (!('label' in fieldData)) {
      fieldData.label = '';
    }
    let labelText = inferenceUtil.findChildText(label);
    if (labelText.length === 0 && !label.htmlFor) {
      labelText = inferLabelFromNext(fieldElement)?.label || '';
    }
    // Concatenate labels because some sites might have multiple label
    // candidates.
    if (fieldData.label!.length > 0 && labelText.length > 0) {
      fieldData.label += ' ';
    }
    fieldData.label += labelText;
  }
}

// Returns true if the node `a` is a successor of node `b` if they have a common
// root node.
function isDOMSuccessor(a: Node, b: Node): boolean {
  return (a.compareDocumentPosition(b) & Node.DOCUMENT_POSITION_PRECEDING) > 0;
}

/**
 * Common function shared by webFormElementToFormData() and
 * unownedFormElementsAndFieldSetsToFormData(). Either pass in:
 * 1) |formElement|, |formControlElement| and an empty |fieldsets|.
 * or
 * 2) a non-empty |fieldsets|.
 *
 * It is based on the logic in
 *    bool OwnedOrUnownedFormToFormData(
 *        const WebFrame* frame,
 *        const blink::WebFormElement& form_element,
 *        const blink::WebFormControlElement* form_control_element,
 *        const WebVector<WebFormControlElement>& control_elements,
 *        const std::vector<blink::WebElement>& iframe_elements,
 *        const FieldDataManager* field_data_manager,
 *        ExtractMask extract_mask,
 *        FormData* form,
 *        FormFieldData* optional_field)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc
 *
 * @param formElement The form element that will be processed.
 * @param formControlElement A control element in
 *     formElement, the FormField of which will be returned in field.
 * @param fieldsets The fieldsets to look through if
 *     formElement and formControlElement are not specified.
 * @param controlElements The control elements that
 *     will be processed.
 * @param iframeElements The iframe elements that
 *     will be processed.
 * @param form Form to fill in the AutofillFormData
 *     information of formElement.
 * @param field Field to fill in the form field
 *     information of formControlElement.
 * @return Whether there are fields and not too many fields in the
 *     form.
 */
function formOrFieldsetsToFormData(
    formElement: HTMLFormElement|null,
    formControlElement: fillConstants.FormControlElement|null,
    fieldsets: Element[], controlElements: fillConstants.FormControlElement[],
    iframeElements: HTMLIFrameElement[], form: fillUtil.AutofillFormData,
    _field?: fillUtil.AutofillFormFieldData): boolean {
  // This should be a map from a control element to the AutofillFormFieldData.
  // However, without Map support, it's just an Array of AutofillFormFieldData.
  const elementArray: fillUtil.AutofillFormFieldData[] = [];

  // The extracted FormFields.
  const formFields: fillUtil.AutofillFormFieldData[] = [];

  // The extracted child frames.
  const childFrames: fillUtil.FrameTokenWithPredecessor[] = [];

  // A vector of booleans that indicate whether each element in
  // |controlElements| meets the requirements and thus will be in the resulting
  // |form|.
  const fieldsExtracted: boolean[] = [];

  if (!extractFieldsFromControlElements(
          controlElements, iframeElements, formFields, childFrames,
          fieldsExtracted, elementArray)) {
    return false;
  }

  if (formElement) {
    // Loop through the label elements inside the form element. For each label
    // element, get the corresponding form control element, use the form control
    // element along with |controlElements| and |elementArray| to find the
    // previously created AutofillFormFieldData and set the
    // AutofillFormFieldData's label.
    const labels = formElement.getElementsByTagName('label');
    matchLabelsAndFields(labels, formElement, controlElements, elementArray);
  } else {
    // Same as the if block, but for all the labels in fieldset
    for (const fieldset of fieldsets) {
      const labels = fieldset.getElementsByTagName('label');
      matchLabelsAndFields(labels, formElement, controlElements, elementArray);
    }
  }

  // Extract the frame tokens of `iframeElements`.
  if (childFrames.length != iframeElements.length) {
    // `extractFieldsFromControlElements` should create one entry in
    // `childFrames` for each entry in `iframeElements`. If this hasn't
    // happened, attempting to process the frames will cause errors, so early
    // return.
    return false;
  }
  for (let j = 0; j < iframeElements.length; ++j) {
    const frame = iframeElements[j]!;

    childFrames[j]!['token'] = registerChildFrame(frame);
  }

  // Loop through the form control elements, extracting the label text from
  // the DOM.  We use the |fieldsExtracted| vector to make sure we assign the
  // extracted label to the correct field, as it's possible |form_fields| will
  // not contain all of the elements in |control_elements|.
  for (let ctlElemIdx = 0, fieldIdx = 0, nextIframe = 0;
       ctlElemIdx < controlElements.length && fieldIdx < formFields.length;
       ++ctlElemIdx) {
    // This field didn't meet the requirements, so don't try to find a label
    // for it.
    if (!fieldsExtracted[ctlElemIdx]) {
      continue;
    }

    const controlElement = controlElements[ctlElemIdx];
    const currentField = formFields[fieldIdx]!;
    if (!currentField.label) {
      currentField.label =
          gCrWeb.fill.inferLabelForElement(controlElement)?.label || '';
    }
    if (currentField.label!.length > fillConstants.MAX_DATA_LENGTH) {
      currentField.label =
          currentField.label!.substr(0, fillConstants.MAX_DATA_LENGTH);
    }

    if (controlElement === formControlElement) {
      _field = formFields[fieldIdx];
    }

    // Finds the last frame that precedes |control_element|.
    while (nextIframe < iframeElements.length &&
           isDOMSuccessor(controlElement!, iframeElements[nextIframe]!)) {
      ++nextIframe;
    }
    // The |next_frame|th frame succeeds `control_element` and thus the last
    // added FormFieldData. The |k|th frames for |k| > |next_frame| may also
    // succeeds that FormFieldData. If they do not,
    // `child_frames[k].predecessor` will be updated in a later iteration.
    for (let k = nextIframe; k < iframeElements.length; ++k) {
      childFrames[k]!['predecessor'] = fieldIdx;
    }

    ++fieldIdx;
  }


  form.fields = formFields;
  // Protect against custom implementation of Array.toJSON in host pages.
  (form.fields as any).toJSON = null;

  form.host_frame = gCrWeb.message.getFrameId();

  if (childFrames.length > 0) {
    form.child_frames = childFrames;
    (form.child_frames as any).toJSON = null;
  }
  return true;
}

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
 * @param frame The window or frame where the
 *     formElement is in.
 * @param formElement The form element that will be processed.
 * @param formControlElement A control element in
 *     formElement, the FormField of which will be returned in field.
 * @param form Form to fill in the AutofillFormData
 *     information of formElement.
 * @param field Field to fill in the form field
 *     information of formControlElement.
 * @return Whether there are fields and not too many fields in the
 *     form.
 */
gCrWeb.fill.webFormElementToFormData = function(
    frame: Window, formElement: HTMLFormElement,
    formControlElement: fillConstants.FormControlElement,
    form: fillUtil.AutofillFormData,
    field?: fillUtil.AutofillFormFieldData): boolean {
  if (!frame) {
    return false;
  }

  form.name = gCrWeb.form.getFormIdentifier(formElement);
  form.origin = gCrWeb.common.removeQueryAndReferenceFromURL(frame.origin);
  form.action = gCrWeb.fill.getCanonicalActionForForm(formElement);

  // The raw name and id attributes, which may be empty.
  form.name_attribute = formElement.getAttribute('name') || '';
  form.id_attribute = formElement.getAttribute('id') || '';

  form.renderer_id = gCrWeb.fill.getUniqueID(formElement);

  form.host_frame = frame.__gCrWeb.message.getFrameId();

  // Note different from form_autofill_util.cc version of this method, which
  // computes |form.action| using document.completeURL(form_element.action())
  // and falls back to formElement.action() if the computed action is invalid,
  // here the action returned by |absoluteURL_| is always valid, which is
  // computed by creating a <a> element, and we don't check if the action is
  // valid.

  const controlElements = gCrWeb.form.getFormControlElements(formElement);

  const iframeElements =
    gCrWeb.autofill_form_features.isAutofillAcrossIframesEnabled() ?
    gCrWeb.form.getIframeElements(formElement) :
    [];

  return formOrFieldsetsToFormData(
      formElement, formControlElement, /*fieldsets=*/[], controlElements,
      iframeElements, form, field);
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
 * @param element The element to be processed.
 * @param field Field to fill in the element information.
 */
gCrWeb.fill.webFormControlElementToFormField = function(
    element: fillConstants.FormControlElement,
    field: fillUtil.AutofillFormFieldData) {
  if (!field || !element) {
    return;
  }
  // The label is not officially part of a form control element; however, the
  // labels for all form control elements are scraped from the DOM and set in
  // form data.
  field.identifier = gCrWeb.form.getFieldIdentifier(element);
  field.name = gCrWeb.form.getFieldName(element);

  // The raw name and id attributes, which may be empty.
  field.name_attribute = element.getAttribute('name') || '';
  field.id_attribute = element.getAttribute('id') || '';

  field.renderer_id = gCrWeb.fill.getUniqueID(element);

  field.form_control_type = element.type;
  const autocompleteAttribute = element.getAttribute('autocomplete');
  if (autocompleteAttribute) {
    field.autocomplete_attribute = autocompleteAttribute;
  }
  if (field.autocomplete_attribute != null &&
    field.autocomplete_attribute.length > fillConstants.MAX_DATA_LENGTH) {
    // Discard overly long attribute values to avoid DOS-ing the browser
    // process. However, send over a default string to indicate that the
    // attribute was present.
    field.autocomplete_attribute = 'x-max-data-length-exceeded';
  }

  const roleAttribute = element.getAttribute('role');
  if (roleAttribute && roleAttribute.toLowerCase() === 'presentation') {
    field.role = fillConstants.ROLE_ATTRIBUTE_PRESENTATION;
  }

  field.placeholder_attribute = element.getAttribute('placeholder') || '';
  if (field.placeholder_attribute != null &&
    field.placeholder_attribute.length > fillConstants.MAX_DATA_LENGTH) {
    // Discard overly long attribute values to avoid DOS-ing the browser
    // process. However, send over a default string to indicate that the
    // attribute was present.
    field.placeholder_attribute = 'x-max-data-length-exceeded';
  }

  field.aria_label = gCrWeb.fill.getAriaLabel(element);
  field.aria_description = gCrWeb.fill.getAriaDescription(element);

  if (!gCrWeb.fill.isAutofillableElement(element)) {
    return;
  }

  if (gCrWeb.fill.isAutofillableInputElement(element) ||
      inferenceUtil.isTextAreaElement(element) ||
      gCrWeb.fill.isSelectElement(element)) {
    field.is_autofilled = (element as any).isAutofilled;
    field.is_user_edited = gCrWeb.form.fieldWasEditedByUser(element);
    field.should_autocomplete = gCrWeb.fill.shouldAutocomplete(element);
    field.is_focusable = !element.disabled && !(element as any).readOnly &&
        element.tabIndex >= 0 && gCrWeb.fill.isVisibleNode(element);
  }

  if (gCrWeb.fill.isAutofillableInputElement(element)) {
    if (gCrWeb.fill.isTextInput(element)) {
      field.max_length = (element as HTMLInputElement).maxLength;
      if (field.max_length === -1) {
        // Take default value as defined by W3C.
        field.max_length = 524288;
      }
    }
    field.is_checkable = gCrWeb.fill.isCheckableElement(element);
  } else if (inferenceUtil.isTextAreaElement(element)) {
    // Nothing more to do in this case.
  } else {
    gCrWeb.fill.getOptionStringsFromElement(element, field);
  }

  let value = gCrWeb.fill.value(element);

  // There is a constraint on the maximum data length in method
  // WebFormControlElementToFormField() in form_autofill_util.h in order to
  // prevent a malicious site from DOS'ing the browser: http://crbug.com/49332,
  // which isn't really meaningful here, but we need to follow the same logic to
  // get the same form signature wherever possible (to get the benefits of the
  // existing crowdsourced field detection corpus).
  if (value.length > fillConstants.MAX_DATA_LENGTH) {
    value = value.substr(0, fillConstants.MAX_DATA_LENGTH);
  }
  field.value = value;
};

/**
 * Returns a serialized version of |form| to send to the host on form
 * submission.
 * The result string is similar to the result of calling |extractForms| filtered
 * on |form| (that is why a list is returned).
 *
 * @param form The form to serialize.
 * @return a JSON encoded version of |form|
 */
gCrWeb.fill.autofillSubmissionData = function(form: HTMLFormElement): string {
  const formData = new gCrWeb['common'].JSONSafeObject();
  gCrWeb['fill'].webFormElementToFormData(window, form, null, formData, null);
  return gCrWeb.stringify([formData]);
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
 * @param elements elements to look through.
 * @param fieldsets out param for unowned fieldsets.
 * @return The elements that are not part of a form.
 */
gCrWeb.fill.getUnownedAutofillableFormFieldElements = function(
    elements: fillConstants.FormControlElement[],
    fieldsets: Element[]): fillConstants.FormControlElement[] {
  const unownedFieldsetChildren: fillConstants.FormControlElement[] = [];
  for (const element of elements) {
    if (gCrWeb.form.isFormControlElement(element)) {
      if (!element.form) {
        unownedFieldsetChildren.push(element);
      }
    }

    if (gCrWeb.fill.hasTagName(element, 'fieldset') &&
        !gCrWeb.fill.isElementInsideFormOrFieldSet(element)) {
      fieldsets.push(element);
    }
  }
  return extractAutofillableElementsFromSet(unownedFieldsetChildren);
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
 * @param frame The window or frame where the
 *     formElement is in.
 * @param fieldsets The fieldsets to look through.
 * @param controlElements The control elements that
 *     will be processed.
 * @param restrictUnownedFieldsToFormlessCheckout whether forms made of
 *     unowned fields (i.e., not within a <form> tag) should be restricted to
 *     those that appear to be in a checkout flow.
 * @param form Form to fill in the AutofillFormData
 *     information of formElement.
 * @return Whether there are fields and not too many fields in the
 *     form.
 */
gCrWeb.fill.unownedFormElementsAndFieldSetsToFormData = function(
    frame: Window, fieldsets: Element[],
    controlElements: fillConstants.FormControlElement[],
    iframeElements: HTMLIFrameElement[],
    restrictUnownedFieldsToFormlessCheckout: boolean,
    form: fillUtil.AutofillFormData): boolean {
  if (!frame) {
    return false;
  }
  form.name = '';
  form.origin = gCrWeb.common.removeQueryAndReferenceFromURL(frame.origin);
  form.action = '';

  if (!restrictUnownedFieldsToFormlessCheckout) {
    // TODO(crbug.com/40266126): Pass iframe elements.
    return formOrFieldsetsToFormData(
        /*formElement=*/ null, /*formControlElement=*/ null, fieldsets,
        controlElements, /*iframeElements=*/ iframeElements, form);
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
    const keyword = keywords[index]!;
    if (title.includes(keyword) || path.includes(keyword)) {
      // TODO(crbug.com/40266126): Pass iframe elements.
      return formOrFieldsetsToFormData(
          /* formElement= */ null, /* formControlElement= */ null, fieldsets,
          controlElements, /* iframeElements= */ iframeElements, form);
    }
  }

  // Since it's not a checkout flow, only add fields that have a non-"off"
  // autocomplete attribute to the formless autofill.
  const controlElementsWithAutocomplete:
      fillConstants.FormControlElement[] = [];
  for (const controlElement of controlElements) {
    if (controlElement.hasAttribute('autocomplete') &&
        controlElement.getAttribute('autocomplete') !== 'off') {
      controlElementsWithAutocomplete.push(controlElement);
    }
  }

  if (controlElementsWithAutocomplete.length === 0) {
    return false;
  }
  // TODO(crbug.com/40266126): Pass iframe elements.
  return formOrFieldsetsToFormData(
      /* formElement= */ null, /* formControlElement= */ null, fieldsets,
      controlElementsWithAutocomplete, /* iframeElements= */ iframeElements,
      form);
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
 * @param controlElements Set of control elements.
 * @return The array of autofillable elements.
 */
function extractAutofillableElementsFromSet(
    controlElements: fillConstants.FormControlElement[])
    : fillConstants.FormControlElement[] {
  const autofillableElements
      : fillConstants.FormControlElement[] = [];
  for (const element of controlElements) {
    if (!gCrWeb.fill.isAutofillableElement(element)) {
      continue;
    }
    autofillableElements.push(element);
  }
  return autofillableElements;
}
