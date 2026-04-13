// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>


import '/strings.m.js';

import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import {BrowserProxy} from './../browser_proxy.js';
import type {Log} from './../policy.mojom-webui.js';
import type {VersionInfo} from './types.js';

const policyPageMojoMigrationEnabled =
    loadTimeData.getBoolean('policyPageMojoMigrationEnabled');

let logs: Log[];
let versionInfo: VersionInfo;

const severities = ['error', 'warning', 'info', 'verbose'];

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
    logMessageContainer.innerHTML = window.trustedTypes.emptyHTML;
  } else {
    logMessageContainer.innerHTML = '';
  }

  // Only show log lines that match all the words (AND filtering). Accept
  // matches in `message`, `fileAndLine`, and `severity`.
  const filterInput = getRequiredElement<HTMLInputElement>('filter');
  const filterWords =
      filterInput.value.toLowerCase().split(/\s+/).filter(t => t.length > 0);

  const severityCheckboxes: {[key: string]: boolean} =
      Object.fromEntries(severities.map(
          s =>
              [s.toUpperCase(),
               getRequiredElement<HTMLInputElement>(`${s}-checkbox`).checked,
  ]));

  const filteredLogs = logs.filter(log => {
    const matchesFilter = filterWords.every(
        word => log.message.toLowerCase().includes(word) ||
            log.fileAndLine.toLowerCase().includes(word) ||
            log.logSeverity.toLowerCase().includes(word));
    const matchesSeverity = severityCheckboxes[log.logSeverity] ?? true;
    return matchesFilter && matchesSeverity;
  });

  for (const log of filteredLogs) {
    const logMessage = document.createElement('div');
    logMessage.setAttribute('role', 'row');
    logMessage.className = 'log-line';

    // Use en-CA locale to get a format of YYYY-MM-DD, HH:MM:SS.
    const timestamp = new Date(log.timestamp).toLocaleString('en-CA', {
      timeZoneName: 'short',
      hour12: false,
    });

    // Create a row with 4 columns: timestamp, severity, file and line, message.

    const timestampDiv = document.createElement('div');
    timestampDiv.setAttribute('role', 'gridcell');
    timestampDiv.className = 'log-column timestamp';
    timestampDiv.textContent = timestamp;
    logMessage.appendChild(timestampDiv);

    const severityDiv = document.createElement('div');
    severityDiv.setAttribute('role', 'gridcell');
    severityDiv.className = 'log-column severity';
    severityDiv.appendChild(highlightText(log.logSeverity, filterWords));
    logMessage.appendChild(severityDiv);

    const fileAndLineDiv = document.createElement('div');
    fileAndLineDiv.setAttribute('role', 'gridcell');
    fileAndLineDiv.className = 'log-column file-and-line';
    // "file.cc:123" is a link to the file in the Chromium code search.
    const anchor = document.createElement('a');
    anchor.href = log.location;
    anchor.title = log.fileAndLine;
    anchor.setAttribute('target', '_blank');
    // If the name is too long, only shorten the "file" part with ellipses,
    // not the line number.
    const [file, line] = log.fileAndLine.split(':');
    const fileSpan = document.createElement('span');
    fileSpan.className = 'file';
    fileSpan.appendChild(highlightText(file ?? '', filterWords));
    anchor.appendChild(fileSpan);
    anchor.appendChild(highlightText(':' + line, filterWords));
    fileAndLineDiv.appendChild(anchor);
    logMessage.appendChild(fileAndLineDiv);

    const messageDiv = document.createElement('div');
    messageDiv.setAttribute('role', 'gridcell');
    messageDiv.className = 'log-column message';
    messageDiv.appendChild(highlightText(log.message, filterWords));
    logMessage.appendChild(messageDiv);

    logMessageContainer.appendChild(logMessage);
  }
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
  if (policyPageMojoMigrationEnabled) {
    logs =
        (await BrowserProxy.getInstance().handler.getPolicyLogs()).policyLogs;
  } else {
    logs = await sendWithPromise('getPolicyLogs');
  }
}

function initialize() {
  displayVersionInfo();

  const fetchLogsAndDisplay = () => fetchLogs().then(displayList);
  fetchLogsAndDisplay();

  getRequiredElement('logs-dump')
      .addEventListener('click', dumpFileWithJsonContents);
  getRequiredElement('logs-refresh')
      .addEventListener('click', fetchLogsAndDisplay);

  const filterInput = getRequiredElement<HTMLInputElement>('filter');
  filterInput.addEventListener('input', displayList);

  for (const severity of severities) {
    getRequiredElement(`${severity}-checkbox`)
        .addEventListener('change', displayList);
  }
}

interface Interval {
  start: number;
  end: number;
}

// Find all matches of `words` in `text`, and merge matches together.
//
// e.g. findMatchesAndMerge("hello", ["el", "lo"]) matches "ello" and returns
// [{start: 1, end: 4}]
function findMatchesAndMerge(text: string, words: string[]): Interval[] {
  // Find all matches of `words` in `text`, case-insensitively.
  const intervals: Interval[] = [];
  const lowerText = text.toLowerCase();
  for (const word of words) {
    const lowerWord = word.toLowerCase();
    let index = lowerText.indexOf(lowerWord);
    while (index !== -1) {
      intervals.push({start: index, end: index + word.length});
      index = lowerText.indexOf(lowerWord, index + 1);
    }
  }

  if (intervals.length === 0) {
    return [];
  }

  // Sort by start index.
  intervals.sort((a, b) => a.start - b.start);

  // Merge overlapping intervals.
  const merged: Interval[] = [intervals[0]!];
  for (let i = 1; i < intervals.length; i++) {
    const curr = intervals[i]!;
    const last = merged[merged.length - 1]!;
    if (curr.start <= last.end) {
      last.end = Math.max(last.end, curr.end);
    } else {
      merged.push(curr);
    }
  }
  return merged;
}

// Highlights all `words` matches in `text` with `<span class="highlight">`.
// Returns a Node ready to insert in the DOM.
function highlightText(text: string, words: string[]): DocumentFragment {
  const intervals = findMatchesAndMerge(text, words);

  const fragment = document.createDocumentFragment();
  let lastIndex = 0;
  for (const {start, end} of intervals) {
    if (start > lastIndex) {
      // Add normal text.
      fragment.appendChild(
          document.createTextNode(text.substring(lastIndex, start)));
    }
    // Add highlighted text.
    const span = document.createElement('span');
    span.className = 'highlight';
    span.textContent = text.substring(start, end);
    fragment.appendChild(span);
    lastIndex = end;
  }
  if (lastIndex < text.length) {
    // Add the rest of the normal text.
    fragment.appendChild(document.createTextNode(text.substring(lastIndex)));
  }

  return fragment;
}

document.addEventListener('DOMContentLoaded', initialize);
