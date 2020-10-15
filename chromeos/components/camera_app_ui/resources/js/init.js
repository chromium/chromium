// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Comlink from './lib/comlink.js';
// eslint-disable-next-line no-unused-vars

document.addEventListener('DOMContentLoaded', async () => {
  const workerPath = '/js/test_bridge.js';
  // The cast here is a workaround to avoid warning in closure compiler since
  // it does not have such signature. We need to fix it in upstream.
  // TODO(crbug.com/980846): Remove the cast once the PR is merged:
  // https://github.com/google/closure-compiler/pull/3704
  const sharedWorker =
      new SharedWorker(workerPath, /** @type {string} */ ({type: 'module'}));
  const testBridge = Comlink.wrap(sharedWorker.port);
  const appWindow = await testBridge.bindWindow(window.location.href);
  // TODO(crbug.com/980846): Refactor to use a better way rather than window
  // properties to pass data to other modules.
  window['appWindow'] = appWindow;
  window['windowCreationTime'] = performance.now();
  if (appWindow !== null) {
    await appWindow.waitUntilReadyOnTastSide();
  }

  const mainScript = document.createElement('script');
  mainScript.setAttribute('type', 'module');
  mainScript.setAttribute('src', '/js/main.js');
  document.head.appendChild(mainScript);
});
