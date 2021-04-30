// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helpers for testing against DOM. For integration tests, this is
 * injected into an isolated world, so can't access objects in other scripts.
 */

/**
 * Runs a query selector until it finds an element (repeated on each mutation).
 * If the element does not exist this will timeout.
 *
 * opt_path defines the path of ancestor Elements to the queried Element, whose
 * shadow boundaries need to be crossed to find the queried Element. These must
 * be defined in order from closest parent of the queried Element, to the
 * ancestor that is in the document.body subtree.
 * If opt_path is not defined correctly this will timeout.
 *
 * @param {string} query
 * @param {!Array<string>=} opt_path
 * @return {!Promise<!Element>}
 */
async function waitForNode(query, opt_path) {
  /** @type {!HTMLElement|!ShadowRoot} */
  let node = document.body;
  const parent = opt_path ? opt_path.shift() : undefined;
  if (parent) {
    const element = await waitForNode(parent, opt_path);
    if (!(element instanceof HTMLElement) || !element.shadowRoot) {
      throw new Error('Path not a shadow root HTMLElement');
    }
    node = element.shadowRoot;
  }
  const existingElement = node.querySelector(query);
  if (existingElement) {
    return Promise.resolve(existingElement);
  }
  console.log('Waiting for ' + query);
  return new Promise(resolve => {
    const observer = new MutationObserver((mutationList, observer) => {
      const element = node.querySelector(query);
      if (element) {
        resolve(element);
        observer.disconnect();
      }
    });
    observer.observe(node, {attributes: true, childList: true, subtree: true});
  });
}
