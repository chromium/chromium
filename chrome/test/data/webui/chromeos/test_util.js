// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';
// clang-format on

// Do not depend on the Chai Assertion Library in this file. Some consumers of
// the following test utils are not configured to use Chai.

/**
 * Observes an HTML attribute and fires a promise when it matches a given
 * value.
 * @param {!HTMLElement} target
 * @param {string} attributeName
 * @param {*} attributeValue
 * @return {!Promise}
 */
export function whenAttributeIs(target, attributeName, attributeValue) {
  function isDone() {
    return target.getAttribute(attributeName) === attributeValue;
  }

  return isDone() ? Promise.resolve() : new Promise(function(resolve) {
    new MutationObserver(function(mutations, observer) {
      for (const mutation of mutations) {
        if (mutation.type === 'attributes' &&
            mutation.attributeName === attributeName && isDone()) {
          observer.disconnect();
          resolve();
          return;
        }
      }
    })
        .observe(
            target, {attributes: true, childList: false, characterData: false});
  });
}

/**
 * Observes an HTML element and fires a promise when the check function is
 * satisfied.
 * @param {!HTMLElement} target
 * @param {Function} check
 * @return {!Promise}
 */
export function whenCheck(target, check) {
  return check() ?
      Promise.resolve() :
      new Promise(resolve => new MutationObserver((list, observer) => {
                               if (check()) {
                                 observer.disconnect();
                                 resolve();
                               }
                             }).observe(target, {
        attributes: true,
        childList: true,
        subtree: true,
      }));
}

/**
 * Converts an event occurrence to a promise.
 * @param {string} eventType
 * @param {!Element|!EventTarget|!Window} target
 * @return {!Promise} A promise firing once the event occurs.
 */
export function eventToPromise(eventType, target) {
  return new Promise(function(resolve, reject) {
    target.addEventListener(eventType, function f(e) {
      target.removeEventListener(eventType, f);
      resolve(e);
    });
  });
}

/**
 * Returns whether or not the element specified is visible.
 * @param {?Element} element
 * @return {boolean}
 */
export function isVisible(element) {
  const rect = element ? element.getBoundingClientRect() : null;
  return (!!rect && rect.width * rect.height > 0);
}

/**
 * Searches the DOM of the parentEl element for a child matching the provided
 * selector then checks the visibility of the child.
 * @param {!Element} parentEl
 * @param {string} selector
 * @param {boolean=} checkLightDom
 * @return {boolean}
 */
export function isChildVisible(parentEl, selector, checkLightDom) {
  const element = checkLightDom ? parentEl.querySelector(selector) :
                                  parentEl.shadowRoot.querySelector(selector);
  return isVisible(element);
}
