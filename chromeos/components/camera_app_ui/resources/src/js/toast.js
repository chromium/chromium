// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from './browser_proxy/browser_proxy.js';
import * as dom from './dom.js';
import * as util from './util.js';

/**
 * Updates the toast message.
 * @param {string} message Message to be updated.
 * @param {boolean} spoken Whether the toast is spoken only.
 */
function update(message, spoken) {
  // TTS speaks changes of on-screen aria-live elements. Force content changes
  // and clear content once inactive to avoid stale content being read out.
  const element = dom.get('#toast', HTMLElement);
  util.animateCancel(element);  // Cancel the active toast if any.
  element.textContent = '';     // Force to reiterate repeated messages.
  element.textContent = message;

  element.classList.toggle('spoken', spoken);
  util.animateOnce(element, () => element.textContent = '');
}

/**
 * Shows a toast with given message which doesn't need i18n.
 * @param {string} message Message to be shown.
 */
export function showDebugMessage(message) {
  update(message, false);
}

/**
 * Shows a toast message.
 * @param {string} label The label of the message to show.
 * @param {...string} substitutions The substitutions needed for the given
 *     label.
 */
export function show(label, ...substitutions) {
  update(browserProxy.getI18nMessage(label, ...substitutions), false);
}

/**
 * Speaks a toast message.
 * @param {string} label The label of the message to show.
 * @param {...string} substitutions The substitutions needed for the given
 *     label.
 */
export function speak(label, ...substitutions) {
  update(browserProxy.getI18nMessage(label, ...substitutions), true);
}
