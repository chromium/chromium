// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//components/autofill/ios/form_util/resources/create_fill_namespace.js';

import * as fillConstants from '//components/autofill/ios/form_util/resources/fill_constants.js';
import {findChildText} from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {trim} from '//ios/web/public/js_messaging/resources/utils.js';

declare interface AutofillFormFieldData {
  name: string;
  value: string;
  renderer_id: string;
  form_control_type: string;
  autocomplete_attribute: string;
  max_length: number;
  is_autofilled: boolean;
  is_user_edited: boolean;
  is_checkable: boolean;
  is_focusable: boolean;
  should_autocomplete: boolean;
  role: number;
  placeholder_attribute: string;
  aria_label: string;
  aria_description: string;
  option_texts: string[];
  option_values: string[];
  label?: string;
  identifier?: string;
  name_attribute?: string;
  id_attribute?: string;
}

declare interface AutofillFormData {
  name: string;
  renderer_id: string;
  origin: string;
  action: string;
  fields: AutofillFormFieldData[];
  host_frame: string;
  child_frames?: FrameTokenWithPredecessor[];
  name_attribute?: string;
  id_attribute?: string;
}

declare interface FrameTokenWithPredecessor {
  token: string;
  predecessor: number;
}

/**
 * Acquires the specified DOM `attribute` from the DOM `element` and returns
 * its lower-case value, or null if not present.
 *
 * @param element A DOM element.
 * @param attribute An attribute name.
 * @return Lowercase value of DOM element or null if not present.
 */
