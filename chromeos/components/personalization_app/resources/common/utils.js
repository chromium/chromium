// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions to be shared between trusted and untrusted
 * code.
 */

/**
 * Checks if argument is an array with non-zero length.
 * @param {?Object} maybeArray
 * @return {boolean}
 */
export function isNonEmptyArray(maybeArray) {
  return Array.isArray(maybeArray) && maybeArray.length > 0;
}

/**
 * Attach a listener to a child element onload function. Returns a promise
 * that resolves when that child element is loaded.
 * @param {*} element A polymer element
 * @param {!string} id Id of the child element.
 * @param {function(*, function(...*): void, !Array=): void}
 *     afterNextRender callback for first render of element.
 * @return {!Promise<!HTMLElement>}
 */
export function promisifyOnload(element, id, afterNextRender) {
  const promise = new Promise((resolve) => {
    function readyCallback() {
      const child = element.shadowRoot.getElementById(id);
      child.onload = () => resolve(child);
    }
    afterNextRender(element, readyCallback);
  });
  return promise;
}

/**
 * Convert an UnguessableToken to string to use as a key.
 * @param {!mojoBase.mojom.UnguessableToken} param0
 * @return {string}
 */
export function unguessableTokenToString({high, low}) {
  return `${high},${low}`;
}

/**
 * @param {!string} str
 * @return {!mojoBase.mojom.UnguessableToken}
 */
export function stringToUnguessableToken(str) {
  const [high, low] = str.split(',').map(BigInt);
  return {high, low};
}

/**
 * @param {!mojoBase.mojom.UnguessableToken} a
 * @param {!mojoBase.mojom.UnguessableToken} b
 * @return {boolean}
 */
export function unguessableTokensEqual(a, b) {
  return a.hasOwnProperty('high') && a.high === b.high &&
      a.hasOwnProperty('low') && a.low === b.low;
}

/**
 * Returns true if this event is a user action to select an item.
 * @param {!Event} event
 * @return {boolean}
 */
export function isSelectionEvent(event) {
  return (event instanceof MouseEvent && event.type === 'click') ||
      (event instanceof KeyboardEvent && event.key === 'Enter');
}

/**
 * Sets a css variable to control the animation delay.
 * @param {number} index
 * @return {string}
 */
export function getLoadingPlaceholderAnimationDelay(index) {
  return `--animation-delay: ${index * 83}ms;`
}
