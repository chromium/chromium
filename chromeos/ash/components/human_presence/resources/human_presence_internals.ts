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

interface HpsResultState {
  state?: number;
  disabled?: boolean;
};

let g_senseState = HpsResult.DISABLED;
let g_notifyState = HpsResult.DISABLED;
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

function onSenseChanged(value: HpsResultState) {
  if (value.disabled) {
    enableButton('enable-sense', true);
    enableButton('disable-sense', false);
    g_senseState = HpsResult.DISABLED;
  } else {
    enableButton('enable-sense', false);
    enableButton('disable-sense', true);
    g_senseState = value.state!;
  }
  $('sense-state').textContent = hpsResultToString(g_senseState);
  $('sense-state').className = hpsResultToClass(g_senseState);
  updatePolling();
}

function onNotifyChanged(value: HpsResultState) {
  if (value.disabled) {
    enableButton('enable-notify', true);
    enableButton('disable-notify', false);
    g_notifyState = HpsResult.DISABLED;
  } else {
    enableButton('enable-notify', false);
    enableButton('disable-notify', true);
    g_notifyState = value.state!;
  }
  $('notify-state').textContent = hpsResultToString(g_notifyState);
  $('notify-state').className = hpsResultToClass(g_notifyState);
  updatePolling();
}

function initialize() {
  addWebUIListener('connected', onConnected);
  addWebUIListener('sense_changed', onSenseChanged);
  addWebUIListener('notify_changed', onNotifyChanged);
  addWebUIListener('enable_error', onEnableError);
  $('enable-sense').onclick = enableSense;
  $('disable-sense').onclick = disableSense;
  $('enable-notify').onclick = enableNotify;
  $('disable-notify').onclick = disableNotify;
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

function updatePolling() {
  const shouldPoll =
      g_notifyState !== HpsResult.DISABLED ||
      g_senseState !== HpsResult.DISABLED;
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

function recordSample() {
  if (g_senseState !== undefined) {
    let sample = document.createElement('span');
    sample.className = hpsResultToClass(g_senseState);
    $('sense-history').appendChild(sample);
    pruneSamples($('sense-history'));
  }
  if (g_notifyState !== undefined) {
    let sample = document.createElement('span');
    sample.className = hpsResultToClass(g_notifyState);
    $('notify-history').appendChild(sample);
    pruneSamples($('notify-history'));
  }
}

document.addEventListener('DOMContentLoaded', initialize);