function getLowerCaseAttribute(
    element: Element | null, attribute: string): string | null {
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
 * @param element An element to check if it can be autocompleted.
 * @return true if element can be autocompleted.
 */
function autoComplete(element: fillConstants.FormControlElement|null): boolean {
  if (!element) {
    return false;
  }
  if (getLowerCaseAttribute(element, 'autocomplete') === 'off') {
    return false;
  }
  if (getLowerCaseAttribute(element.form, 'autocomplete') == 'off') {
    return false;
  }
  return true;
}

/**
 * Returns true if an element should suggest autocomplete dropdown.
 *
 * @param element An element to check if it can be autocompleted.
 * @return true if autocomplete dropdown should be suggested.
 */
gCrWeb.fill.shouldAutocomplete = function(
    element: fillConstants.FormControlElement|null): boolean {
  if (!autoComplete(element)) {
    return false;
  }
  if (getLowerCaseAttribute(element!, 'autocomplete') === 'one-time-code') {
    return false;
  }
  if (getLowerCaseAttribute(
    element!.form, 'autocomplete') === 'one-time-code') {
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
 * @param value The value the input element will be set.
 * @param input The input element of which the value is set.
 */
function setInputElementAngularValue(
    value: string, input: Element | null): void {
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
    function(parse: Function) {
      const setter = parse(angularModel);
      setter.assign(angularScope, value);
    },
  ]);
}

/**
 * Sets the value of an input, dispatches the events on the changed element and
 * call `callback` if it is defined.
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
 * @param value The value the input element will be set.
 * @param input The input element of which the value is set.
 * @param callback Callback function called after the input
 *     element's value is changed.
 * @return Whether the value has been set successfully.
 */
gCrWeb.fill.setInputElementValue = function(
    value: string, input: HTMLInputElement|null,
    callback: Function|undefined = undefined): boolean {
  if (!input) {
    return false;
  }

  const activeElement = document.activeElement;
  if (input !== activeElement) {
    createAndDispatchHTMLEvent(activeElement, 'blur', true, false);
    createAndDispatchHTMLEvent(input, 'focus', true, false);
  }

  const filled = setInputElementValue(value, input);
  if (callback) {
    callback();
  }

  if (input !== activeElement) {
    createAndDispatchHTMLEvent(input, 'blur', true, false);
    createAndDispatchHTMLEvent(activeElement, 'focus', true, false);
  }
  return filled;
};

declare interface PropertyDescriptor {
    get(): string;
    set?(): void;
    configurable: boolean;
}

/**
 * Internal function to set the element value.
 *
 * @param value The value the input element will be set.
 * @param input The input element of which the value is set.
 * @return Whether the value has been set successfully.
 */
function setInputElementValue(value: string, input: HTMLInputElement): boolean {
  const propertyName = (input.type === 'checkbox' || input.type === 'radio') ?
      'checked' :
      'value';
  if (input.type !== 'select-one' && input.type !== 'checkbox' &&
      input.type !== 'radio') {
    // In HTMLInputElement.cpp there is a check on canSetValue(value), which
    // returns false only for file input. As file input is not relevant for
    // autofill and this method is only used for autofill for now, there is no
    // such check in this implementation.
    value = sanitizeValueForInputElement(value, input);
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

  const oldPropertyDescriptor =
      Object.getOwnPropertyDescriptor(input, propertyName);
  const overrideProperty =
      oldPropertyDescriptor && oldPropertyDescriptor.configurable;
  let setterCalled = false;

  if (overrideProperty) {
    const newProperty: PropertyDescriptor = {
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
      newProperty.set = function() {
        setterCalled = true;
        oldPropertyDescriptor.set!.call(input, value);
      };
    }
    Object.defineProperty(input, propertyName, newProperty);
  } else {
    setterCalled = true;
    (input[propertyName] as boolean|string) = value;
  }

  if (window['angular']) {
    // The page uses the AngularJS framework. Update the angular value before
    // sending events.
    setInputElementAngularValue(value, input);
  }
  notifyElementValueChanged(input);

  if (overrideProperty) {
    Object.defineProperty(input, propertyName, oldPropertyDescriptor);
    if (!setterCalled) {
      // The setter was never called. This may be intentional (the framework
      // ignored the input event) or not (the event did not conform to what
      // framework expected). The whole function will likely fail, but try to
      // set the value directly as a last try.
      (input[propertyName] as boolean|string) = value;
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
 * @param proposedValue The proposed value.
 * @param element The element for which the proposedValue is to be
 *     sanitized.
 * @return The sanitized value.
 */
function sanitizeValueForInputElement(
    proposedValue: string|null, element: Element): string {
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
  if (gCrWeb.common.isTextField(element)) {
    return sanitizeValueForTextFieldInputType(
        proposedValue, element as HTMLInputElement);
  }
  return proposedValue;
}

/**
 * Returns a sanitized value for a text field.
 *
 * The logic is based on `String sanitizeValue(const String&)`
 * in chromium/src/third_party/WebKit/Source/core/html/TextFieldInputType.h
 * Note this method is overridden in EmailInputType and NumberInputType.
 *
 * @param proposedValue The proposed value.
 * @param element The element for which the proposedValue is to be
 *     sanitized.
 * @return The sanitized value.
 */
function sanitizeValueForTextFieldInputType(
    proposedValue: string, element: HTMLInputElement): string {
  const textFieldElementType = element.type;
  if (textFieldElementType === 'email') {
    return sanitizeValueForEmailInputType(proposedValue, element);
  } else if (textFieldElementType === 'number') {
    return sanitizeValueForNumberInputType(proposedValue);
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
    const current = valueWithLineBreakRemoved[i]!;
    if (current < ' ' && current !== '\t') {
      newLength = i;
      break;
    }
  }
  return valueWithLineBreakRemoved.substring(0, newLength);
}

/**
 * Returns the sanitized value for an email input.
 *
 * The logic is based on
 *
 *     String EmailInputType::sanitizeValue(const String& proposedValue) const
 *
 * in chromium/src/third_party/WebKit/Source/core/html/EmailInputType.cpp
 *
 * @param proposedValue The proposed value.
 * @param element The element for which the proposedValue is to be
 *     sanitized.
 * @return The sanitized value.
 */
function sanitizeValueForEmailInputType(
    proposedValue: string, element: HTMLInputElement): string {
  const valueWithLineBreakRemoved = proposedValue.replace(/(\r\n|\n\r)/gm, '');

  if (!element.multiple) {
    return trim(proposedValue);
  }
  const addresses = valueWithLineBreakRemoved.split(',');
  for (let i = 0; i < addresses.length; ++i) {
    addresses[i] = trim(addresses[i]!);
  }
  return addresses.join(',');
}


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
 * @param proposedValue The proposed value.
 * @return The sanitized value.
 */
function sanitizeValueForNumberInputType(proposedValue: string): string {
  const sanitizedValue = Number(proposedValue);
  if (isNaN(sanitizedValue)) {
    return '';
  }
  return sanitizedValue.toString();
}

/**
 * Creates and sends notification that element has changed.
 *
 * Send events that 'mimic' the user typing in a field.
 * 'input' event is often use in case of a text field, and 'change'event is
 * more often used in case of selects.
 *
 * @param {Element} element The element that changed.
 */
function notifyElementValueChanged(element: Element): void {
  createAndDispatchHTMLEvent(element, 'keydown', true, false);
  createAndDispatchHTMLEvent(element, 'keypress', true, false);
  createAndDispatchHTMLEvent(element, 'input', true, false);
  createAndDispatchHTMLEvent(element, 'keyup', true, false);
  createAndDispatchHTMLEvent(element, 'change', true, false);
}

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
function createAndDispatchHTMLEvent(
    element: Element | null, type: string, bubbles: boolean,
    cancelable: boolean): void {
  const event =
      new Event(type, {bubbles: bubbles, cancelable: cancelable});
  element?.dispatchEvent(event);
}

/**
 * Converts a relative URL into an absolute URL.
 */
function absoluteURL(doc: Document, relativeURL: string): string {
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
 * Returns a canonical action for `formElement`. It works the same as upstream
 * function GetCanonicalActionForForm.
 * @return Canonical action.
 */
gCrWeb.fill.getCanonicalActionForForm = function(
    formElement: HTMLFormElement): string {
  const rawAction = formElement.getAttribute('action') || '';
  const absoluteUrl = absoluteURL(formElement.ownerDocument, rawAction);
  return gCrWeb.common.removeQueryAndReferenceFromURL(absoluteUrl);
};

declare interface OptionFieldStrings {
    option_values: string[] & {toJSON?: string|null};
    option_texts: string[]&{toJSON?: string | null};
}

/**
 * Fills `field` data with the values of the <option> elements present in
 * `selectElement`.
 *
 * It is based on the logic in
 *     void GetOptionStringsFromElement(const WebSelectElement& select_element,
 *                                      std::vector<string16>* option_values,
 *                                      std::vector<string16>* option_texts)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param selectElement A select element from which option data are
 *     extracted.
 * @param field A field that will contain the extracted option
 *     information.
 */
gCrWeb.fill.getOptionStringsFromElement = function(
    selectElement: HTMLSelectElement, field: OptionFieldStrings): void {
  field.option_values = [];
  // Protect against custom implementation of Array.toJSON in host pages.
  field.option_values.toJSON = null;
  field.option_texts = [];
  field.option_texts.toJSON = null;
  const options = selectElement.options;
  for (let i = 0; i < options.length; ++i) {
    const option = options[i]!;
    field.option_values.push(
        option.value.substring(0, fillConstants.MAX_STRING_LENGTH));
    field.option_texts.push(
        option.text.substring(0, fillConstants.MAX_STRING_LENGTH));
  }
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
 * @param element An element to examine.
 * @return The value for `element`.
 */
gCrWeb.fill.value = function(
    element: fillConstants.FormControlElement|HTMLOptionElement): string {
  let value = element.value;
  if (gCrWeb.fill.isSelectElement(element)) {
    const selectElement = element as HTMLSelectElement;
    if (selectElement.options.length > 0 && selectElement.selectedIndex === 0 &&
        selectElement.options[0]!.disabled &&
        !selectElement.options[0]!.hasAttribute('selected')) {
      for (const option of selectElement.options) {
        if (!option.disabled || option.hasAttribute('selected')) {
          value = option.value;
          break;
        }
      }
    }
  }
  return (value || '').replace(/[\n\t]/gm, '');
};

/**
 * Returns the coalesced child text of the elements who's ids are found in
 * the `attribute` of `element`.
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
function coalesceTextByIdList(
  element: Element|null, attribute: string): string {
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
        return findChildText(n!);
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
gCrWeb.fill.getAriaLabel = function(element: Element): string {
  let label = coalesceTextByIdList(element, 'aria-labelledby');
  if (!label) {
    label = element.getAttribute('aria-label') || '';
  }
  return label.trim();
};

/**
 * Returns the coalesced text referenced by the aria-describedby attribute.
 */
gCrWeb.fill.getAriaDescription = function(element: Element): string {
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
 * @param element An element to examine.
 * @return Whether the element is inside a <form> or <fieldset>.
 */
gCrWeb.fill.isElementInsideFormOrFieldSet = function(
    element: fillConstants.FormControlElement): boolean {
  let parentNode = element.parentNode;
  while (parentNode) {
    if ((parentNode.nodeType === Node.ELEMENT_NODE) &&
        (gCrWeb.fill.hasTagName(parentNode, 'form') ||
         gCrWeb.fill.hasTagName(parentNode, 'fieldset'))) {
      return true;
    }
    parentNode = parentNode.parentNode;
  }
  return false;
};

/**
 * @param element Form or form input element.
 * @return Unique stable ID converted to string..
 */
gCrWeb.fill.getUniqueID = function(element: any): string {
  // `setUniqueIDIfNeeded` is only available in the isolated content world.
  // Check before invoking it as this script is injected into the page content
  // world as well.
  if (gCrWeb.fill.setUniqueIDIfNeeded) {
    gCrWeb.fill.setUniqueIDIfNeeded(element);
  }

  try {
    const uniqueIDSymbol = gCrWeb.fill.ID_SYMBOL;
    if (typeof element[uniqueIDSymbol] !== 'undefined' &&
        !isNaN(element[uniqueIDSymbol]!)) {
      return element[uniqueIDSymbol].toString();
    } else {
      // Use the fallback value stored in the DOM. This will happen when the
      // script is running in the page content world. JavaScript properties are
      // not shared across content worlds. This means that `element[uniqueID]`
      // will not have value in the page content world because it was set in the
      // isolated content world.
      const valueInDOM =
          element.getAttribute(fillConstants.UNIQUE_ID_ATTRIBUTE);

      // Check that there is a valid integer ID stored in the DOM. If not,
      // return the fallback value.
      return isNaN(parseInt(valueInDOM)) ? fillConstants.RENDERER_ID_NOT_SET :
                                           valueInDOM;
    }
  } catch (e) {
    return fillConstants.RENDERER_ID_NOT_SET;
  }
};

export {AutofillFormFieldData, AutofillFormData, FrameTokenWithPredecessor};
