// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FormControlElement} from '//components/autofill/ios/form_util/resources/fill_constants.js';
import {inferLabelFromNext} from '//components/autofill/ios/form_util/resources/fill_element_inference.js';
import {findChildText} from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';
import type {AutofillFormFieldData} from '//components/autofill/ios/form_util/resources/fill_util.js';
import {removeQueryAndReferenceFromURL} from '//ios/web/public/js_messaging/resources/utils.js';

// Returns the URL for the frame to be set in the FormData.
export function getFrameUrlOrOrigin(frame: Window): string {
  if ((frame === frame.top) ||
      ((frame.location.href !== 'about:blank') &&
       (frame.location.href !== 'about:srcdoc'))) {
    // If the full URL is available, use it.
    return removeQueryAndReferenceFromURL(frame.location.href);
  } else {
    // Iframes might have empty own URLs, and they do not have access to the
    // parent frame URL, only to the origin. Use it as the only available data.
    return frame.origin;
  }
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
export function matchLabelsAndFields(
    labels: HTMLCollectionOf<HTMLLabelElement>,
    formElement: HTMLFormElement|null, controlElements: FormControlElement[],
    elementArray: AutofillFormFieldData[]) {
  for (let index = 0; index < labels.length; ++index) {
    const label = labels[index]!;
    const fieldElement = label!.control as FormControlElement;
    let fieldData: AutofillFormFieldData|null = null;
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

    if (!('label' in fieldData)) {
      fieldData.label = '';
    }
    let labelText = findChildText(label);
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
