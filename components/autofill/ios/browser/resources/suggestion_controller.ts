// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * @fileoverview Installs suggestion management functions on the
 * __gCrWeb object.
 */

/**
 * Returns the element with the specified name that is a child of the
 * specified parent element. This function searches through the whole subtree
 * and, in the event that multiple elements match, the first element in NLR
 * order is returned.
 * @param parent The parent of the desired element.
 * @param name The name of the desired element.
 * @return The element if found, otherwise null;
 */
function getElementByNameWithParent(parent: Element, name: string): Element|
    null {
  if ('name' in parent && parent.name === name) {
    return parent;
  }

  for (const child of parent.children) {
    const element = getElementByNameWithParent(child, name);
    if (element) {
      return element;
    }
  }
  return null;
}

/**
 * Returns the first element in `elements` that is later than `elementToCompare`
 * in tab order.
 *
 * @param elementToCompare The element to start searching forward in tab order
 *     from.
 * @param elementList Elements in which the first element that is later than
 *     `elementToCompare` in tab order is to be returned if there is one;
 *     `elements` should be sorted in DOM NLR tree order and should contain
 *     `elementToCompare`.
 * @return the first element in `elements` that is later than `elementToCompare`
 *     in tab order if there is one; null otherwise.
 */
function getNextElementInTabOrder(
    elementToCompare: Element, elementList: NodeListOf<Element>): Element|null {
  // There is no defined behavior if the element is not reachable. Here the
  // next reachable element in DOM tree order is returned. (This is what is
  // observed in Mobile Safari and Chrome Desktop, if `elementToCompare` is not
  // the last element in DOM tree order).
  // TODO(chenyu): investigate and simulate Mobile Safari's behavior when
  // `elementToCompare` is the last one in DOM tree order.
  const elements = Array.from(elementList);
  if (!isSequentiallyReachable(elementToCompare)) {
    const indexToCompare = elements.indexOf(elementToCompare);
    if (indexToCompare === elements.length - 1 || indexToCompare === -1) {
      return null;
    }
    for (let index = indexToCompare + 1; index < elements.length; ++index) {
      const element = elements[index]!;
      if (element instanceof HTMLElement && isSequentiallyReachable(element)) {
        return element;
      }
    }
    return null;
  }
  // Returns true iff `element1` that has DOM tree position `index1` is after
  // `element2` that has DOM tree position `index2` in tab order. It is assumed
  // `index1 !== index2`.
  const comparator = function(
      element1: Element, index1: number, element2: Element, index2: number) {
    const tabOrder1 = getTabOrder(element1);
    const tabOrder2 = getTabOrder(element2);
    return tabOrder1 > tabOrder2 ||
        (tabOrder1 === tabOrder2 && index1 > index2);
  };
  return getFormElementAfter(elementToCompare, elements, comparator);
}

/**
 * Returns the last element in `elements` that is earlier than
 * `elementToCompare` in tab order.
 *
 * @param elementToCompare The element to start searching backward in tab order
 *     from.
 * @param elementList Elements in which the last element that is earlier than
 *     `elementToCompare` in tab order is to be returned if there is one;
 *     `elements` should be sorted in DOM tree order and it should contain
 *     `elementToCompare`.
 * @return the last element in `elements` that is earlier than
 *     `elementToCompare` in tab order if there is one; null otherwise.
 */
function getPreviousElementInTabOrder(
    elementToCompare: Element, elementList: NodeListOf<Element>): Element|null {
  const elements = Array.from(elementList);
  // There is no defined behavior if the element is not reachable. Here the
  // previous reachable element in DOM tree order is returned.
  if (!isSequentiallyReachable(elementToCompare)) {
    const indexToCompare = elements.indexOf(elementToCompare);
    if (indexToCompare <= 0) {  // Ignore if first or no element is found.
      return null;
    }
    for (let index = indexToCompare - 1; index >= 0; --index) {
      const element = elements[index];
      if (element && element instanceof HTMLElement &&
          isSequentiallyReachable(element)) {
        return element;
      }
    }
    return null;
  }
  // Returns true iff `element1` that has DOM tree position `index1` is before
  // `element2` that has DOM tree position `index2` in tab order. It is assumed
  // `index1 !== index2`.
  const comparator = function(
      element1: Element, index1: number, element2: Element,
      index2: number): boolean {
    const tabOrder1 = getTabOrder(element1);
    const tabOrder2 = getTabOrder(element2);
    return tabOrder1 < tabOrder2 ||
        (tabOrder1 === tabOrder2 && index1 < index2);
  };
  return getFormElementAfter(elementToCompare, elements, comparator);
}

