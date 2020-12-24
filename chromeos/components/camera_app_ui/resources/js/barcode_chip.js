// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from './browser_proxy/browser_proxy.js';
import * as dom from './dom.js';
import * as snackbar from './snackbar.js';
import * as util from './util.js';

/**
 * The detected string that is being shown currently.
 * @type {?string}
 */
let currentCode = null;

/**
 * The barcode chip container that is being shown currently.
 * @type {?HTMLElement}
 */
let currentChip = null;

/**
 * Resets the variables of the current state.
 */
function resetCurrentState() {
  if (currentChip !== null) {
    currentChip.classList.add('hidden');
  }
  currentCode = null;
  currentChip = null;
}

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
 * Setups the copy button.
 * @param {!HTMLElement} container The container for the button.
 * @param {string} content The content to be copied.
 * @param {string} snackbarLabel The label to be displayed on snackbar when the
 *     content is copied.
 */
function setupCopyButton(container, content, snackbarLabel) {
  const copyButton =
      dom.getFrom(container, '.barcode-copy-button', HTMLButtonElement);
  copyButton.onclick = async () => {
    await navigator.clipboard.writeText(content);
    snackbar.show(snackbarLabel);
  };
}

/**
 * Shows an actionable url chip.
 * @param {string} url
 */
function showUrl(url) {
  const container = dom.get('#barcode-chip-url-container', HTMLDivElement);
  container.classList.remove('hidden');

  const anchor = dom.getFrom(container, 'a', HTMLAnchorElement);
  Object.assign(anchor, {
    href: url,
    textContent: url,
  });
  const hostname = new URL(url).hostname;
  const label = browserProxy.getI18nMessage('barcode_link_detected', hostname);
  anchor.setAttribute('aria-label', label);
  anchor.setAttribute('aria-description', url);
  anchor.focus();

  setupCopyButton(container, url, 'snackbar_link_copied');

  currentChip = container;
  util.animateOnce(container, resetCurrentState);
}

/**
 * Shows an actionable text chip.
 * @param {string} text
 */
function showText(text) {
  const container = dom.get('#barcode-chip-text-container', HTMLDivElement);
  container.classList.remove('hidden', 'expanded');

  const textEl = dom.get('#barcode-chip-text-content', HTMLDivElement);
  textEl.textContent = text;
  const expandable = textEl.scrollWidth > textEl.clientWidth;

  const expandEl = dom.get('#barcode-chip-text-expand', HTMLButtonElement);
  expandEl.classList.toggle('hide', !expandable);
  expandEl.onclick = () => {
    container.classList.toggle('expanded');
  };

  setupCopyButton(container, text, 'snackbar_text_copied');

  // TODO(b/172879638): Handle a11y.
  currentChip = container;
  util.animateOnce(container, resetCurrentState);
}

/**
 * Shows an actionable chip for the string detected from a barcode.
 * @param {string} code
 */
export async function show(code) {
  if (code === currentCode) {
    return;
  }

  if (currentChip !== null) {
    await util.animateCancel(currentChip);
  }

  currentCode = code;

  if (isSafeUrl(code)) {
    showUrl(code);
  } else {
    showText(code);
  }
}
