// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {$, getRequiredElement} from 'chrome://resources/js/util_ts.js';
import {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {DownloadedModelInfo, PageHandlerFactory} from './optimization_guide_internals.mojom-webui.js';
import {OptimizationGuideInternalsBrowserProxy} from './optimization_guide_internals_browser_proxy.js';

// Contains all the log events received when the internals page is open.
const logMessages: Array<{
  eventTime: string,
  logSource: string,
  sourceLocation: string,
  message: string,
}> = [];

/**
 * Converts a mojo time to a JS time.
 * @param {!mojoBase.mojom.Time} mojoTime
 * @return {!Date}
 */
function convertMojoTimeToJS(mojoTime: Time) {
  // The JS Date() is based off of the number of milliseconds since the
  // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
  // base::Time (represented in mojom.Time) represents the number of
  // microseconds since the Windows FILETIME epoch (1601-01-01 00:00:00 UTC).
  // This computes the final JS time by computing the epoch delta and the
  // conversion from microseconds to milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to base::Time::kTimeTToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
}

/**
 * Converts the source location to chromium source URL.
 * @param sourceFile
 * @param sourceLine
 * @returns string
 */
function getChromiumSourceLink(sourceFile: string, sourceLine: number) {
  // Valid source file starts with ../../
  if (!sourceFile.startsWith('../../')) {
    return `${sourceFile}(${sourceLine})`;
  }
  const fileName = sourceFile.slice(sourceFile.lastIndexOf('/') + 1);
  if (fileName.length == 0) {
    return `${sourceFile}(${sourceLine})`;
  }
  return `<a href="https://source.chromium.org/chromium/chromium/src/+/main:${
      sourceFile.slice(6)};l=${sourceLine}">${fileName}(${sourceLine})</a>`;
}

/**
 * Maps the logSource to a human readable string representation.
 * Must be kept in sync with the |LogSource| enum in
 * optimization_guide_internals/webui/optimization_guide_internals.mojom.
 * @param logSource
 * @returns string
 */
function getLogSource(logSource: number) {
  if (logSource == 1) {
    return 'SERVICE_AND_SETTINGS';
  }
  if (logSource == 2) {
    return 'HINTS';
  }
  if (logSource == 3) {
    return 'MODEL_MANAGEMENT';
  }
  if (logSource == 4) {
    return 'PAGE_CONTENT_ANNOTATIONS';
  }
  if (logSource == 5) {
    return 'HINTS_NOTIFICATIONS';
  }
  if (logSource == 6) {
    return 'TEXT_CLASSIFIER';
  }
  return logSource.toString();
}

/**
 * The callback to button#log-messages-dump to save the logs to a file.
 */
function onLogMessagesDump() {
  const data = JSON.stringify(logMessages);
  const blob = new Blob([data], {'type': 'text/json'});
  const url = URL.createObjectURL(blob);
  const filename = 'optimization_guide_internals_logs_dump.json';

  const a = document.createElement('a');
  a.setAttribute('href', url);
  a.setAttribute('download', filename);

  const event = document.createEvent('MouseEvent');
  event.initMouseEvent(
      'click', true, true, window, 0, 0, 0, 0, 0, false, false, false, false, 0,
      null);
  a.dispatchEvent(event);
}

async function onModelsPageOpen() {
  const downloadedModelsContainer =
      getRequiredElement<HTMLTableElement>('downloaded-models-container');
  try {
    const response: {downloadedModelsInfo: DownloadedModelInfo[]} =
        await PageHandlerFactory.getRemote().requestDownloadedModelsInfo();
    const downloadedModelsInfo = response.downloadedModelsInfo;
    for (const {optimizationTarget, version, filePath} of
             downloadedModelsInfo) {
      const versionStr = version.toString();
      const existingModel = $<HTMLTableRowElement>(optimizationTarget);
      if (existingModel) {
        existingModel.querySelector('.downloaded-models-version')!.innerHTML =
            versionStr;
        existingModel.querySelector('.downloaded-models-file-path')!.innerHTML =
            filePath;
      } else {
        const downloadedModel = downloadedModelsContainer.insertRow();
        downloadedModel.id = optimizationTarget;
        appendTD(
            downloadedModel, optimizationTarget,
            'downloaded-models-optimization-target');
        appendTD(downloadedModel, versionStr, 'downloaded-models-version');
        appendTD(downloadedModel, filePath, 'downloaded-models-file-path');
      }
    }
  } catch (err) {
    throw new Error(
        `Error resolving promise from requestDownloadedModelsInfo, ${err}`);
  }
}

/**
 * Appends a new TD element to the specified |parent| element.
 *
 * @param {HTMLTableRowElement} parent The element to which a new TD element is
 *     appended.
 * @param {string} innerHTML The inner HTML of the element.
 * @param {string} className The class name of the element.
 */
function appendTD(
    parent: HTMLTableRowElement, innerHTML: string, className: string) {
  const td = parent.insertCell();
  td.innerHTML = innerHTML;
  td.className = className;
  parent.appendChild(td);
}

function getProxy(): OptimizationGuideInternalsBrowserProxy {
  return OptimizationGuideInternalsBrowserProxy.getInstance();
}


function initialize() {
  const tabbox = document.querySelector('cr-tab-box');
  assert(tabbox);
  tabbox.hidden = false;

  const logMessageContainer =
      getRequiredElement<HTMLTableElement>('log-message-container');

  getRequiredElement('log-messages-dump')
      .addEventListener('click', onLogMessagesDump);

  getProxy().getCallbackRouter().onLogMessageAdded.addListener(
      (eventTime: Time, logSource: number, sourceFile: string,
       sourceLine: number, message: string) => {
        const eventTimeStr = convertMojoTimeToJS(eventTime).toISOString();
        const sourceLocation = getChromiumSourceLink(sourceFile, sourceLine);
        const logSourceStr = getLogSource(logSource);
        logMessages.push({
          eventTime: eventTimeStr,
          logSource: logSourceStr,
          sourceLocation,
          message,
        });
        const logMessage = logMessageContainer.insertRow();
        appendTD(logMessage, eventTimeStr, 'event-logs-time');
        appendTD(logMessage, logSourceStr, 'event-logs-log-source');
        appendTD(logMessage, sourceLocation, 'event-logs-source-location');
        appendTD(logMessage, message, 'event-logs-message');
      });

  const tabpanelNodeList = document.querySelectorAll('div[slot=\'panel\']');
  const tabpanels = Array.prototype.slice.call(tabpanelNodeList, 0);
  const tabpanelIds = tabpanels.map(function(tab) {
    return tab.id;
  });

  tabbox.addEventListener('selected-index-change', e => {
    const tabpanel = tabpanels[(e as CustomEvent).detail];
    const hash = tabpanel.id.match(/(?:^tabpanel-)(.+)/)[1];
    window.location.hash = hash;
  });

  const activateTabByHash = function() {
    let hash = window.location.hash;

    // Remove the first character '#'.
    hash = hash.substring(1);

    const id = 'tabpanel-' + hash;
    const index = tabpanelIds.indexOf(id);
    if (index === -1) {
      return;
    }
    tabbox.setAttribute('selected-index', `${index}`);

    if (hash === 'models') {
      onModelsPageOpen();
    }
  };

  window.onhashchange = activateTabByHash;
  activateTabByHash();
}

document.addEventListener('DOMContentLoaded', initialize);