/**
 * Given an element `elementToCompare`, such as
 * `__gCrWeb.suggestion.isSequentiallyReachable(elementToCompare)`, and a list
 * of `elements` which are sorted in DOM tree order and contains
 * `elementToCompare`, this method returns the next element in `elements` after
 * `elementToCompare` in the order defined by `comparator`, where an element is
 * said to be 'after' anotherElement if and only if
 * comparator(element, indexOfElement, anotherElement, anotherIndex) is true.
 *
 * @param elementToCompare The element to be compared.
 * @param elementList Elements to compare; `elements` should be
 *     sorted in DOM tree order and it should contain `elementToCompare`.
 * @param comparator A function that returns a boolean, given an Element
 *     `element1`, an integer that represents `element1`'s position in DOM tree
 *     order, an Element `element2` and an integer that represents `element2`'s
 *     position in DOM tree order.
 * @return The element that satisfies the conditions given above.
 */
function getFormElementAfter(
    elementToCompare: Element, elementList: Element[],
    comparator: (
        element1: Element, index1: number, element2: Element, index2: number) =>
        boolean): Element|null {
  // Computes the index `indexToCompare` of `elementToCompare` in `element`.
  const indexToCompare = elementList.indexOf(elementToCompare);
  if (indexToCompare === -1) {
    return null;
  }
  let result: Element|null = null;
  let resultIndex = -1;
  for (let index = 0; index < elementList.length; ++index) {
    if (index === indexToCompare) {
      continue;
    }
    const element = elementList[index];
    if (!element || !isSequentiallyReachable(element)) {
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
}

/**
 * Tests an element's visibility. This test is expensive so should be used
 * sparingly.
 *
 * @param element A DOM element.
 * @return true if the `element` is currently part of the visible
 * DOM.
 */
function isElementVisible(element: Element): boolean {
  let node: Node|null = element as Node;
  while (node && node !== document) {
    if (node.nodeType === Node.ELEMENT_NODE) {
      const style = window.getComputedStyle(node as Element);
      if (style.display === 'none' || style.visibility === 'hidden') {
        return false;
      }
    }
    // Move up the tree and test again.
    node = node.parentNode;
  }
  // Test reached the top of the DOM without finding a concealed
  // ancestor.
  return true;
}

/**
 * Returns if an element is reachable in sequential navigation.
 *
 * @param element The element that is to be examined.
 * @return Whether an element is reachable in sequential navigation.
 */
function isSequentiallyReachable(element: Element): boolean {
  // It is proposed in W3C that if tabIndex is omitted or parsing the
  // value returns an error, the user agent should follow platform
  // conventions to determine whether the element can be reached using
  // sequential focus navigation, and if so, what its relative order
  // should be. No document is found on the platform conventions in this
  // case on iOS.
  //
  // There is a list of elements for which the tabIndex focus flags are
  // suggested to be set in this case in W3C proposal. It is observed
  // that in WKWebView parsing the tabIndex of an element in this list
  // returns 0 if it is omitted or it is set to be an invalid value,
  // undefined, null or NaN. So here it is assumed that all the elements
  // that have invalid tabIndex are not reachable in sequential focus
  // navigation.
  //
  // It is proposed in W3C that if tabIndex is a negative integer, the
  // user agent should not allow the element to be reached using
  // sequential focus navigation.
  const tabIndex: number|null =
      'tabIndex' in element ? element.tabIndex as number : null;
  if ((!tabIndex && tabIndex !== 0) || tabIndex < 0) {
    return false;
  }
  const elementType: string|null =
      'type' in element ? element.type as string : null;
  if (elementType === 'hidden' || element.hasAttribute('disabled')) {
    return false;
  }
  // false is returned if `element` is neither an input nor a select.
  // Note based on this condition, false is returned for an iframe (as
  // Mobile Safari does not navigate to elements in an iframe, there is
  // no need to recursively check if there is a reachable element in an
  // iframe).
  if (element.tagName !== 'INPUT' && element.tagName !== 'SELECT' &&
      element.tagName !== 'TEXTAREA') {
    return false;
  }
  // The following elements are skipped when navigating using 'Prev' and
  // "Next' buttons in Mobile Safari.
  if (element.tagName === 'INPUT' &&
      (elementType === 'submit' || elementType === 'reset' ||
       elementType === 'image' || elementType === 'button' ||
       elementType === 'range' || elementType === 'radio' ||
       elementType === 'checkbox')) {
    return false;
  }
  // Expensive, final check that the element is not concealed.
  return isElementVisible(element);
}

/**
 * It is proposed in W3C an element that has a tabIndex greater than zero
 * should be placed before any focusable element whose tabIndex is equal to
 * zero in sequential focus navigation order. Here a value adjusted from
 * tabIndex that reflects this order is returned. That is, given `element1`
 * and `element2`, if `__gCrWeb.suggestion.getTabOrder(element1) >
 * __gCrWeb.suggestion.getTabOrder(element2)`, then `element1` is after
 * `element2` in sequential navigation.
 *
 * @param element The element of which the sequential navigation order
 *     information is returned.
 * @return An adjusted value that reflect `element`'s position in the sequential
 *     navigation.
 */
function getTabOrder(element: Element): number {
  const tabIndex: number|null =
      'tabIndex' in element ? element.tabIndex as number : null;
  if (tabIndex === 0) {
    return Number.MAX_VALUE;
  }
  return tabIndex as number;
}

/**
 * Returns the element named `fieldName` in the form specified by
 * `formName`, if it exists.
 *
 * @param formName The name of the form containing the element.
 * @param fieldName The name of the field containing the element.
 * @return The element if found, otherwise null.
 */
function getFormElement(formName: string, fieldName: string): Element|null {
  const form = gCrWeb.form.getFormElementFromIdentifier(formName);
  if (!form) {
    return null;
  }
  return getElementByNameWithParent(form, fieldName);
}

/**
 * Focuses the next element in the sequential focus navigation. No operation
 * if there is no such element.
 */
function selectNextElement(formName: string, fieldName: string) {
  const currentElement =
      formName ? getFormElement(formName, fieldName) : document.activeElement;
  if (!currentElement) {
    return;
  }
  const nextElement =
      getNextElementInTabOrder(currentElement, document.querySelectorAll('*'));
  if (nextElement && nextElement instanceof HTMLElement) {
    nextElement.focus();
  }
}

/**
 * Focuses the previous element in the sequential focus navigation. No
 * operation if there is no such element.
 */
function selectPreviousElement(formName: string, fieldName: string) {
  const currentElement =
      formName ? getFormElement(formName, fieldName) : document.activeElement;
  if (!currentElement) {
    return;
  }
  const prevElement = getPreviousElementInTabOrder(
      currentElement, document.querySelectorAll('*'));
  if (prevElement && prevElement instanceof HTMLElement) {
    prevElement.focus();
  }
}

/**
 * @param formName The name of the form containing the element.
 * @param fieldName The name of the field containing the element.
 * @return Whether there is an element in the sequential navigation after the
 *     currently active element.
 */
function hasNextElement(formName: string, fieldName: string): boolean {
  const currentElement =
      formName ? getFormElement(formName, fieldName) : document.activeElement;
  if (!currentElement) {
    return false;
  }
  return getNextElementInTabOrder(
             currentElement, document.querySelectorAll('*')) !== null;
}

/**
 * @param formName The name of the form containing the element.
 * @param fieldName The name of the field containing the element.
 * @return Whether there is an element in the sequential navigation before the
 *     currently active element.
 */
function hasPreviousElement(formName: string, fieldName: string): boolean {
  const currentElement =
      formName ? getFormElement(formName, fieldName) : document.activeElement;
  if (!currentElement) {
    return false;
  }
  return getPreviousElementInTabOrder(
             currentElement, document.querySelectorAll('*')) !== null;
}

declare interface PreviousNextElements {
  previous: boolean;
  next: boolean;
}

/**
 * @param formName The name of the form containing the element.
 * @param fieldName The name of the field containing the element.
 * @return Whether there is an element in the sequential navigation before and
 *     after currently active element. The result is returned as an object with
 *     a boolean value for the keys `previous` and `next`.
 */
function hasPreviousNextElements(
    formName: string, fieldName: string): PreviousNextElements {
  return {
    previous: hasPreviousElement(formName, fieldName),
    next: hasNextElement(formName, fieldName),
  };
}

gCrWeb.suggestion = {
  getNextElementInTabOrder,
  getPreviousElementInTabOrder,
  selectNextElement,
  selectPreviousElement,
  hasNextElement,
  hasPreviousElement,
  hasPreviousNextElements,
};
