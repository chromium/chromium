// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {registerChildFrame} from '//components/autofill/ios/form_util/resources/child_frame_registration_lib.js';
import * as fillConstants from '//components/autofill/ios/form_util/resources/fill_constants.js';
import type {FormControlElement} from '//components/autofill/ios/form_util/resources/fill_constants.js';
import {inferLabelForElement, inferLabelFromNext} from '//components/autofill/ios/form_util/resources/fill_element_inference.js';
import * as inferenceUtil from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';
import * as fillUtil from '//components/autofill/ios/form_util/resources/fill_util.js';
import {getFieldIdentifier, getFormControlElements, getFormIdentifier, getIframeElements} from '//components/autofill/ios/form_util/resources/form_utils.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {isTextField, removeQueryAndReferenceFromURL, sendWebKitMessage, trim} from '//ios/web/public/js_messaging/resources/utils.js';

if (typeof document.__gCrWasEditedByUserMap === 'undefined') {
  document.__gCrWasEditedByUserMap = new WeakMap();
}

if (typeof document.__gCrFormSubmissionRegistry === 'undefined') {
  document.__gCrFormSubmissionRegistry = new WeakSet();
}

/**
 * A WeakMap to track if the current value of a field was entered by user or
 * programmatically.
 * If the map is null, the source of changed is not track.
 */
export const wasEditedByUser: WeakMap<any, any> =
    document.__gCrWasEditedByUserMap;

/**
 * Retrieves the registered 'autofill_form_features' CrWebApi
 * instance for use in this file.
 */
const autofillFormFeaturesApi =
    gCrWeb.getRegisteredApi('autofill_form_features');

