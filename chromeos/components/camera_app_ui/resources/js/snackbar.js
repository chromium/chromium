// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from './browser_proxy/browser_proxy.js';
import * as dom from './dom.js';
import * as util from './util.js';

/**
 * Shows a snackbar message.
 * @param {string} label The label of the message to show.
 * @param {...string} substitutions The substitutions for the label.
 */
export function show(label, ...substitutions) {
  const message = browserProxy.getI18nMessage(label, ...substitutions);
  const el = dom.get('.snackbar', HTMLElement);
  el.textContent = '';  // Force reiterate the same message for a11y.
  el.textContent = message;
  util.animateOnce(el);
}
