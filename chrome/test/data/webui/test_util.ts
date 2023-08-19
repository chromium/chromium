// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Do not depend on the Chai Assertion Library in this file. Some consumers of
// the following test utils are not configured to use Chai.

/**
 * Observes an HTML attribute and fires a promise when it matches a given
 * value.
 */
export function whenAttributeIs(
    target: HTMLElement, attributeName: string,
    attributeValue: any): Promise<void> {
  function isDone(): boolean {
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
 */
export function whenCheck(
    target: HTMLElement, check: () => boolean): Promise<void> {
  return check() ?
      Promise.resolve() :
      new Promise(resolve => new MutationObserver((_list, observer) => {
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
 * @return A promise firing once the event occurs.
 */
export function eventToPromise(
    eventType: string, target: Element|EventTarget|Window): Promise<any> {
  return new Promise(function(resolve, _reject) {
    target.addEventListener(eventType, function f(e) {
      target.removeEventListener(eventType, f);
      resolve(e);
    });
  });
}

/**
 * Returns whether or not the element specified is visible.
 */
export function isVisible(element: Element|null): boolean {
  const rect = element ? element.getBoundingClientRect() : null;
  return (!!rect && rect.width * rect.height > 0);
}

/**
 * Searches the DOM of the parentEl element for a child matching the provided
 * selector then checks the visibility of the child.
 */
export function isChildVisible(parentEl: Element, selector: string,
                               checkLightDom?: boolean): boolean {
  const element = checkLightDom ? parentEl.querySelector(selector) :
                                  parentEl.shadowRoot!.querySelector(selector);
  return isVisible(element);
}

/**
 * Queries |selector| on |element|'s shadow root and returns the resulting
 * element if there is any.
 */
export function $$<E extends HTMLElement = HTMLElement>(
    element: HTMLElement, selector: string): E|null;
export function $$(element: HTMLElement, selector: string) {
  return element.shadowRoot!.querySelector(selector);
}

/**
 * Returns whether the |element|'s style |property| matches the expected value.
 */
export function hasStyle(
    element: Element, property: string, expected: string): boolean {
  return expected === element.computedStyleMap().get(property)?.toString();
}
