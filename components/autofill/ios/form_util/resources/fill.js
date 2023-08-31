// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//components/autofill/ios/form_util/resources/create_fill_namespace.js';
import '//components/autofill/ios/form_util/resources/fill_constants.js';
import '//components/autofill/ios/form_util/resources/fill_element_inference.js';

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
 * Maps elements using their unique ID
 */
const elementMap = new Map();

/**
 * Acquires the specified DOM |attribute| from the DOM |element| and returns
 * its lower-case value, or null if not present.
 *
 * @param {Element} element A DOM element.
 * @param {string} attribute An attribute name.
 * @return {?string} Lowercase value of DOM element or null if not present.
 */
function getLowerCaseAttribute_(element, attribute) {
  if (!element) {
    return null;
  }
  const value = element.getAttribute(attribute);
  if (value) {
    return value.toLowerCase();
  }
  return null;
}

/**
 * Returns true if an element can be autocompleted.
 *
 * This method aims to provide the same logic as method
 *     bool autoComplete() const
 * in chromium/src/third_party/WebKit/Source/WebKit/chromium/src/
 * WebFormElement.cpp.
 *
 * @param {Element} element An element to check if it can be autocompleted.
 * @return {boolean} true if element can be autocompleted.
 */
__gCrWeb.fill.autoComplete = function(element) {
  if (!element) {
    return false;
  }
  if (getLowerCaseAttribute_(element, 'autocomplete') === 'off') {
    return false;
  }
  if (getLowerCaseAttribute_(element.form, 'autocomplete') == 'off') {
    return false;
  }
  return true;
};

/**
 * Returns true if an element should suggest autocomplete dropdown.
 *
 * @param {Element} element An element to check if it can be autocompleted.
 * @return {boolean} true if autocomplete dropdown should be suggested.
 */
__gCrWeb.fill.shouldAutocomplete = function(element) {
  if (!__gCrWeb.fill.autoComplete(element)) {
    return false;
  }
  if (getLowerCaseAttribute_(element, 'autocomplete') === 'one-time-code') {
    return false;
  }
  if (getLowerCaseAttribute_(element.form, 'autocomplete') ===
      'one-time-code') {
    return false;
  }
  return true;
};

/**
 * Sets the value of a data-bound input using AngularJS.
 *
 * The method first set the value using the val() method. Then, if input is
 * bound to a model value, it sets the model value.
 * Documentation of relevant modules of AngularJS can be found at
 * https://docs.angularjs.org/guide/databinding
 * https://docs.angularjs.org/api/auto/service/$injector
 * https://docs.angularjs.org/api/ng/service/$parse
 *
 * @param {string} value The value the input element will be set.
 * @param {Element} input The input element of which the value is set.
 */
function setInputElementAngularValue_(value, input) {
  if (!input || !window['angular']) {
    return;
  }
  const angularElement =
      window['angular'].element && window['angular'].element(input);
  if (!angularElement) {
    return;
  }
  angularElement.val(value);
  const angularModel = angularElement.data && angularElement.data('ngModel');
  const angularScope = angularElement.scope();
  if (!angularModel || !angularScope) {
    return;
  }
  angularElement.injector().invoke([
    '$parse',
    function(parse) {
      const setter = parse(angularModel);
      setter.assign(angularScope, value);
    },
  ]);
}

/**
 * Sets the value of an input, dispatches the events on the changed element and
 * call |callback| if it is defined.
 *
 * It is based on the logic in
 *
 *     void setValue(const WebString&, bool sendChangeEvent = false)
 *
 * in chromium/src/third_party/WebKit/Source/WebKit/chromium/src/
 * WebInputElement.cpp, which calls
 *    void setValue(const String& value, TextFieldEventBehavior eventBehavior)
 * or
 *    void setChecked(bool nowChecked, TextFieldEventBehavior eventBehavior)
 * in chromium/src/third_party/WebKit/Source/core/html/HTMLInputElement.cpp.
 *
 * @param {string} value The value the input element will be set.
 * @param {Element} input The input element of which the value is set.
 * @param {function()=} callback Callback function called after the input
 *     element's value is changed.
 * @return {boolean} Whether the value has been set successfully.
 */
