// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';

import {HistoryClustersInternalsBrowserProxy} from './history_clusters_internals_browser_proxy.js';

// Contains all the log events received when the internals page is open.
const logMessages: string[] = [];


/**
 * The callback to button#log-messages-dump to save the logs to a file.
 */
function onLogMessagesDump() {
  const data = logMessages.join('\n');
  const blob = new Blob([data], {'type': 'text/json'});
  const url = URL.createObjectURL(blob);
  const filename = 'history_clusters_internals_logs_dump.json';

  const a = document.createElement('a');
  a.setAttribute('href', url);
  a.setAttribute('download', filename);

  const event = document.createEvent('MouseEvent');
  event.initMouseEvent(
      'click', true, true, window, 0, 0, 0, 0, 0, false, false, false, false, 0,
      null);
  a.dispatchEvent(event);
}

function getProxy(): HistoryClustersInternalsBrowserProxy {
  return HistoryClustersInternalsBrowserProxy.getInstance();
}


function initialize() {
  const logMessageContainer = $('log-message-container') as HTMLTableElement;

  $('log-messages-dump').addEventListener('click', onLogMessagesDump);

  getProxy().getCallbackRouter().onLogMessageAdded.addListener(
      (message: string) => {
        logMessages.push(message);
        if (logMessageContainer) {
          const logmessage = logMessageContainer.insertRow();
          logmessage.insertCell().innerHTML = `<pre>${message}</pre>`;
        }
      });
}

document.addEventListener('DOMContentLoaded', initialize);
