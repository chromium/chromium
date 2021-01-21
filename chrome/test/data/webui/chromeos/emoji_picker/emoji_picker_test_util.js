// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

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

  let el = root.shadowRoot || root;

  for (const part of path) {
    el = el.querySelector(part);
    if (!el) {
      break;
    }
    if (el.shadowRoot) {
      el = el.shadowRoot;
    }
  }

  return el;
}
