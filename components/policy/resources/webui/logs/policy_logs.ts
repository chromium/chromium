// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>


import '../strings.m.js';

import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {Log, VersionInfo} from './types.js';

let logs: Log[];
let versionInfo: VersionInfo;

// Dumps file with JSON contents to filename.
function dumpFileWithJsonContents() {
  const dumpObject = {versionInfo, logs};

  const data = JSON.stringify(dumpObject, null, 3);
  const filename = 'policy_logs_dump.json';

  const blob = new Blob([data], {'type': 'application/json'});
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.setAttribute('href', url);
  a.setAttribute('download', filename);
  a.click();
}

function displayList() {
  const logMessageContainer = getRequiredElement('logs-container');

  // TrustedTypes is not supported on iOS
  if (window.trustedTypes) {
    logMessageContainer.innerHTML = window.trustedTypes!.emptyHTML;
  } else {
    logMessageContainer.innerHTML = '';
  }
  logs.forEach(log => {
    const logMessage = document.createElement('li');
    logMessage.textContent = `[${log.logSeverity}] ${log.message}`;
    logMessageContainer.appendChild(logMessage);
  });
}

function displayVersionInfo() {
  versionInfo = JSON.parse(loadTimeData.getString('versionInfo'));

  getRequiredElement('chrome-version-value').textContent = versionInfo.version;

  getRequiredElement('chrome-revision-value').textContent =
      versionInfo.revision;

  getRequiredElement('os-version-value').textContent = versionInfo.deviceOs;

  const activeVariationsDiv = getRequiredElement('active-variations-container');
  versionInfo.variations.forEach((variation) => {
    const activeVariationItem = document.createElement('li');
    activeVariationItem.textContent = variation;
    activeVariationsDiv.appendChild(activeVariationItem);
  });
}

async function fetchLogs() {
  logs = await sendWithPromise('getPolicyLogs');
}

function initialize() {
  displayVersionInfo();

  const fetchLogsAndDisplay = () => fetchLogs().then(displayList);
  fetchLogsAndDisplay();

  getRequiredElement('logs-dump')
      .addEventListener('click', dumpFileWithJsonContents);
  getRequiredElement('logs-refresh')
      .addEventListener('click', fetchLogsAndDisplay);
}

document.addEventListener('DOMContentLoaded', initialize);
