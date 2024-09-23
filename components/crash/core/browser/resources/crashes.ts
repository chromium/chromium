// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import 'chrome://resources/js/action_link.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {appendParam, getRequiredElement} from 'chrome://resources/js/util.js';

/* Id for tracking automatic refresh of crash list.  */
let refreshCrashListId: number|undefined = undefined;

/**
 * Requests the list of crashes from the backend.
 */
function requestCrashes() {
  chrome.send('requestCrashList');
}

/**
 * Format filesize in appropriate display units.
 */
function formatBytes(bytes: number): string {
  const k = 1024;
  const units = ['B', 'KB', 'MB', 'GB', 'TB', 'PB'];

  let unitAmount = bytes;
  let unit = 0;
  for (unit; unitAmount >= k && unit < units.length - 1; unit++) {
    unitAmount /= k;
  }
  const unitAmountLocalized = unitAmount.toLocaleString(
      undefined,  // Default locale.
      {maximumFractionDigits: 2});

  return `${unitAmountLocalized} ${units[unit]}`;
}

// Keep in sync with components/crash/core/browser/crashes_ui_util.cc.
enum State {
  NOT_UPLOADED = 'not_uploaded',
  PENDING = 'pending',
  PENDING_USER_REQUESTED = 'pending_user_requested',
  UPLOADED = 'uploaded',
}

interface CrashData {
  file_size?: number;
  id: string;
  local_id: string;
  state: State;
  capture_time?: string;
  upload_time?: string;
}

interface UpdateCrashListParams {
  enabled: boolean;
  dynamicBackend: boolean;
  manualUploads: boolean;
  crashes: CrashData[];
  version: string;
  os: string;
  isGoogleAccount: boolean;
}

/**
 * Callback from backend with the list of crashes. Builds the UI.
 */
function updateCrashList({
  enabled,
  dynamicBackend,
  manualUploads,
  crashes,
  version,
  os,
  isGoogleAccount,
}: UpdateCrashListParams) {
  getRequiredElement('crashesCount').textContent = loadTimeData.getStringF(
      'crashCountFormat', crashes.length.toLocaleString());

  const crashList = getRequiredElement('crashList');

  getRequiredElement('disabledMode').hidden = enabled;
  getRequiredElement('crashUploadStatus').hidden = !enabled || !dynamicBackend;

  const template = crashList.querySelector('template');
  assert(template);

  // Clear any previous list.
  crashList.querySelectorAll('.crash-row').forEach((elm) => elm.remove());

  const productName = loadTimeData.getString('shortProductName');

  for (const crash of crashes) {
    if (crash.local_id === '') {
      crash.local_id = productName;
    }

    const clone = template.content.cloneNode(true) as HTMLElement;
    if (crash.state !== State.UPLOADED) {
      const crashRow = clone.querySelector('.crash-row');
      assert(crashRow);
      crashRow.classList.add('not-uploaded');
    }

    const uploaded = crash.state === State.UPLOADED;

    // Some clients do not distinguish between capture time and upload time,
    // so use the latter if the former is not available.
    const captureTime = clone.querySelector('.capture-time');
    assert(captureTime);
    captureTime.textContent = loadTimeData.getStringF(
        'crashCaptureTimeFormat',
        crash.capture_time || crash.upload_time || '');
    const localIdCell = clone.querySelector('.local-id .value');
    assert(localIdCell);
    localIdCell.textContent = crash.local_id;

    let stateText = '';
    switch (crash.state) {
      case State.NOT_UPLOADED:
        stateText = loadTimeData.getString('crashStatusNotUploaded');
        break;
      case State.PENDING:
        stateText = loadTimeData.getString('crashStatusPending');
        break;
      case State.PENDING_USER_REQUESTED:
        stateText = loadTimeData.getString('crashStatusPendingUserRequested');
        break;
      case State.UPLOADED:
        stateText = loadTimeData.getString('crashStatusUploaded');
        break;
      default:
        continue;  // Unknown state.
    }
    const statusCell = clone.querySelector('.status .value');
    assert(statusCell);
    statusCell.textContent = stateText;

    const uploadId = clone.querySelector('.upload-id');
    assert(uploadId);
    const uploadTime = clone.querySelector('.upload-time');
    assert(uploadTime);
    const sendNowButton = clone.querySelector<HTMLButtonElement>('.send-now');
    assert(sendNowButton);
    const fileBugButton = clone.querySelector<HTMLButtonElement>('.file-bug');
    assert(fileBugButton);
    if (uploaded) {
      const uploadIdValue = uploadId.querySelector('.value');
      assert(uploadIdValue);
      if (isGoogleAccount) {
        const crashLink = document.createElement('a');
        crashLink.href = `https://goto.google.com/crash/${crash.id}`;
        crashLink.target = '_blank';
        crashLink.textContent = crash.id;
        uploadIdValue.appendChild(crashLink);
      } else {
        uploadIdValue.textContent = crash.id;
      }

      const uploadTimeCell = uploadTime.querySelector('.value');
      assert(uploadTimeCell);
      uploadTimeCell.textContent = crash.upload_time || '';

      sendNowButton.remove();
      fileBugButton.onclick = () => fileBug(crash.id, os, version);
    } else {
      uploadId.remove();
      uploadTime.remove();
      fileBugButton.remove();
      // Do not allow crash submission if the Chromium build does not support
      // it, or if the user already requested it.
      if (!manualUploads || crash.state === State.PENDING_USER_REQUESTED) {
        sendNowButton.remove();
      }
      sendNowButton.onclick = (_e: Event) => {
        sendNowButton.disabled = true;
        chrome.send('requestSingleCrashUpload', [crash.local_id]);
      };
    }

    const fileSize = clone.querySelector('.file-size');
    assert(fileSize);
    if (crash.file_size === undefined) {
      fileSize.remove();
    } else {
      const fileSizeCell = fileSize.querySelector('.value');
      assert(fileSizeCell);
      fileSizeCell.textContent = formatBytes(crash.file_size);
    }

    crashList.appendChild(clone);
  }

  getRequiredElement('noCrashes').hidden = crashes.length !== 0;
}

