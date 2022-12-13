// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

export function assertCloseTo(actual, expected) {
  assertTrue(
      Math.abs(1 - actual / expected) <= 0.001,
      `expected ${expected} to be close to ${actual}`);
}

/**
 * Queries for an element through a path of custom elements.
 * This is needed because querySelector() does not query into
 * custom elements' shadow roots.
 *
 * @param {!Element} root element to start searching from.
 * @param {!Array<string>} path array of query selectors. each selector should
 *     correspond to one shadow root.
 * @returns {HTMLElement|null} element or null if not found.
 */
export function deepQuerySelector(root, path) {
  assert(root, 'deepQuerySelector called with null root');

  let el = root;

  for (const part of path) {
    if (el.shadowRoot) {
      el = el.shadowRoot;
    }

    el = el.querySelector(part);
    if (!el) {
      break;
    }
  }

  return el;
}

/**
 * Constructs a promise which resolves when the given condition function
 * evaluates to a truthy value.
 * @param {!function(): T} condition condition function to check.
 * @param {string=} message error message to show when maxWait reached.
 * @param {number=} maxWait maximum wait time in ms.
 * @return {!Promise<T>} promise resolving to truthy return value of condition.
 * @template T return type of condition function.
 */
export async function waitForCondition(condition, message, maxWait = 5000) {
  const interval = 10;
  let waiting = 0;

  /** @type {T} */
  let result;
  while (!(result = condition()) && waiting < maxWait) {
    await timeout(interval);
    waiting += interval;
  }
  assert(
      result,
      message || 'waitForCondition timed out after ' + maxWait + ' ms.');

  return result;
}

/**
 * Constructs a promise which resolves after the given amount of time.
 * @param {!number} ms timeout in milliseconds.
 * @return {!Promise} timeout promise.
 */
export function timeout(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

/**
 * Constructs a promise which resolves after 0 seconds.
 * @return {!Promise} timeout promise.
 */
export function completePendingMicrotasks() {
  return new Promise((resolve) => {
    setTimeout(resolve, 0);
  });
}

/**
 * Constructs a promise which resolves when the given promise resolves,
 * or fails after the given timeout - whichever occurs first.
 * @param {!Promise<T>} promise promise to wait for.
 * @param {!number} ms max timeout in milliseconds
 * @param {message=} message message on timeout.
 * @return {!Promise<T>} promise with timeout.
 * @template T resolve type of promise.
 */
export function waitWithTimeout(promise, ms, message) {
  message = message || 'waiting for promise timed out after ' + ms + ' ms.';
  return Promise.race(
      [promise, timeout(ms).then(
        () => Promise.reject(new Error(message)))]);
}

/**
 * Constructs a promise which resolves when the given element receives
 * an event of the given type.
 * Note: this function should be called *before* event is expected to set up
 * the handler, then it should be awaited when the event is required.
 * @param {!Element} element element to listen on.
 * @param {!string} eventType event type to listen for.
 * @return {!Promise<!Event>} event promise.
 */
export function waitForEvent(element, eventType) {
  return new Promise(
      resolve => element.addEventListener(eventType, resolve, {once: true}));
}


/**
 * Simulates a mouse click event on the given element.
 * @param {!Element} element element to right click.
 * @param {!number} button button number for event.
 * @param {!string=} eventType event type to dispatch.
 */
export function dispatchMouseEvent(element, button, eventType = 'contextmenu') {
  element.dispatchEvent(new MouseEvent(eventType, {
    bubbles: true,
    cancelable: true,
    view: window,
    button: button,
    buttons: 0,
    clientX: element.getBoundingClientRect().x,
    clientY: element.getBoundingClientRect().y,
  }));
}

const ACTIVE_EMOJI_GROUP_CLASS = 'emoji-group-active';
const ACTIVE_TEXT_GROUP_CLASS = 'text-group-active';
/**
 * Checks if the given emoji-group-button or text-group-button element is
 * activated.
 * @param {?Element} element element to check.
 * @return {boolean} true if active, false otherwise.
 */
export function isGroupButtonActive(element) {
  assert(element, 'group button element should not be null');
  return element.classList.contains(ACTIVE_EMOJI_GROUP_CLASS) ||
      element.classList.contains(ACTIVE_TEXT_GROUP_CLASS);
}
