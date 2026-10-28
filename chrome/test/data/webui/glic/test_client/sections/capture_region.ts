// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CaptureRegionResult, Subscriber} from '/glic/glic_api/glic_api.js';

import {client, getBrowser, logMessage, releaseHeldCaptureRegionHotkey} from '../client.js';
import {$} from '../page_element_types.js';

let captureRegionSubscription: Subscriber|null = null;
// Runs once the next capture settles (result, error or completion).
let onCaptureSettled: (() => void)|null = null;

function resetCaptureButton() {
  $.captureRegionBtn.textContent = 'Capture Region';
  $.captureRegionBtn.removeAttribute('pressed');
  if (captureRegionSubscription) {
    captureRegionSubscription.unsubscribe();
    captureRegionSubscription = null;
  }
  const settled = onCaptureSettled;
  onCaptureSettled = null;
  settled?.();
}

// Answers a held CAPTURE_REGION_HOTKEY invoke the way the real client does:
// start the capture, then complete the invoke. The completion waits for the
// browser's response to the capture, so a test can rely on the browser having
// handled the capture request before the invoke ends.
function onReleaseCaptureRegionHotkeyClick() {
  onCaptureSettled = releaseHeldCaptureRegionHotkey;
  onCaptureRegionClick();
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
  $.releaseCaptureRegionHotkey.addEventListener(
      'click', onReleaseCaptureRegionHotkeyClick);
  $.deleteCaptureRegionBtn.addEventListener('click', onDeleteRegionClick);
}