/**
 * Opens a new tab/window to report the crash to crbug.
 * @param The crash report ID.
 * @param The OS name.
 * @param The product version.
 */
function fileBug(crashId: string, os: string, version: string) {
  const commentLines = [
    'IMPORTANT: Your crash has already been automatically reported ' +
        'to our crash system. Please file this bug only if you can provide ' +
        'more information about it.',
    '',
    '',
    'Chrome Version: ' + version,
    'Operating System: ' + os,
    '',
    'URL (if applicable) where crash occurred:',
    '',
    'Can you reproduce this crash?',
    '',
    'What steps will reproduce this crash? (If it\'s not ' +
        'reproducible, what were you doing just before the crash?)',
    '1.',
    '2.',
    '3.',
    '',
    '****DO NOT CHANGE BELOW THIS LINE****',
    'Crash ID: crash/' + crashId,
  ];
  const params: {[key: string]: string} = {
    template: 'Crash Report',
    comment: commentLines.join('\n'),
    // TODO(scottmg): Use add_labels to add 'User-Submitted' rather than
    // duplicating the template's labels (the first two) once
    // https://bugs.chromium.org/p/monorail/issues/detail?id=1488 is done.
    labels:
        'Restrict-View-EditIssue,Stability-Crash,User-Submitted,Pri-3,Type-Bug',
  };
  let href = 'https://bugs.chromium.org/p/chromium/issues/entry';
  for (const param in params) {
    href = appendParam(href, param, params[param]!);
  }

  window.open(href);
}

/**
 * Request crashes get uploaded in the background.
 */
function requestCrashUpload() {
  // Don't need locking with this call because the system crash reporter
  // has locking built into itself.
  chrome.send('requestCrashUpload');

  // Trigger a refresh in 5 seconds.  Clear any previous requests.
  clearTimeout(refreshCrashListId);
  refreshCrashListId = setTimeout(requestCrashes, 5000);
}

/**
 * Toggles hiding/showing the developer details of a crash report, depending
 * on the value of the check box.
 */
function toggleDevDetails(e: Event) {
  getRequiredElement('crashList')
      .classList.toggle(
          'showing-dev-details', (e.target as HTMLInputElement).checked);
}

document.addEventListener('DOMContentLoaded', function() {
  addWebUiListener('update-crash-list', updateCrashList);
  getRequiredElement('uploadCrashes').onclick = requestCrashUpload;
  getRequiredElement('showDevDetails').onclick = toggleDevDetails;
  requestCrashes();
});