__gCrWeb.fill.setInputElementValue = function(
    value, input, callback = undefined) {
  if (!input) {
    return false;
  }

  const activeElement = document.activeElement;
  if (input !== activeElement) {
    __gCrWeb.fill.createAndDispatchHTMLEvent(
        activeElement, value, 'blur', true, false);
    __gCrWeb.fill.createAndDispatchHTMLEvent(
        input, value, 'focus', true, false);
  }

  const filled = setInputElementValue_(value, input);
  if (callback) {
    callback();
  }

  if (input !== activeElement) {
    __gCrWeb.fill.createAndDispatchHTMLEvent(input, value, 'blur', true, false);
    __gCrWeb.fill.createAndDispatchHTMLEvent(
        activeElement, value, 'focus', true, false);
  }
  return filled;
};

/**
 * Internal function to set the element value.
 *
 * @param {string} value The value the input element will be set.
 * @param {Element} input The input element of which the value is set.
 * @return {boolean} Whether the value has been set successfully.
 */
function setInputElementValue_(value, input) {
  const propertyName = (input.type === 'checkbox' || input.type === 'radio') ?
      'checked' :
      'value';
  if (input.type !== 'select-one' && input.type !== 'checkbox' &&
      input.type !== 'radio') {
    // In HTMLInputElement.cpp there is a check on canSetValue(value), which
    // returns false only for file input. As file input is not relevant for
    // autofill and this method is only used for autofill for now, there is no
    // such check in this implementation.
    value = __gCrWeb.fill.sanitizeValueForInputElement(value, input);
  }

  // Return early if the value hasn't changed.
  if (input[propertyName] === value) {
    return false;
  }

  // When the user inputs a value in an HTMLInput field, the property setter is
  // not called. The different frameworks often call it explicitly when
  // receiving the input event.
  // This is probably due to the sync between the HTML object and the DOM
  // object.
  // The sequence of event is: User input -> input event -> setter.
  // When the property is set programmatically (input.value = 'foo'), the setter
  // is called immediately (then probably called again on the input event)
  // JS input -> setter.
  // The only way to emulate the user behavior is to override the property
  // The getter will return the new value to emulate the fact the the HTML
  // value was updated without calling the setter.
  // The setter simply forwards the set to the older property descriptor.
  // Once the setter has been called, just forward get and set calls.

  const oldPropertyDescriptor = /** @type {!Object} */ (
      Object.getOwnPropertyDescriptor(input, propertyName));
  const overrideProperty =
      oldPropertyDescriptor && oldPropertyDescriptor.configurable;
  let setterCalled = false;

  if (overrideProperty) {
    const newProperty = {
      get() {
        if (setterCalled && oldPropertyDescriptor.get) {
          return oldPropertyDescriptor.get.call(input);
        }
        // Simulate the fact that the HTML value has been set but not yet the
        // property.
        return value + '';
      },
      configurable: true,
    };
    if (oldPropertyDescriptor.set) {
      newProperty.set = function(e) {
        setterCalled = true;
        oldPropertyDescriptor.set.call(input, value);
      };
    }
    Object.defineProperty(input, propertyName, newProperty);
  } else {
    setterCalled = true;
    input[propertyName] = value;
  }

  if (window['angular']) {
    // The page uses the AngularJS framework. Update the angular value before
    // sending events.
    setInputElementAngularValue_(value, input);
  }
  __gCrWeb.fill.notifyElementValueChanged(input, value);

  if (overrideProperty) {
    Object.defineProperty(input, propertyName, oldPropertyDescriptor);
    if (!setterCalled && input[propertyName] !== value) {
      // The setter was never called. This may be intentional (the framework
      // ignored the input event) or not (the event did not conform to what
      // framework expected). The whole function will likely fail, but try to
      // set the value directly as a last try.
      input[propertyName] = value;
    }
  }
  return true;
}

