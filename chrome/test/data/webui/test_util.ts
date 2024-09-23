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


/**
 * Helper method to locally add a breakpoint to a test and automatically pause
 * execution once it is hit, allowing further debugging via the DevTools, like
 * stepping through the code.
 *
 * Note: DO NOT commit such calls into the repository.
 *
 * Specifically in order to use it, follow the steps below:
 *
 *  1) Add an `await launchDebugger();` statement right before the line of
 *     interest.
 *  2) [Optional] Change test() to test.only() to only run the relevant test
 *     case.
 *  3) Launch the test with all of the following command line flags
 *     --enable-pixel-output-in-tests
 *     --ui-test-action-timeout=1000000
 *     --auto-open-devtools-for-tabs
 *     --gtest_filter=MyTest.Foo
 *
 *     Once the test launches, you should see a DevTools window, with the code
 *     paused at the 'debugger;' statement below.
 *  4) Click the "Step out" icon in the DevTools Sources panel to step out of
 *     this helper function and into the actual test code of interest. Continue
 *     debugging as needed.
 *
 *  Note that the timeout is necessary because 'debugger;' statements are a
 *  no-op if DevTools is not open, and opening DevTools with the
 *  --auto-open-devtools-for-tabs flag takes a bit of time. The default value
 *  seems to work for most tests, but can be overridden with a larger value if
 *  more time is needed in your environment.
 */
export async function launchDebugger(timeout: number = 2000) {
  await new Promise<void>(res => {
    window.setTimeout(() => res(), timeout);
  });
   /* eslint-disable-next-line no-debugger */
  debugger;
}

/**
 * When dealing with CrLitElement instances, prefer this over using the
 * `updateComplete` Promise, since it guarantees that microtasks queued by all
 * other Lit elements have executed, as well as any updates that would be
 * triggered by those updates, and so on.
 */
export function microtasksFinished(): Promise<void> {
  return new Promise(resolve => setTimeout(() => resolve(), 0));
}
