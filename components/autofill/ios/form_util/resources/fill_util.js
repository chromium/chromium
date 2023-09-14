// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MAX_STRING_LENGTH} from '//components/autofill/ios/form_util/resources/fill_constants.js';

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
        option['value'].substring(0, MAX_STRING_LENGTH));
    field['option_contents'].push(
        option['text'].substring(0, MAX_STRING_LENGTH));
  }
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