/**
 * Returns a sanitized value of proposedValue for a given input element type.
 * The logic is based on
 *
 *      String sanitizeValue(const String&) const
 *
 * in chromium/src/third_party/WebKit/Source/core/html/InputType.h
 *
 * @param {string} proposedValue The proposed value.
 * @param {Element} element The element for which the proposedValue is to be
 *     sanitized.
 * @return {string} The sanitized value.
 */
__gCrWeb.fill.sanitizeValueForInputElement = function(proposedValue, element) {
  if (!proposedValue) {
    return '';
  }

  // Method HTMLInputElement::sanitizeValue() calls InputType::sanitizeValue()
  // (chromium/src/third_party/WebKit/Source/core/html/InputType.cpp) for
  // non-null proposedValue. InputType::sanitizeValue() returns the original
  // proposedValue by default and it is overridden in classes
  // BaseDateAndTimeInputType, ColorInputType, RangeInputType and
  // TextFieldInputType (all are in
  // chromium/src/third_party/WebKit/Source/core/html/). Currently only
  // TextFieldInputType is relevant and sanitizeValue() for other types of
  // input elements has not been implemented.
  if (__gCrWeb.common.isTextField(element)) {
    return __gCrWeb.fill.sanitizeValueForTextFieldInputType(
        proposedValue, element);
  }
  return proposedValue;
};

/**
 * Returns a sanitized value for a text field.
 *
 * The logic is based on |String sanitizeValue(const String&)|
 * in chromium/src/third_party/WebKit/Source/core/html/TextFieldInputType.h
 * Note this method is overridden in EmailInputType and NumberInputType.
 *
 * @param {string} proposedValue The proposed value.
 * @param {Element} element The element for which the proposedValue is to be
 *     sanitized.
 * @return {string} The sanitized value.
 */
__gCrWeb.fill.sanitizeValueForTextFieldInputType = function(
    proposedValue, element) {
  const textFieldElementType = element.type;
  if (textFieldElementType === 'email') {
    return __gCrWeb.fill.sanitizeValueForEmailInputType(proposedValue, element);
  } else if (textFieldElementType === 'number') {
    return __gCrWeb.fill.sanitizeValueForNumberInputType(proposedValue);
  }
  const valueWithLineBreakRemoved = proposedValue.replace(/(\r\n|\n|\r)/gm, '');
  // TODO(chenyu): Should we also implement numCharactersInGraphemeClusters()
  // in chromium/src/third_party/WebKit/Source/core/platform/text/
  // TextBreakIterator.cpp and call it here when computing newLength?
  // Different from the implementation in TextFieldInputType.h, where a limit
  // on the text length is considered due to
  // https://bugs.webkit.org/show_bug.cgi?id=14536, no such limit is
  // considered here for now.
  let newLength = valueWithLineBreakRemoved.length;
  // This logic is from method String limitLength() in TextFieldInputType.h
  for (let i = 0; i < newLength; ++i) {
    const current = valueWithLineBreakRemoved[i];
    if (current < ' ' && current !== '\t') {
      newLength = i;
      break;
    }
  }
  return valueWithLineBreakRemoved.substring(0, newLength);
};

/**
 * Returns the sanitized value for an email input.
 *
 * The logic is based on
 *
 *     String EmailInputType::sanitizeValue(const String& proposedValue) const
 *
 * in chromium/src/third_party/WebKit/Source/core/html/EmailInputType.cpp
 *
 * @param {string} proposedValue The proposed value.
 * @param {Element} element The element for which the proposedValue is to be
 *     sanitized.
 * @return {string} The sanitized value.
 */
__gCrWeb.fill.sanitizeValueForEmailInputType = function(
    proposedValue, element) {
  const valueWithLineBreakRemoved = proposedValue.replace(/(\r\n|\n\r)/gm, '');

  if (!element.multiple) {
    return __gCrWeb.common.trim(proposedValue);
  }
  const addresses = valueWithLineBreakRemoved.split(',');
  for (let i = 0; i < addresses.length; ++i) {
    addresses[i] = __gCrWeb.common.trim(addresses[i]);
  }
  return addresses.join(',');
};

