// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * dynamic_import.js is used to avoid syntax failure of closure compiler since
 * it currently does not support the dynamic imports feature.
 * @suppress {moduleLoad}
 */

import {dynamicImport} from './dynamic_import.js';
import * as Comlink from './lib/comlink.js';
import {WaitableEvent} from './waitable_event.js';

/**
 * @type {!WaitableEvent}
 */
const domReady = new WaitableEvent();

const exposedObjects = {loadScript};

/**
 * Loads given script into the untrusted context.
 * @param {string} scriptUrl
 * @return {!Promise}
 */
async function loadScript(scriptUrl) {
  await domReady.wait();
  const module = await dynamicImport(scriptUrl);
  Object.assign(exposedObjects, module);
}

document.addEventListener('DOMContentLoaded', () => {
  domReady.signal();
});

Comlink.expose(exposedObjects, Comlink.windowEndpoint(self.parent, self));
