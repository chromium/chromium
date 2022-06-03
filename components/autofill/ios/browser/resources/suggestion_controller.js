// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Installs suggestion management functions on the
 * __gCrWeb object.
 *
 * TODO(crbug.com/647084): Enable checkTypes error for this file.
 * @suppress {checkTypes}
 */
goog.provide('__crWeb.suggestion');

/* Beginning of anonymous object. */
(function() {

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb.suggestion = {};

// Store suggestion namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['suggestion'] = __gCrWeb.suggestion;

/**
 * Returns the element with the specified name that is a child of the
 * specified parent element.
 * @param {Element} parent The parent of the desired element.
 * @param {string} name The name of the desired element.
 * @return {Element} The element if found, otherwise null;
 */
const getElementByNameWithParent = function(parent, name) {
  if (parent.name === name) return parent;

  let el;
  for (let i = 0; i < parent.children.length; i++) {
    el = getElementByNameWithParent(parent.children[i], name);
    if (el) return el;
  }
  return null;
};

/**
 * Returns the first element in |elements| that is later than |elementToCompare|
 * in tab order.
 *
 * @param {Element} elementToCompare The element to start searching forward in
 *     tab order from.
 * @param {NodeList} elementList Elements in which the first element that is
 *     later than |elementToCompare| in tab order is to be returned if there is
 *     one; |elements| should be sorted in DOM tree order and should contain
 *     |elementToCompare|.
 * @return {Element} the first element in |elements| that is later than
 *     |elementToCompare| in tab order if there is one; null otherwise.
 */
__gCrWeb.suggestion.getNextElementInTabOrder = function(
    elementToCompare, elementList) {
  const elements = [];
  for (let i = 0; i < elementList.length; ++i) {
    elements[i] = elementList[i];
  }
  // There is no defined behavior if the element is not reachable. Here the
  // next reachable element in DOM tree order is returned. (This is what is
  // observed in Mobile Safari and Chrome Desktop, if |elementToCompare| is not
  // the last element in DOM tree order).
  // TODO(chenyu): investigate and simulate Mobile Safari's behavior when
  // |elementToCompare| is the last one in DOM tree order.
  if (!__gCrWeb.suggestion.isSequentiallyReachable(elementToCompare)) {
    const indexToCompare = elements.indexOf(elementToCompare);
    if (indexToCompare === elements.length - 1 || indexToCompare === -1) {
      return null;
    }
    for (let index = indexToCompare + 1; index < elements.length; ++index) {
      const element = elements[index];
      if (__gCrWeb.suggestion.isSequentiallyReachable(element)) {
        return element;
      }
    }
    return null;
  }

  // Returns true iff |element1| that has DOM tree position |index1| is after
  // |element2| that has DOM tree position |index2| in tab order. It is assumed
  // |index1 !== index2|.
  const comparator = function(element1, index1, element2, index2) {
    const tabOrder1 = __gCrWeb.suggestion.getTabOrder(element1);
    const tabOrder2 = __gCrWeb.suggestion.getTabOrder(element2);
    return tabOrder1 > tabOrder2 ||
        (tabOrder1 === tabOrder2 && index1 > index2);
  };
  return __gCrWeb.suggestion.getFormElementAfter(
      elementToCompare, elements, comparator);
};

/**
 * Returns the last element in |elements| that is earlier than
 * |elementToCompare| in tab order.
 *
 * @param {Element} elementToCompare The element to start searching backward in
 *     tab order from.
 * @param {NodeList} elementList Elements in which the last element that is
 *     earlier than |elementToCompare| in tab order is to be returned if
 *     there is one; |elements| should be sorted in DOM tree order and it should
 *     contain |elementToCompare|.
 * @return {Element} the last element in |elements| that is earlier than
 *     |elementToCompare| in tab order if there is one; null otherwise.
 */
__gCrWeb.suggestion.getPreviousElementInTabOrder = function(
    elementToCompare, elementList) {
  const elements = [];
  for (let i = 0; i < elementList.length; ++i) {
    elements[i] = elementList[i];
  }

  // There is no defined behavior if the element is not reachable. Here the
  // previous reachable element in DOM tree order is returned.
  if (!__gCrWeb.suggestion.isSequentiallyReachable(elementToCompare)) {
    const indexToCompare = elements.indexOf(elementToCompare);
    if (indexToCompare <= 0) {  // Ignore if first or no element is found.
      return null;
    }
    for (let index = indexToCompare - 1; index >= 0; --index) {
      const element = elements[index];
      if (__gCrWeb.suggestion.isSequentiallyReachable(element)) {
        return element;
      }
    }
    return null;
  }

  // Returns true iff |element1| that has DOM tree position |index1| is before
  // |element2| that has DOM tree position |index2| in tab order. It is assumed
  // |index1 !== index2|.
  const comparator = function(element1, index1, element2, index2) {
    const tabOrder1 = __gCrWeb.suggestion.getTabOrder(element1);
    const tabOrder2 = __gCrWeb.suggestion.getTabOrder(element2);
    return tabOrder1 < tabOrder2 ||
        (tabOrder1 === tabOrder2 && index1 < index2);
  };

  return __gCrWeb.suggestion.getFormElementAfter(
      elementToCompare, elements, comparator);
};

/**
 * Given an element |elementToCompare|, such as
 * |__gCrWeb.suggestion.isSequentiallyReachable(elementToCompare)|, and a list
 * of |elements| which are sorted in DOM tree order and contains
 * |elementToCompare|, this method returns the next element in |elements| after
 * |elementToCompare| in the order defined by |comparator|, where an element is
 * said to be 'after' anotherElement if and only if
 * comparator(element, indexOfElement, anotherElement, anotherIndex) is true.
 *
 * @param {Element} elementToCompare The element to be compared.
 * @param {Array<Element>} elements Elements to compare; |elements| should be
 *     sorted in DOM tree order and it should contain |elementToCompare|.
 * @param {function(Element, number, Element, number):boolean} comparator A
 *     function that returns a boolean, given an Element |element1|, an integer
 *     that represents |element1|'s position in DOM tree order, an Element
 *     |element2| and an integer that represents |element2|'s position in DOM
 *     tree order.
 * @return {Element} The element that satisfies the conditions given above.
 */
__gCrWeb.suggestion.getFormElementAfter = function(
    elementToCompare, elements, comparator) {
  // Computes the index |indexToCompare| of |elementToCompare| in |element|.
  const indexToCompare = elements.indexOf(elementToCompare);
  if (indexToCompare === -1) {
    return null;
  }

  let result = null;
  let resultIndex = -1;
  for (let index = 0; index < elements.length; ++index) {
    if (index === indexToCompare) {
      continue;
    }
    const element = elements[index];
    if (!__gCrWeb.suggestion.isSequentiallyReachable(element)) {
      continue;
    }

    if (comparator(element, index, elementToCompare, indexToCompare)) {
      if (!result) {
        result = element;
        resultIndex = index;
      } else {
        if (comparator(result, resultIndex, element, index)) {
          result = element;
          resultIndex = index;
        }
      }
    }
  }
  return result;
};

/**
 * Returns if an element is reachable in sequential navigation.
 *
 * @param {Element} element The element that is to be examined.
 * @return {boolean} Whether an element is reachable in sequential navigation.
 */
__gCrWeb.suggestion.isSequentiallyReachable = function(element) {
  const tabIndex = element.tabIndex;
  // It is proposed in W3C that if tabIndex is omitted or parsing the value
  // returns an error, the user agent should follow platform conventions to
  // determine whether the element can be reached using sequential focus
  // navigation, and if so, what its relative order should be. No document is
  // found on the platform conventions in this case on iOS.
  //
  // There is a list of elements for which the tabIndex focus flags are
  // suggested to be set in this case in W3C proposal. It is observed that in
  // UIWebview parsing the tabIndex of an element in this list returns 0 if it
  // is omitted or it is set to be an invalid value, undefined, null or NaN. So
  // here it is assumed that all the elements that have invalid tabIndex is
  // not reachable in sequential focus navigation.
  //
  // It is proposed in W3C that if tabIndex is a negative integer, the user
  // agent should not allow the element to be reached using sequential focus
  // navigation.
  if ((!tabIndex && tabIndex !== 0) || tabIndex < 0) {
    return false;
  }
  if (element.type === 'hidden' || element.hasAttribute('disabled')) {
    return false;
  }

  // false is returned if |element| is neither an input nor a select. Note based
  // on this condition, false is returned for an iframe (as Mobile Safari does
  // not navigate to elements in an iframe, there is no need to recursively
  // check if there is a reachable element in an iframe).
  if (element.tagName !== 'INPUT' && element.tagName !== 'SELECT' &&
      element.tagName !== 'TEXTAREA') {
    return false;
  }

  // The following elements are skipped when navigating using 'Prev' and "Next'
  // buttons in Mobile Safari.
  if (element.tagName === 'INPUT' &&
      (element.type === 'submit' || element.type === 'reset' ||
       element.type === 'image' || element.type === 'button' ||
       element.type === 'range' || element.type === 'radio' ||
       element.type === 'checkbox')) {
    return false;
  }

  // Expensive, final check that the element is not concealed.
  return __gCrWeb['common'].isElementVisible(element);
};

/**
 * It is proposed in W3C an element that has a tabIndex greater than zero should
 * be placed before any focusable element whose tabIndex is equal to zero in
 * sequential focus navigation order. Here a value adjusted from tabIndex that
 * reflects this order is returned. That is, given |element1| and |element2|,
 * if |__gCrWeb.suggestion.getTabOrder(element1) >
 * __gCrWeb.suggestion.getTabOrder(element2)|, then |element1| is after
 * |element2| in sequential navigation.
 *
 * @param {Element} element The element of which the sequential navigation order
 *     information is returned.
 * @return {number} An adjusted value that reflect |element|'s position in the
 *     sequential navigation.
 */
__gCrWeb.suggestion.getTabOrder = function(element) {
  const tabIndex = element.tabIndex;
  if (tabIndex === 0) {
    return Number.MAX_VALUE;
  }
  return tabIndex;
};

/**
 * Returns the element named |fieldName| in the form specified by |formName|,
 * if it exists.
 *
 * @param {string} formName The name of the form containing the element.
 * @param {string} fieldName The name of the field containing the element.
 * @return {Element} The element if found, otherwise null.
 */
__gCrWeb.suggestion.getFormElement = function(formName, fieldName) {
  const form = __gCrWeb.form.getFormElementFromIdentifier(formName);
  if (!form) return null;
  return getElementByNameWithParent(form, fieldName);
};

/**
 * Focuses the next element in the sequential focus navigation. No operation
 * if there is no such element.
 */
__gCrWeb.suggestion['selectNextElement'] = function(formName, fieldName) {
  const currentElement = formName ?
      __gCrWeb.suggestion.getFormElement(formName, fieldName) :
      document.activeElement;
  const nextElement = __gCrWeb.suggestion.getNextElementInTabOrder(
      currentElement, document.all);
  if (nextElement) {
    nextElement.focus();
  }
};

/**
 * Focuses the previous element in the sequential focus navigation. No
 * operation if there is no such element.
 */
__gCrWeb.suggestion['selectPreviousElement'] = function(formName, fieldName) {
  const currentElement = formName ?
      __gCrWeb.suggestion.getFormElement(formName, fieldName) :
      document.activeElement;
  const prevElement = __gCrWeb.suggestion.getPreviousElementInTabOrder(
      currentElement, document.all);
  if (prevElement) {
    prevElement.focus();
  }
};

/**
 * @param {string} formName The name of the form containing the element.
 * @param {string} fieldName The name of the field containing the element.
 * @return {boolean} Whether there is an element in the sequential navigation
 *     after the currently active element.
 */
__gCrWeb.suggestion['hasNextElement'] = function(formName, fieldName) {
  const currentElement = formName ?
      __gCrWeb.suggestion.getFormElement(formName, fieldName) :
      document.activeElement;
  return __gCrWeb.suggestion.getNextElementInTabOrder(
             currentElement, document.all) !== null;
};

/**
 * @param {string} formName The name of the form containing the element.
 * @param {string} fieldName The name of the field containing the element.
 * @return {boolean} Whether there is an element in the sequential navigation
 *     before the currently active element.
 */
__gCrWeb.suggestion['hasPreviousElement'] = function(formName, fieldName) {
  const currentElement = formName ?
      __gCrWeb.suggestion.getFormElement(formName, fieldName) :
      document.activeElement;
  return __gCrWeb.suggestion.getPreviousElementInTabOrder(
             currentElement, document.all) !== null;
};

/**
 * @param {string} formName The name of the form containing the element.
 * @param {string} fieldName The name of the field containing the element.
 * @return {string} Whether there is an element in the sequential navigation
 *     before and after currently active element. The result is returned as a
 *     comma separated string of the strings |true| and |false|.
 *     TODO(crbug.com/893368): Return a dictionary with the values instead.
 */
__gCrWeb.suggestion['hasPreviousNextElements'] = function(formName, fieldName) {
  return [
    __gCrWeb.suggestion.hasPreviousElement(formName, fieldName),
    __gCrWeb.suggestion.hasNextElement(formName, fieldName)
  ].toString();
};

/**
 * Blurs the |activeElement| of the current document.
 */
__gCrWeb.suggestion['blurActiveElement'] = function() {
  document.activeElement.blur();
};

}());  // End of anonymous object