/**
 * Returns the sanitized value of a proposed value for a number input.
 *
 * The logic is based on
 *
 *     String NumberInputType::sanitizeValue(const String& proposedValue)
 *         const
 *
 * in chromium/src/third_party/WebKit/Source/core/html/NumberInputType.cpp
 *
 * Note in this implementation method Number() is used in the place of method
 * parseToDoubleForNumberType() called in NumberInputType.cpp.
 *
 * @param {string} proposedValue The proposed value.
 * @return {string} The sanitized value.
 */
__gCrWeb.fill.sanitizeValueForNumberInputType = function(proposedValue) {
  const sanitizedValue = Number(proposedValue);
  if (isNaN(sanitizedValue)) {
    return '';
  }
  return sanitizedValue.toString();
};

/**
 * Trims any whitespace from the start and end of a string.
 * Used in preference to String.prototype.trim as this can be overridden by
 * sites.
 *
 * @param {string} str The string to be trimmed.
 * @return {string} The string after trimming.
 */


/**
 * Creates and sends notification that element has changed.
 *
 * Send events that 'mimic' the user typing in a field.
 * 'input' event is often use in case of a text field, and 'change'event is
 * more often used in case of selects.
 *
 * @param {Element} element The element that changed.
 */
__gCrWeb.fill.notifyElementValueChanged = function(element, value) {
  __gCrWeb.fill.createAndDispatchHTMLEvent(
      element, value, 'keydown', true, false);
  __gCrWeb.fill.createAndDispatchHTMLEvent(
      element, value, 'keypress', true, false);
  __gCrWeb.fill.createAndDispatchHTMLEvent(
      element, value, 'input', true, false);
  __gCrWeb.fill.createAndDispatchHTMLEvent(
      element, value, 'keyup', true, false);
  __gCrWeb.fill.createAndDispatchHTMLEvent(
      element, value, 'change', true, false);
};

/**
 * Creates and dispatches an HTML event.
 *
 * @param {Element} element The element for which an event is created.
 * @param {string} type The type of the event.
 * @param {boolean} bubbles A boolean indicating whether the event should
 *     bubble up through the event chain or not.
 * @param {boolean} cancelable A boolean indicating whether the event can be
 *     canceled.
 */
__gCrWeb.fill.createAndDispatchHTMLEvent = function(
    element, value, type, bubbles, cancelable) {
  const event =
      new Event(type, {bubbles: bubbles, cancelable: cancelable, data: value});
  if (type === 'input') {
    event.inputType = 'insertText';
  }
  element.dispatchEvent(event);
};

/**
 * Converts a relative URL into an absolute URL.
 *
 * @param {Object} doc Document.
 * @param {string} relativeURL Relative URL.
 * @return {string} Absolute URL.
 */
function absoluteURL_(doc, relativeURL) {
  // In the case of data: URL-based pages, relativeURL === absoluteURL.
  if (doc.location.protocol === 'data:') {
    return doc.location.href;
  }
  let urlNormalizer = doc['__gCrWebURLNormalizer'];
  if (!urlNormalizer) {
    urlNormalizer = doc.createElement('a');
    doc['__gCrWebURLNormalizer'] = urlNormalizer;
  }

  // Use the magical quality of the <a> element. It automatically converts
  // relative URLs into absolute ones.
  urlNormalizer.href = relativeURL;
  return urlNormalizer.href;
}

/**
 * Returns a canonical action for |formElement|. It works the same as upstream
 * function GetCanonicalActionForForm.
 * @param {HTMLFormElement} formElement
 * @return {string} Canonical action.
 */
