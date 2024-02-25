// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$, getRequiredElement} from 'chrome://resources/js/util.js';

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
 * The callback to button#context-clusters-dump to save the most recent context
 * clusters to a file.
 */
function onContextClustersDumpRequest() {
  getProxy().getHandler().getContextClustersJson().then(
      onContextClustersJsonReady);
}

/**
 * The callback when the context clusters JSON string has been prepared.
 */
function onContextClustersJsonReady(resp: {contextClustersJson: string}) {
  const data = resp.contextClustersJson;
  const filename = 'history_context_clusters_dump.json';

  dumpFileWithJsonContents(data, filename);
}

function getProxy(): HistoryClustersInternalsBrowserProxy {
  return HistoryClustersInternalsBrowserProxy.getInstance();
}

/**
 * The callback to button#print-keyword-bag-state that prints the keyword bag.
 */
function onPrintKeywordBagState() {
  getProxy().getHandler().printKeywordBagStateToLogMessages();
}

function initialize() {
  const logMessageContainer = $<HTMLTableElement>('log-message-container');

  getRequiredElement('log-messages-dump')
      .addEventListener('click', onLogMessagesDump);
  getRequiredElement('context-clusters-dump')
      .addEventListener('click', onContextClustersDumpRequest);
  getRequiredElement('print-keyword-bag-state')
      .addEventListener('click', onPrintKeywordBagState);

  getProxy().getCallbackRouter().onLogMessageAdded.addListener(
      (message: string) => {
        logMessages.push(message);
        if (logMessageContainer) {
          const logmessage = logMessageContainer.insertRow();
          const cell = logmessage.insertCell();
          cell.innerHTML = window.trustedTypes!.emptyHTML;
          const pre = document.createElement('pre');
          pre.textContent = message;
          cell.appendChild(pre);
        }
      });
}

document.addEventListener('DOMContentLoaded', initialize);
