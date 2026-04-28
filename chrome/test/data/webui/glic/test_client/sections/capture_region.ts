// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CaptureRegionResult, Subscriber} from '/glic/glic_api/glic_api.js';

import {client, getBrowser, logMessage} from '../client.js';
import {$} from '../page_element_types.js';

let captureRegionSubscription: Subscriber|null = null;

function resetCaptureButton() {
  $.captureRegionBtn.textContent = 'Capture Region';
  $.captureRegionBtn.removeAttribute('pressed');
  if (captureRegionSubscription) {
    captureRegionSubscription.unsubscribe();
    captureRegionSubscription = null;
  }
}

function onCaptureRegionClick() {
  const browser = getBrowser();
  if (!browser) {
    $.captureRegionResultList.textContent = 'Browser API not available.';
    return;
  }

  if (captureRegionSubscription) {
    // If capture is in progress, cancel it.
    captureRegionSubscription.unsubscribe();
    resetCaptureButton();
  } else {
    // Start capturing.
    if (!browser.captureRegion) {
      $.captureRegionResultList.textContent = 'captureRegion() not supported.';
      return;
    }

    logMessage('Starting region capture...');
    $.captureRegionBtn.textContent = 'Cancel Capture';
    $.captureRegionBtn.setAttribute('pressed', 'true');
    $.captureRegionResultList.innerHTML = '';

    const observable = browser.captureRegion({
      tabId: client.getFocusedTabId(),
      options: {
        viewportScreenshot: true,
        annotatedPageContent: true,
      },
    });
    captureRegionSubscription = observable.subscribeObserver!({
      next: (result: CaptureRegionResult) => {
        logMessage(`Region captured: ${JSON.stringify(result)}`);
        const li = document.createElement('li');
        li.textContent = `tabId: ${result.tabId}, rect: ${
            JSON.stringify(result.region?.rect)}`;
        $.captureRegionResultList.appendChild(li);
        resetCaptureButton();
      },
      error: (err: any) => {
        logMessage(`Capture region error: ${err}`);
        const li = document.createElement('li');
        li.textContent = `Error: ${err.message}`;
        $.captureRegionResultList.appendChild(li);
        resetCaptureButton();
      },
      complete: () => {
        logMessage('Capture complete.');
        const li = document.createElement('li');
        li.textContent = 'Capture complete.';
        $.captureRegionResultList.appendChild(li);
        resetCaptureButton();
      },
    });
  }
}

function onDeleteRegionClick() {
  const browser = getBrowser();
  if (!browser) {
    $.captureRegionResultList.textContent = 'Browser API not available.';
    return;
  }

  // Start capturing.
  if (!browser.deleteCapturedRegion) {
    $.captureRegionResultList.textContent =
        'deleteCapturedRegion() not supported.';
    return;
  }
  logMessage('Starting delete region capture...');
  browser.deleteCapturedRegion(
      client.getFocusedTabId(), $.deleteCaptureRegion.value);
}

export function initCaptureRegion() {
  $.captureRegionBtn.addEventListener('click', onCaptureRegionClick);
  $.deleteCaptureRegionBtn.addEventListener('click', onDeleteRegionClick);
}
