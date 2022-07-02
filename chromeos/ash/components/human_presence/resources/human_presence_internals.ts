// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

const POLL_INTERVAL_MS = 500;  // Matches hpsd polling rate.
const MAX_HISTORY = 512 / 4;

enum HpsResult {
  DISABLED = -1,
  UNKNOWN = 0,
  NEGATIVE = 1,
  POSITIVE = 2,
};

interface ConnectionState {
  connected: boolean;
};

interface IncomingHpsResult {
  state?: number;
  disabled?: boolean;
  inference_result?: number;
  inference_result_valid?: boolean;
};

interface HpsResultState {
  result: HpsResult;
  inference_result?: number;
  inference_result_valid?: boolean;
};

let g_senseState: HpsResultState = {result: HpsResult.DISABLED};
let g_notifyState: HpsResultState = {result: HpsResult.DISABLED};
let g_pollTimer: number|undefined = undefined;

function hpsResultToString(result: HpsResult) {
  switch (result) {
    case HpsResult.DISABLED:
      return 'disabled';
    case HpsResult.UNKNOWN:
      return 'unknown';
    case HpsResult.NEGATIVE:
      return 'negative';
    case HpsResult.POSITIVE:
      return 'positive';
  }
}

function hpsResultToClass(result: HpsResult) {
  // For now we reuse the display strings for class names, but if the UI is ever
  // translated, this should be adapted.
  return hpsResultToString(result);
}

function enableButton(selector: string, enabled: boolean) {
  ($(selector) as HTMLButtonElement).disabled = !enabled;
}

function onConnected(state: ConnectionState) {
  const connected = state.connected;
  onSenseChanged({disabled: true});
  onNotifyChanged({disabled: true});
  enableButton('enable-sense', false);
  enableButton('disable-sense', false);
  enableButton('enable-notify', false);
  enableButton('disable-notify', false);
  $('connection-error').style.display = connected ? 'none' : 'block';
  if (connected) {
    // Query the state of each feature to see if they are enabled or not.
    chrome.send('query_sense');
    chrome.send('query_notify');
  }
  updatePolling();
}

function onEnableError() {
  $('enable-error').style.display = 'block';
}

function onManifest(manifest: string) {
  $('manifest').textContent = manifest;
}

function onSenseChanged(value: IncomingHpsResult) {
  if (value.disabled) {
    enableButton('enable-sense', true);
    enableButton('disable-sense', false);
    g_senseState = {result: HpsResult.DISABLED};
  } else {
    enableButton('enable-sense', false);
    enableButton('disable-sense', true);
    g_senseState = {
      result: value.state!,
      inference_result: value.inference_result!,
      inference_result_valid: value.inference_result_valid!
    };
  }
  $('sense-state').textContent = hpsResultToString(g_senseState.result);
  $('sense-state').className = hpsResultToClass(g_senseState.result);
  updatePolling();
}

function onNotifyChanged(value: IncomingHpsResult) {
  if (value.disabled) {
    enableButton('enable-notify', true);
    enableButton('disable-notify', false);
    g_notifyState = {result: HpsResult.DISABLED};
  } else {
    enableButton('enable-notify', false);
    enableButton('disable-notify', true);
    g_notifyState = {
      result: value.state!,
      inference_result: value.inference_result!,
      inference_result_valid: value.inference_result_valid!
    };
  }
  $('notify-state').textContent = hpsResultToString(g_notifyState.result);
  $('notify-state').className = hpsResultToClass(g_notifyState.result);
  updatePolling();
}

function initialize() {
  addWebUIListener('connected', onConnected);
  addWebUIListener('sense_changed', onSenseChanged);
  addWebUIListener('notify_changed', onNotifyChanged);
  addWebUIListener('enable_error', onEnableError);
  addWebUIListener('manifest', onManifest);
  $('enable-sense').onclick = enableSense;
  $('disable-sense').onclick = disableSense;
  $('enable-notify').onclick = enableNotify;
  $('disable-notify').onclick = disableNotify;
  $('show-info').onclick = showInfo;
  onConnected({connected: false});
  chrome.send('connect');
}

function enableSense() {
  $('enable-error').style.display = 'none';
  chrome.send('enable_sense');
}

function disableSense() {
  chrome.send('disable_sense');
  // Query the feature state immediately after toggling it off to make sure the
  // UI always reflects the latest state. This is needed because hpsd sends an
  // UNKNOWN result whenever a feature is turned off.
  chrome.send('query_sense');
}

function enableNotify() {
  $('enable-error').style.display = 'none';
  chrome.send('enable_notify');
}

function disableNotify() {
  chrome.send('disable_notify');
  // Query the feature state immediately after toggling it off to make sure the
  // UI always reflects the latest state. This is needed because hpsd sends an
  // UNKNOWN result whenever a feature is turned off.
  chrome.send('query_notify');
}

function showInfo() {
  ($('info-dialog') as any).showModal();
}

function updatePolling() {
  const shouldPoll =
      g_notifyState.result !== HpsResult.DISABLED ||
      g_senseState.result !== HpsResult.DISABLED;
  if (shouldPoll && g_pollTimer === undefined) {
    g_pollTimer = setInterval(recordSample, POLL_INTERVAL_MS);
    recordSample();
  } else if (!shouldPoll && g_pollTimer !== undefined) {
    clearInterval(g_pollTimer);
    g_pollTimer = undefined;
  }
  $('root').dispatchEvent(new CustomEvent('state-updated-for-test'));
}

function pruneSamples(container: HTMLElement) {
  while (container.childElementCount > MAX_HISTORY) {
    container.firstChild!.remove();
  }
}

function recordSampleForFeature(state: HpsResultState, featureName: String) {
  if (state.result === undefined)
    return;
  let sample = document.createElement('span');
  sample.className = hpsResultToClass(state.result);
  $(`${featureName}-history`).appendChild(sample);

  sample = document.createElement('span');
  let height = '64px';
  if (state.inference_result !== undefined) {
    let score = state.inference_result!;
    height = Math.max(0, Math.min(128, Math.floor(score / 2) + 64)) + 'px';
    $(`${featureName}-inference-result`).textContent = score.toString();
  } else {
    $(`${featureName}-inference-result`).textContent = "—";
  }
  if (!state.inference_result_valid) {
    sample.classList.add("invalid");
  }
  sample.style.height = height;
  $(`${featureName}-inference-result`).style.height = height;
  $(`${featureName}-inference-history`).appendChild(sample);

  pruneSamples($(`${featureName}-history`));
  pruneSamples($(`${featureName}-inference-history`));
}

function recordSample() {
  recordSampleForFeature(g_senseState, 'sense');
  recordSampleForFeature(g_notifyState, 'notify');
}

document.addEventListener('DOMContentLoaded', initialize);