__gCrWeb.fill.getCanonicalActionForForm = function(formElement) {
  const rawAction = formElement.getAttribute('action') || '';
  const absoluteUrl = absoluteURL_(formElement.ownerDocument, rawAction);
  return __gCrWeb.common.removeQueryAndReferenceFromURL(absoluteUrl);
};

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
    if (formFields.length > __gCrWeb.fill.MAX_EXTRACTABLE_FIELDS) {
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
    let labelText = __gCrWeb.fill.findChildText(label);
    if (labelText.length === 0 && !label.htmlFor) {
      labelText = __gCrWeb.fill.inferLabelFromNext(fieldElement);
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
    if (currentField['label'].length > __gCrWeb.fill.MAX_DATA_LENGTH) {
      currentField['label'] =
          currentField['label'].substr(0, __gCrWeb.fill.MAX_DATA_LENGTH);
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
 * Fills |field| data with the values of the <option> elements present in
 * |selectElement|.
 *
 * It is based on the logic in
 *     void GetOptionStringsFromElement(const WebSelectElement& select_element,
 *                                      std::vector<string16>* option_values,
 *                                      std::vector<string16>* option_contents)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {Element} selectElement A select element from which option data are
 *     extracted.
 * @param {Object} field A field that will contain the extracted option
 *     information.
 */
__gCrWeb.fill.getOptionStringsFromElement = function(selectElement, field) {
  field['option_values'] = [];
  // Protect against custom implementation of Array.toJSON in host pages.
  field['option_values'].toJSON = null;
  field['option_contents'] = [];
  field['option_contents'].toJSON = null;
  const options = selectElement.options;
  for (let i = 0; i < options.length; ++i) {
    const option = options[i];
    field['option_values'].push(
        option['value'].substring(0, __gCrWeb.fill.MAX_STRING_LENGTH));
    field['option_contents'].push(
        option['text'].substring(0, __gCrWeb.fill.MAX_STRING_LENGTH));
  }
};

/**
 * Returns true if |element| is a text input element.
 *
 * It is based on the logic in
 *     bool IsTextInput(const blink::WebInputElement* element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {boolean} Whether element is a text input field.
 */
__gCrWeb.fill.isTextInput = function(element) {
  if (!element) {
    return false;
  }
  return __gCrWeb.common.isTextField(element);
};

/**
 * Returns true if |element| is a 'select' element.
 *
 * It is based on the logic in
 *     bool IsSelectElement(const blink::WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {FormControlElement|HTMLOptionElement} element An element to examine.
 * @return {boolean} Whether element is a 'select' element.
 */
__gCrWeb.fill.isSelectElement = function(element) {
  if (!element) {
    return false;
  }
  return element.type === 'select-one';
};

/**
 * Returns true if |element| is a 'textarea' element.
 *
 * It is based on the logic in
 *     bool IsTextAreaElement(const blink::WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {boolean} Whether element is a 'textarea' element.
 */
__gCrWeb.fill.isTextAreaElement = function(element) {
  if (!element) {
    return false;
  }
  return element.type === 'textarea';
};

/**
 * Returns true if |element| is a checkbox or a radio button element.
 *
 * It is based on the logic in
 *     bool IsCheckableElement(const blink::WebInputElement* element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {boolean} Whether element is a checkbox or a radio button.
 */
__gCrWeb.fill.isCheckableElement = function(element) {
  if (!element) {
    return false;
  }
  return element.type === 'checkbox' || element.type === 'radio';
};

/**
 * Returns true if |element| is one of the input element types that can be
 * autofilled. {Text, Radiobutton, Checkbox}.
 *
 * It is based on the logic in
 *    bool IsAutofillableInputElement(const blink::WebInputElement* element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {boolean} Whether element is one of the input element types that
 *     can be autofilled.
 */
__gCrWeb.fill.isAutofillableInputElement = function(element) {
  return __gCrWeb.fill.isTextInput(element) ||
      __gCrWeb.fill.isCheckableElement(element);
};

/**
 * Returns the nodeValue in a way similar to the C++ version of node.nodeValue,
 * used in src/components/autofill/content/renderer/form_autofill_util.h.
 * Newlines and tabs are stripped.
 *
 * @param {Node} node A node to examine.
 * @return {string} The text contained in |element|.
 */
__gCrWeb.fill.nodeValue = function(node) {
  return (node.nodeValue || '').replace(/[\n\t]/gm, '');
};

/**
 * Returns the value in a way similar to the C++ version of node.value,
 * used in src/components/autofill/content/renderer/form_autofill_util.h.
 * Newlines and tabs are stripped.
 *
 * Note: this method tries to match the behavior of Blink for the select
 * element. On Blink, a select element with a first option that is disabled and
 * not explicitly selected will automatically select the second element.
 * On WebKit, the disabled element is enabled until user interacts with it.
 * As the result of this method will be used by code written for Blink, match
 * the behavior on it.
 *
 * @param {FormControlElement|HTMLOptionElement} element An element to examine.
 * @return {string} The value for |element|.
 */
__gCrWeb.fill.value = function(element) {
  let value = element.value;
  if (__gCrWeb.fill.isSelectElement(element)) {
    if (element.options.length > 0 && element.selectedIndex === 0 &&
        element.options[0].disabled &&
        !element.options[0].hasAttribute('selected')) {
      for (let i = 0; i < element.options.length; i++) {
        if (!element.options[i].disabled ||
            element.options[i].hasAttribute('selected')) {
          value = element.options[i].value;
          break;
        }
      }
    }
  }
  return (value || '').replace(/[\n\t]/gm, '');
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
 *     See the document on variable __gCrWeb.fill.EXTRACT_MASK_NONE,
 *     __gCrWeb.fill.EXTRACT_MASK_VALUE,
 *     __gCrWeb.fill.EXTRACT_MASK_OPTION_TEXT and
 *     __gCrWeb.fill.EXTRACT_MASK_OPTIONS.
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
      field['autocomplete_attribute'].length > __gCrWeb.fill.MAX_DATA_LENGTH) {
    // Discard overly long attribute values to avoid DOS-ing the browser
    // process. However, send over a default string to indicate that the
    // attribute was present.
    field['autocomplete_attribute'] = 'x-max-data-length-exceeded';
  }

  const roleAttribute = element.getAttribute('role');
  if (roleAttribute && roleAttribute.toLowerCase() === 'presentation') {
    field['role'] = __gCrWeb.fill.ROLE_ATTRIBUTE_PRESENTATION;
  }

  field['aria_label'] = __gCrWeb.fill.getAriaLabel(element);
  field['aria_description'] = __gCrWeb.fill.getAriaDescription(element);

  if (!__gCrWeb.fill.isAutofillableElement(element)) {
    return;
  }

  if (__gCrWeb.fill.isAutofillableInputElement(element) ||
      __gCrWeb.fill.isTextAreaElement(element) ||
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
  } else if (__gCrWeb.fill.isTextAreaElement(element)) {
    // Nothing more to do in this case.
  } else if (extractMask & __gCrWeb.fill.EXTRACT_MASK_OPTIONS) {
    __gCrWeb.fill.getOptionStringsFromElement(element, field);
  }

  if (!(extractMask & __gCrWeb.fill.EXTRACT_MASK_VALUE)) {
    return;
  }

  let value = __gCrWeb.fill.value(element);

  if (__gCrWeb.fill.isSelectElement(element) &&
      (extractMask & __gCrWeb.fill.EXTRACT_MASK_OPTION_TEXT)) {
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
  if (value.length > __gCrWeb.fill.MAX_DATA_LENGTH) {
    value = value.substr(0, __gCrWeb.fill.MAX_DATA_LENGTH);
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
      __gCrWeb.fill.EXTRACT_MASK_VALUE | __gCrWeb.fill.EXTRACT_MASK_OPTIONS;
  __gCrWeb['fill'].webFormElementToFormData(
      window, form, null, extractMask, formData, null);
  return __gCrWeb.stringify([formData]);
};

/**
 * Returns the coalesced child text of the elements who's ids are found in
 * the |attribute| of |element|.
 *
 * For example, given this document...
 *
 *      <div id="billing">Billing</div>
 *      <div>
 *        <div id="name">Name</div>
 *        <input id="field1" type="text" aria-labelledby="billing name"/>
 *     </div>
 *     <div>
 *       <div id="address">Address</div>
 *       <input id="field2" type="text" aria-labelledby="billing address"/>
 *     </div>
 *
 * The coalesced text by the id_list found in the aria-labelledby attribute
 * of the field1 input element would be "Billing Name" and for field2 it would
 * be "Billing Address".
 */
function coalesceTextByIdList(element, attribute) {
  if (!element) {
    return '';
  }

  const ids = element.getAttribute(attribute);
  if (!ids) {
    return '';
  }

  return ids.trim()
      .split(/\s+/)
      .map(function(i) {
        return document.getElementById(i);
      })
      .filter(function(e) {
        return e !== null;
      })
      .map(function(n) {
        return __gCrWeb.fill.findChildText(n);
      })
      .filter(function(s) {
        return s.length > 0;
      })
      .join(' ')
      .trim();
}

/**
 * Returns the coalesced text referenced by the aria-labelledby attribute
 * or the value of the aria-label attribute, with priority given to the
 * aria-labelledby text.
 */
__gCrWeb.fill.getAriaLabel = function(element) {
  let label = coalesceTextByIdList(element, 'aria-labelledby');
  if (!label) {
    label = element.getAttribute('aria-label') || '';
  }
  return label.trim();
};

/**
 * Returns the coalesced text referenced by the aria-describedby attribute.
 */
__gCrWeb.fill.getAriaDescription = function(element) {
  return coalesceTextByIdList(element, 'aria-describedby');
};

/**
 * Searches an element's ancestors to see if the element is inside a <form> or
 * <fieldset>.
 *
 * It is based on the logic in
 *     bool (const WebElement& element)
 * in chromium/src/components/autofill/content/renderer/form_cache.cc
 *
 * @param {!FormControlElement} element An element to examine.
 * @return {boolean} Whether the element is inside a <form> or <fieldset>.
 */
__gCrWeb.fill.isElementInsideFormOrFieldSet = function(element) {
  let parentNode = element.parentNode;
  while (parentNode) {
    if ((parentNode.nodeType === Node.ELEMENT_NODE) &&
        (__gCrWeb.fill.hasTagName(parentNode, 'form') ||
         __gCrWeb.fill.hasTagName(parentNode, 'fieldset'))) {
      return true;
    }
    parentNode = parentNode.parentNode;
  }
  return false;
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

/**
 * @param {int} nextAvailableID Next available integer.
 */
__gCrWeb.fill['setUpForUniqueIDs'] = function(nextAvailableID) {
  document[__gCrWeb.fill.ID_SYMBOL] = nextAvailableID;
};

/**
 * @param {Element} element Form or form input element.
 */
__gCrWeb.fill.setUniqueIDIfNeeded = function(element) {
  try {
    const uniqueID = __gCrWeb.fill.ID_SYMBOL;
    // Do not assign element id value if the base value for the document
    // is not set.
    if (typeof document[uniqueID] !== 'undefined' &&
        typeof element[uniqueID] === 'undefined') {
      element[uniqueID] = document[uniqueID]++;
      // TODO(crbug.com/1350973): WeakRef starts in 14.5, remove checks once 14
      // is deprecated.
      elementMap.set(
          element[uniqueID], window.WeakRef ? new WeakRef(element) : element);
    }
  } catch (e) {
  }
};

/**
 * @param {Element} element Form or form input element.
 * @return {String} Unique stable ID converted to string..
 */
__gCrWeb.fill.getUniqueID = function(element) {
  try {
    const uniqueID = __gCrWeb.fill.ID_SYMBOL;
    if (typeof element[uniqueID] !== 'undefined' && !isNaN(element[uniqueID])) {
      return element[uniqueID].toString();
    } else {
      return __gCrWeb.fill.RENDERER_ID_NOT_SET;
    }
  } catch (e) {
    return __gCrWeb.fill.RENDERER_ID_NOT_SET;
  }
};

/**
 * @param {int} Unique ID.
 * @return {Element} element Form or form input element.
 */
__gCrWeb.fill.getElementByUniqueID = function(id) {
  try {
    // TODO(crbug.com/1350973): WeakRef starts in 14.5, remove checks once 14 is
    // deprecated.
    return window.WeakRef ? elementMap.get(id).deref() : elementMap.get(id);
  } catch (e) {
    return null;
  }
};
