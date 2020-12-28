// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from './browser_proxy/browser_proxy.js';
import {assert} from './chrome_util.js';
import * as dom from './dom.js';
import {BarcodeContentType, sendBarcodeDetectedEvent} from './metrics.js';
import * as snackbar from './snackbar.js';
import * as state from './state.js';
import {OneShotTimer} from './timer.js';

// TODO(b/172879638): Tune the duration according to the final motion spec.
const CHIP_DURATION = 8000;

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
 * The countdown timer for dismissing the chip.
 * @type {?OneShotTimer}
 */
let currentTimer = null;

/**
 * Resets the variables of the current state and dismisses the chip.
 */
function deactivate() {
  if (currentChip !== null) {
    currentChip.classList.add('invisible');
  }
  currentCode = null;
  currentChip = null;
  currentTimer = null;
}

/**
 * Activates the chip on container and starts the timer.
 * @param {!HTMLElement} container The container of the chip.
 */
function activate(container) {
  container.classList.remove('invisible');
  currentChip = container;

  currentTimer = new OneShotTimer(deactivate, CHIP_DURATION);
  if (state.get(state.State.TAB_NAVIGATION)) {
    // Do not auto dismiss the chip when using keyboard for a11y. Screen reader
    // might need long time to read the detected content.
    currentTimer.stop();
  }
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
 * @return {!HTMLElement} The copy button element.
 */
function setupCopyButton(container, content, snackbarLabel) {
  const copyButton =
      dom.getFrom(container, '.barcode-copy-button', HTMLButtonElement);
  copyButton.onclick = async () => {
    await navigator.clipboard.writeText(content);
    snackbar.show(snackbarLabel);
  };
  return copyButton;
}

/**
 * Shows an actionable url chip.
 * @param {string} url
 */
function showUrl(url) {
  const container = dom.get('#barcode-chip-url-container', HTMLDivElement);
  activate(container);

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
}

/**
 * Shows an actionable text chip.
 * @param {string} text
 */
function showText(text) {
  const container = dom.get('#barcode-chip-text-container', HTMLDivElement);
  activate(container);
  container.classList.remove('expanded');

  const textEl = dom.get('#barcode-chip-text-content', HTMLDivElement);
  textEl.textContent = text;
  const expandable = textEl.scrollWidth > textEl.clientWidth;

  const expandEl = dom.get('#barcode-chip-text-expand', HTMLButtonElement);
  expandEl.classList.toggle('hidden', !expandable);
  expandEl.onclick = () => {
    container.classList.toggle('expanded');
    const expanded = container.classList.contains('expanded');
    expandEl.setAttribute('aria-expanded', expanded.toString());
  };

  const copyButton = setupCopyButton(container, text, 'snackbar_text_copied');

  // TODO(b/172879638): There is a race in ChromeVox which will speak the
  // focused element twice.
  if (expandable) {
    expandEl.focus();
  } else {
    copyButton.focus();
  }
}

/**
 * Shows an actionable chip for the string detected from a barcode.
 * @param {string} code
 */
export async function show(code) {
  if (code === currentCode) {
    if (currentTimer !== null) {
      // Extend the duration by resetting the timeout.
      currentTimer.resetTimeout();
    }
    return;
  }

  if (currentTimer !== null) {
    // Dismiss the previous chip.
    currentTimer.fireNow();
    assert(currentTimer === null, 'The timer should be cleared.');
  }

  currentCode = code;

  if (isSafeUrl(code)) {
    sendBarcodeDetectedEvent({contentType: BarcodeContentType.URL});
    showUrl(code);
  } else {
    sendBarcodeDetectedEvent({contentType: BarcodeContentType.TEXT});
    showText(code);
  }
}

/**
 * Dismisses the current barcode chip if it's being shown.
 */
export function dismiss() {
  if (currentTimer === null) {
    return;
  }
  currentTimer.fireNow();
}
