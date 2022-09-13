// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';

import {HistoryClustersInternalsBrowserProxy} from './history_clusters_internals_browser_proxy.js';

// Contains all the log events received when the internals page is open.
const logMessages: string[] = [];

/**
 * Dumps file with JSON contents to filename.
 */
function dumpFileWithJsonContents(contents: string, filename: string) {
  const blob = new Blob([contents], {'type': 'application/json'});
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.setAttribute('href', url);
  a.setAttribute('download', filename);
  a.click();
}

/**
 * The callback to button#log-messages-dump to save the logs to a file.
 */
function onLogMessagesDump() {
  const data = logMessages.join('\n');
  dumpFileWithJsonContents(data, 'history_clusters_internals_logs_dump.json');
}

/**
 * The callback to button#visits-dump to save the visits to a file.
 */
function onVisitsDumpRequested() {
  getProxy().getHandler().getVisitsJson().then(onVisitsJsonReady);
}

/**
 * The callback when the visits JSON string has been prepared.
 */
function onVisitsJsonReady(resp: {visitsJson: string}) {
  const data = resp.visitsJson;
  const filename = 'history_clusters_visits_dump.json';

  dumpFileWithJsonContents(data, filename);
}

function getProxy(): HistoryClustersInternalsBrowserProxy {
  return HistoryClustersInternalsBrowserProxy.getInstance();
}


function initialize() {
  const logMessageContainer = $('log-message-container') as HTMLTableElement;

  $('log-messages-dump').addEventListener('click', onLogMessagesDump);
  $('visits-dump').addEventListener('click', onVisitsDumpRequested);

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
