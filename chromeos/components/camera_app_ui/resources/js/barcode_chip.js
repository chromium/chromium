// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as dom from './dom.js';
import * as toast from './toast.js';
import * as util from './util.js';

/**
 * Checks whether a string is a regular url link with http or https protocol.
 * @param {string} s
 * @return {boolean}
 */
function isSafeUrl(s) {
  try {
    const url = new URL(s);
    if (url.protocol !== 'http:' && url.protocol !== 'https:') {
      console.warn('Reject url with protocol:', url.protocol);
      return false;
    }
    return true;
  } catch (e) {
    return false;
  }
}

/**
 * Shows an actionable url chip.
 * @param {string} url
 */
function showUrl(url) {
  const el = dom.get('.barcode-chip-container', HTMLDivElement);
  const anchor = dom.getFrom(el, 'a', HTMLAnchorElement);
  Object.assign(anchor, {
    href: url,
    textContent: url,
  });
  // TODO(b/172879638): Handle a11y.
  util.animateOnce(el);
  // TODO(b/172879638): Show copy button.
}

/**
 * Shows an actionable text chip.
 * @param {string} text
 */
function showText(text) {
  // TODO(b/172879638): Show text properly.
  toast.showDebugMessage(text);
}

/**
 * Shows an actionable chip for the string detected from a barcode.
 * @param {string} s
 */
export function show(s) {
  if (isSafeUrl(s)) {
    showUrl(s);
  } else {
    showText(s);
  }
}
