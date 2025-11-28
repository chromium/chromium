// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//components/autofill/ios/form_util/resources/fill_util.js';

import * as fillConstants from '//components/autofill/ios/form_util/resources/fill_constants.js';
import * as inferenceUtil from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';
import * as fillUtil from '//components/autofill/ios/form_util/resources/fill_util.js';
import {formOrFieldsetsToFormData, getFrameUrlOrOrigin, webFormElementToFormData} from '//components/autofill/ios/form_util/resources/fill_web_form.js';
import {getFieldIdentifier} from '//components/autofill/ios/form_util/resources/form_utils.js';
import {gCrWeb, gCrWebLegacy} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {isTextField} from '//ios/web/public/js_messaging/resources/utils.js';

// This file provides methods used to fill forms in JavaScript.

// Requires functions from form.ts and child_frame_registration_lib.ts.

/**
 * Retrieves the registered 'autofill_form_features' CrWebApi
 * instance for use in this file.
 */
const autofillFormFeaturesApi =
  gCrWeb.getRegisteredApi('autofill_form_features');

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
gCrWebLegacy.fill.webFormControlElementToFormField = function(
    element: fillConstants.FormControlElement,
    field: fillUtil.AutofillFormFieldData) {
  if (!field || !element) {
    return;
  }
  // The label is not officially part of a form control element; however, the
  // labels for all form control elements are scraped from the DOM and set in
  // form data.
  field.identifier = getFieldIdentifier(element);
  field.name = gCrWebLegacy.form.getFieldName(element);

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
    field.is_user_edited = gCrWebLegacy.form.fieldWasEditedByUser(element);
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
};

/**
 * Returns a serialized version of |form| to send to the host on form
 * submission.
 *
 * @param form The form to serialize.
 * @return a JSON encoded version of |form|
 */
gCrWebLegacy.fill.autofillSubmissionData =
    function(form: HTMLFormElement): fillUtil.AutofillFormData {
  const formData = new gCrWebLegacy['common'].JSONSafeObject();
  webFormElementToFormData(window, form, null, formData);
  return formData;
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
gCrWebLegacy.fill.unownedFormElementsAndFieldSetsToFormData = function(
    frame: Window, fieldsets: Element[],
    controlElements: fillConstants.FormControlElement[],
    iframeElements: HTMLIFrameElement[],
    restrictUnownedFieldsToFormlessCheckout: boolean,
    form: fillUtil.AutofillFormData): boolean {
  if (!frame) {
    return false;
  }
  form.name = '';
  form.origin = getFrameUrlOrOrigin(frame);
  form.action = '';

  // To avoid performance bottlenecks, do not keep child frames if their
  // quantity exceeds the allowed threshold.
  if (iframeElements.length > fillConstants.MAX_EXTRACTABLE_FRAMES &&
    autofillFormFeaturesApi.getFunction('isAutofillAcrossIframesThrottlingEnabled')()) {
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

