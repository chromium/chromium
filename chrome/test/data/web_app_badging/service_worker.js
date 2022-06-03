// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

// Adds a  message event handler that responds to 'set-app-badge' and
// 'clear-app-badge' commands by running setAppBadge() or
// clearAppBadge() on this ServiceWorkerGlobalScope.  Responds with
// a message to the sender after the set/clearAppBadge() promise settles.
//
// Here's how to send a valid message to this service worker:
//
// (1) serviceWorker.postMessage({ command: 'set-app-badge', value: 29 });
// (2) serviceWorker.postMessage({ command: 'set-app-badge' });
// (3) serviceWorker.postMessage({ command: 'clear-app-badge' });
addEventListener('message', async function (event) {
  try {
    const command = event.data.command;
    switch (command) {
      case 'set-app-badge':
        const badgeValue = event.data.value;
        if (badgeValue !== undefined) {
          await navigator.setAppBadge(badgeValue);
        } else {
          await navigator.setAppBadge();
        }
        event.source.postMessage('OK');
        break;

      case 'clear-app-badge':
        await navigator.clearAppBadge();
        event.source.postMessage('OK');
        break;

      default:
        throw `Unknown command: '${command}'`;
    }
  } catch (error) {
    event.source.postMessage(`EXCEPTION: ${error}`);
  }
});
