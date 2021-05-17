// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertInstanceof,
} from './chrome_util.js';

/**
 * CSS rules.
 * @type {!Array<!CSSStyleRule>}
 */
const cssRules = (() => {
  const ruleList = [];
  for (const sheet of /** @type{!Iterable} */ (document.styleSheets)) {
    ruleList.push(...sheet.cssRules);
  }
  return ruleList;
})();

/**
 * Gets the CSS style by the given selector.
 * @param {string} selector Selector text.
 * @return {!CSSStyleDeclaration}
 */
export function cssStyle(selector) {
  const rule = cssRules.find((rule) => rule.selectorText === selector);
  assert(rule !== undefined);
  return assertInstanceof(rule.style, CSSStyleDeclaration);
}