// Returns the URL for the frame to be set in the FormData.
export function getFrameUrlOrOrigin(): string {
  if ((window === window.top) ||
      ((window.location.href !== 'about:blank') &&
       (window.location.href !== 'about:srcdoc'))) {
    // If the full URL is available, use it.
    return removeQueryAndReferenceFromURL(window.location.href);
  } else {
    // Iframes might have empty own URLs, and they do not have access to the
    // parent frame URL, only to the origin. Use it as the only available data.
    return window.origin;
  }
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
export function webFormElementToFormData(
    frame: Window, formElement: HTMLFormElement,
    formControlElement: FormControlElement|null,
    form: fillUtil.AutofillFormData, field?: fillUtil.AutofillFormFieldData,
    extractChildFrames: boolean = true): boolean {
  if (!frame) {
    return false;
  }

  form.name = getFormIdentifier(formElement);
  form.origin = getFrameUrlOrOrigin();
  form.action = formElement !== null ?
      fillUtil.getCanonicalActionForForm(formElement) :
      '';

  // The raw name and id attributes, which may be empty.
  form.name_attribute = formElement?.getAttribute('name') || '';
  form.id_attribute = formElement?.getAttribute('id') || '';

  form.renderer_id = fillUtil.getUniqueID(formElement);

  form.host_frame = gCrWeb.getFrameId();

  // Note different from form_autofill_util.cc version of this method, which
  // computes |form.action| using document.completeURL(form_element.action())
  // and falls back to formElement.action() if the computed action is invalid,
  // here the action returned by |absoluteURL_| is always valid, which is
  // computed by creating a <a> element, and we don't check if the action is
  // valid.

  const controlElements =
      getFormControlElements(formElement) as FormControlElement[];

  let iframeElements = extractChildFrames &&
          autofillFormFeaturesApi.getFunction(
              'isAutofillAcrossIframesEnabled')() ?
      getIframeElements(formElement) :
      [];

  // To avoid performance bottlenecks, do not keep child frames if their
  // quantity exceeds the allowed threshold.
  if (iframeElements.length > fillConstants.MAX_EXTRACTABLE_FRAMES &&
      autofillFormFeaturesApi.getFunction(
          'isAutofillAcrossIframesThrottlingEnabled')()) {
    iframeElements = [];
  }

  return formOrFieldsetsToFormData(
      formElement, formControlElement, /*fieldsets=*/[], controlElements,
      iframeElements, form, field);
}

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
export function webFormControlElementToFormField(
    element: FormControlElement, field: fillUtil.AutofillFormFieldData) {
  if (!field || !element) {
    return;
  }
  // The label is not officially part of a form control element; however, the
  // labels for all form control elements are scraped from the DOM and set in
  // form data.
  field.identifier = getFieldIdentifier(element);
  field.name = getFieldName(element);

  // The raw name and id attributes, which may be empty.
  field.name_attribute = element.getAttribute('name') || '';
  field.id_attribute = element.getAttribute('id') || '';

  field.renderer_id = fillUtil.getUniqueID(element);

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

  field.pattern_attribute = element.getAttribute('pattern') ?? '';

  field.placeholder_attribute = element.getAttribute('placeholder') || '';
  if (field.placeholder_attribute != null &&
      field.placeholder_attribute.length > fillConstants.MAX_DATA_LENGTH) {
    // Discard overly long attribute values to avoid DOS-ing the browser
    // process. However, send over a default string to indicate that the
    // attribute was present.
    field.placeholder_attribute = 'x-max-data-length-exceeded';
  }

  field.aria_label = fillUtil.getAriaLabel(element);
  field.aria_description = fillUtil.getAriaDescription(element);

  if (!inferenceUtil.isAutofillableElement(element)) {
    return;
  }

  if (inferenceUtil.isAutofillableInputElement(element) ||
      inferenceUtil.isTextAreaElement(element) ||
      inferenceUtil.isSelectElement(element)) {
    field.is_autofilled = (element as any).isAutofilled;
    field.is_user_edited_deprecated = fieldWasEditedByUser(element);
    field.should_autocomplete = fillUtil.shouldAutocomplete(element);
    field.is_focusable = !element.disabled && !(element as any).readOnly &&
        element.tabIndex >= 0 && fillUtil.isVisibleNode(element);
  }

  if (inferenceUtil.isAutofillableInputElement(element)) {
    if (isTextField(element)) {
      field.max_length = (element as HTMLInputElement).maxLength;
      if (field.max_length === -1) {
        // Take default value as defined by W3C.
        field.max_length = 524288;
      }
    }
    field.is_checkable = inferenceUtil.isCheckableElement(element);
  } else if (inferenceUtil.isTextAreaElement(element)) {
    // Nothing more to do in this case.
  } else {
    fillUtil.getOptionStringsFromElement(element as HTMLSelectElement, field);
  }

  let value = fillUtil.valueForElement(element);

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
export function formOrFieldsetsToFormData(
    formElement: HTMLFormElement|null,
    formControlElement: FormControlElement|null, fieldsets: Element[],
    controlElements: FormControlElement[], iframeElements: HTMLIFrameElement[],
    form: fillUtil.AutofillFormData,
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
  if (childFrames.length !== iframeElements.length) {
    // `extractFieldsFromControlElements` should create one entry in
    // `childFrames` for each entry in `iframeElements`. If this hasn't
    // happened, attempting to process the frames will cause errors, so early
    // return.
    return false;
  }
  for (let j = 0; j < iframeElements.length; ++j) {
    const frame = iframeElements[j]!;

    childFrames[j]!['token'] = getChildFrameRemoteToken(frame) ?? '';
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
      // TODO(crbug.com/454044167): Cleanup autofill TS type casting.
      currentField.label =
          inferLabelForElement(controlElement as FormControlElement)?.label ||
          '';
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

  form.host_frame = gCrWeb.getFrameId();

  if (childFrames.length > 0) {
    form.child_frames = childFrames;
    (form.child_frames as any).toJSON = null;
  }
  return true;
}

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
export function unownedFormElementsAndFieldSetsToFormData(
    frame: Window, fieldsets: Element[], controlElements: FormControlElement[],
    iframeElements: HTMLIFrameElement[],
    restrictUnownedFieldsToFormlessCheckout: boolean,
    form: fillUtil.AutofillFormData): boolean {
  if (!frame) {
    return false;
  }
  form.name = '';
  form.origin = getFrameUrlOrOrigin();
  form.action = '';

  // To avoid performance bottlenecks, do not keep child frames if their
  // quantity exceeds the allowed threshold.
  if (iframeElements.length > fillConstants.MAX_EXTRACTABLE_FRAMES &&
      autofillFormFeaturesApi.getFunction(
          'isAutofillAcrossIframesThrottlingEnabled')()) {
    iframeElements = [];
  }

  if (!restrictUnownedFieldsToFormlessCheckout) {
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
      return formOrFieldsetsToFormData(
          /* formElement= */ null, /* formControlElement= */ null, fieldsets,
          controlElements, /* iframeElements= */ iframeElements, form);
    }
  }

  // Since it's not a checkout flow, only add fields that have a non-"off"
  // autocomplete attribute to the formless autofill.
  const controlElementsWithAutocomplete: FormControlElement[] = [];
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
}

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
    formElement: HTMLFormElement|null, controlElements: FormControlElement[],
    elementArray: fillUtil.AutofillFormFieldData[]) {
  for (let index = 0; index < labels.length; ++index) {
    const label = labels[index]!;
    const fieldElement = label!.control as FormControlElement;
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
    } else if (fieldElement.form !== formElement || fieldElement.hidden) {
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

    if (!fieldData.label) {
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

/**
 Returns a remote frame token associated to a child frame. When called from the
 isolated world a new token is generated and `frame` registers itself with it.
 When called from the page world, the last generated token in the isolated world
 is returned.
 */
function getChildFrameRemoteToken(frame: HTMLIFrameElement|null): string|null {
  if (!frame) {
    return null;
  }
  // Either register a new token when in the isolated world or read the last
  // registered token from the page content world.
  return registerChildFrame(frame) ??
      frame.getAttribute(fillConstants.CHILD_FRAME_REMOTE_TOKEN_ATTRIBUTE);
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
    controlElements: FormControlElement[], iframeElements: HTMLIFrameElement[],
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

    const controlElement = controlElements[i] as FormControlElement;
    if (!inferenceUtil.isAutofillableElement(controlElement)) {
      continue;
    }

    // Create a new AutofillFormFieldData, fill it out and map it to the
    // field's name.
    const formField = new fillUtil.AutofillFormFieldData();
    webFormControlElementToFormField(controlElement, formField);
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

// Returns true if the node `a` is a successor of node `b` if they have a common
// root node.
function isDOMSuccessor(a: Node, b: Node): boolean {
  return (a.compareDocumentPosition(b) & Node.DOCUMENT_POSITION_PRECEDING) > 0;
}

/**
 * Returns the field's `name` attribute if not space only; otherwise the
 * field's `id` attribute.
 *
 * The name will be used as a hint to infer the autofill type of the field.
 *
 * It aims to provide the logic in
 *     WebString nameForAutofill() const;
 * in chromium/src/third_party/WebKit/Source/WebKit/chromium/public/
 *  WebFormControlElement.h
 *
 * @param element An element of which the name for Autofill will be returned.
 * @return the name for Autofill.
 */
export function getFieldName(element: Element|null): string {
  if (!element) {
    return '';
  }

  if ('name' in element && element.name) {
    const trimmedName = trim(element.name as string);
    if (trimmedName.length > 0) {
      return trimmedName;
    }
  }

  if (element.id) {
    return trim(element.id);
  }

  return '';
}

/**
 * Returns a serialized version of |form| to send to the host on form
 * submission.
 *
 * @param form The form to serialize.
 * @return a JSON encoded version of |form|
 */
export function autofillSubmissionData(form: HTMLFormElement):
    fillUtil.AutofillFormData {
  const formData = new fillUtil.AutofillFormData();
  webFormElementToFormData(window, form, null, formData);
  return formData;
}

/**
 * Returns whether the last `input` or `change` event on `element` was
 * triggered by a user action (was "trusted"). Returns true by default if the
 * feature to fix the user edited bit isn't enabled which is the status quo.
 * TODO(crbug.com/40941928): Match Blink's behavior so that only a 'reset' event
 * makes an edited field unedited.
 */
export function fieldWasEditedByUser(element: Element) {
  return !autofillFormFeaturesApi.getFunction(
             'isAutofillCorrectUserEditedBitInParsedField')() ||
      (wasEditedByUser.get(element) ?? false);
}

/**
 * @param originalURL A string containing a URL (absolute, relative...)
 * @return A string containing a full URL (absolute with scheme)
 */
function getFullyQualifiedUrl(originalURL: string): string {
  // A dummy anchor (never added to the document) is used to obtain the
  // fully-qualified URL of `originalURL`.
  const anchor = document.createElement('a');
  anchor.href = originalURL;
  return anchor.href;
}

// Send the form data to the browser.
export function formSubmittedInternal(
    form: HTMLFormElement,
    messageHandler: string,
    programmaticSubmission: boolean,
    includeRemoteFrameToken: boolean = false,
    ): void {
  if (autofillFormFeaturesApi.getFunction(
          'isAutofillDedupeFormSubmissionEnabled')()) {
    // Handle deduping when the feature allows it.
    if (document.__gCrFormSubmissionRegistry.has(form)) {
      // Do not double submit the same form.
      return;
    }
    document.__gCrFormSubmissionRegistry.add(form);
  }

  // Default URL for action is the document's URL.
  const action = form.getAttribute('action') || document.URL;

  const message = {
    command: 'form.submit',
    frameID: gCrWeb.getFrameId(),
    formName: getFormIdentifier(form),
    href: getFullyQualifiedUrl(action),
    formData: autofillSubmissionData(form),
    remoteFrameToken: includeRemoteFrameToken ? fillUtil.getRemoteFrameToken() :
                                                undefined,
    programmaticSubmission: programmaticSubmission,
  };

  sendWebKitMessage(messageHandler, message);
}

/**
 * Sends the form data to the browser. Errors that are caught via the try/catch
 * are reported to the browser. This is done before the error bubbles above
 * `formSubmitted()` so the generic JS errors wrapper doesn't intercept the
 * error before this custom error handler.
 *
 * @param form The form that was submitted.
 * @param messageHandler The name of the message handler to send the message to.
 * @param programmaticSubmission True if the form submission is programmatic.
 * @includeRemoteFrameToken True if the remote frame token should be included
 *   in the payload of the message sent to the browser.
 */
export function formSubmitted(
    form: HTMLFormElement,
    messageHandler: string,
    programmaticSubmission: boolean,
    includeRemoteFrameToken: boolean = false,
    ): void {
  try {
    formSubmittedInternal(
        form, messageHandler, programmaticSubmission, includeRemoteFrameToken);
  } catch (error) {
    if (autofillFormFeaturesApi.getFunction(
            'isAutofillReportFormSubmissionErrorsEnabled')()) {
      reportFormSubmissionError(error, programmaticSubmission, messageHandler);
    } else {
      // Just let the error go through if not reported.
      throw error;
    }
  }
}

/**
 * Reports a form submission error to the browser.
 * @param error Object that holds information on the error.
 * @param programmaticSubmission True if the submission that errored was
 *   programmatic.
 * @param handler The name of the handler to send the error message to.
 */
export function reportFormSubmissionError(
    error: any, programmaticSubmission: boolean, handler: string) {
  let errorMessage = '';
  let errorStack = '';
  if (error && error instanceof Error) {
    errorMessage = error.message;
    if (error.stack) {
      errorStack = error.stack;
    }
  }

  const message = {
    command: 'form.submit.error',
    errorStack,
    errorMessage,
    programmaticSubmission,
  };
  sendWebKitMessage(handler, message);
}
