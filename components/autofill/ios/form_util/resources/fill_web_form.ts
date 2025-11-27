// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {registerChildFrame} from '//components/autofill/ios/form_util/resources/child_frame_registration_lib.js';
import type {FormControlElement} from '//components/autofill/ios/form_util/resources/fill_constants.js';
import {CHILD_FRAME_REMOTE_TOKEN_ATTRIBUTE, MAX_DATA_LENGTH, MAX_EXTRACTABLE_FIELDS, MAX_EXTRACTABLE_FRAMES} from '//components/autofill/ios/form_util/resources/fill_constants.js';
import {inferLabelForElement, inferLabelFromNext} from '//components/autofill/ios/form_util/resources/fill_element_inference.js';
import {findChildText, isAutofillableElement} from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';
import type {AutofillFormData, AutofillFormFieldData, FrameTokenWithPredecessor} from '//components/autofill/ios/form_util/resources/fill_util.js';
import {getCanonicalActionForForm, getUniqueID} from '//components/autofill/ios/form_util/resources/fill_util.js';
import {getFormControlElements, getFormIdentifier, getIframeElements} from '//components/autofill/ios/form_util/resources/form_utils.js';
import {gCrWeb, gCrWebLegacy} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {removeQueryAndReferenceFromURL} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * Retrieves the registered 'autofill_form_features' CrWebApi
 * instance for use in this file.
 */
const autofillFormFeaturesApi =
    gCrWeb.getRegisteredApi('autofill_form_features');

declare global {
  // Defines an additional property, `__gcrweb`, on the Window object.
  // This definition is needed in order to call into gCrWeb inside an iframe.
  interface Window {
    __gCrWeb: any;
  }
}

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
    formControlElement: FormControlElement|null, form: AutofillFormData,
    field?: AutofillFormFieldData,
    extractChildFrames: boolean = true): boolean {
  if (!frame) {
    return false;
  }

  form.name = getFormIdentifier(formElement);
  form.origin = getFrameUrlOrOrigin(frame);
  form.action =
      formElement !== null ? getCanonicalActionForForm(formElement) : '';

  // The raw name and id attributes, which may be empty.
  form.name_attribute = formElement?.getAttribute('name') || '';
  form.id_attribute = formElement?.getAttribute('id') || '';

  form.renderer_id = getUniqueID(formElement);

  form.host_frame = frame.__gCrWeb.getFrameId();

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
  if (iframeElements.length > MAX_EXTRACTABLE_FRAMES &&
      autofillFormFeaturesApi.getFunction(
          'isAutofillAcrossIframesThrottlingEnabled')()) {
    iframeElements = [];
  }

  return formOrFieldsetsToFormData(
      formElement, formControlElement, /*fieldsets=*/[], controlElements,
      iframeElements, form, field);
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
    form: AutofillFormData, _field?: AutofillFormFieldData): boolean {
  // This should be a map from a control element to the AutofillFormFieldData.
  // However, without Map support, it's just an Array of AutofillFormFieldData.
  const elementArray: AutofillFormFieldData[] = [];

  // The extracted FormFields.
  const formFields: AutofillFormFieldData[] = [];

  // The extracted child frames.
  const childFrames: FrameTokenWithPredecessor[] = [];

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
    if (currentField.label!.length > MAX_DATA_LENGTH) {
      currentField.label = currentField.label!.substr(0, MAX_DATA_LENGTH);
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
      frame.getAttribute(CHILD_FRAME_REMOTE_TOKEN_ATTRIBUTE);
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
    formFields: AutofillFormFieldData[],
    childFrames: FrameTokenWithPredecessor[], fieldsExtracted: boolean[],
    elementArray: Array<AutofillFormFieldData|null>): boolean {
  for (const _i of iframeElements) {
    childFrames.push({token: '', predecessor: -1});
  }

  if (!elementArray) {
    elementArray =
        new Array<AutofillFormFieldData|null>(controlElements.length);
  }

  for (let i = 0; i < controlElements.length; ++i) {
    fieldsExtracted[i] = false;
    elementArray[i] = null;

    const controlElement = controlElements[i] as FormControlElement;
    if (!isAutofillableElement(controlElement)) {
      continue;
    }

    // Create a new AutofillFormFieldData, fill it out and map it to the
    // field's name.
    const formField = new gCrWebLegacy['common'].JSONSafeObject();
    gCrWebLegacy.fill.webFormControlElementToFormField(
        controlElement, formField);
    formFields.push(formField);
    elementArray[i] = formField;
    fieldsExtracted[i] = true;

    // TODO(crbug.com/40266126): This loop should also track which control
    // element appears immediately before the frame, so its index can be
    // set as the frame predecessor.

    // To avoid overly expensive computation, we impose a maximum number of
    // allowable fields.
    if (formFields.length > MAX_EXTRACTABLE_FIELDS) {
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
