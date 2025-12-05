// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//components/autofill/ios/form_util/resources/fill_util.js';

import * as fillConstants from '//components/autofill/ios/form_util/resources/fill_constants.js';
import type {AutofillFormData} from '//components/autofill/ios/form_util/resources/fill_util.js';
import {formOrFieldsetsToFormData, getFrameUrlOrOrigin, webFormElementToFormData} from '//components/autofill/ios/form_util/resources/fill_web_form.js';
import {gCrWeb, gCrWebLegacy} from '//ios/web/public/js_messaging/resources/gcrweb.js';

// This file provides methods used to fill forms in JavaScript.

// Requires functions from form.ts and child_frame_registration_lib.ts.

/**
 * Retrieves the registered 'autofill_form_features' CrWebApi
 * instance for use in this file.
 */
const autofillFormFeaturesApi =
  gCrWeb.getRegisteredApi('autofill_form_features');

/**
 * Returns a serialized version of |form| to send to the host on form
 * submission.
 *
 * @param form The form to serialize.
 * @return a JSON encoded version of |form|
 */
gCrWebLegacy.fill.autofillSubmissionData = function(form: HTMLFormElement):
    AutofillFormData {
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
    form: AutofillFormData): boolean {
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
