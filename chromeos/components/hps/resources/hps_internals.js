// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

const POLL_INTERVAL_MS = 500;  // Matches hpsd polling rate.
const MAX_HISTORY = 512 / 4;
const HPS_RESULT_DISABLED = -1;
const HPS_RESULT_UNKNOWN = 0;
const HPS_RESULT_NEGATIVE = 1;
const HPS_RESULT_POSITIVE = 2;

let g_senseState = HPS_RESULT_DISABLED;
let g_notifyState = HPS_RESULT_DISABLED;
let g_pollTimer = undefined;

function hpsResultToString(result) {
  return {
    [HPS_RESULT_DISABLED]: 'disabled',
    [HPS_RESULT_UNKNOWN]: 'unknown',
    [HPS_RESULT_NEGATIVE]: 'negative',
    [HPS_RESULT_POSITIVE]: 'positive',
  }[result];
}

function onConnected(state) {
  const connected = state.connected;
  onSenseChanged({disabled: true});
  onNotifyChanged({disabled: true});
  $('enable-sense').disabled = true;
  $('disable-sense').disabled = true;
  $('enable-notify').disabled = true;
  $('disable-notify').disabled = true;
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

function onSenseChanged(value) {
  if (value.disabled) {
    $('enable-sense').disabled = false;
    $('disable-sense').disabled = true;
    g_senseState = HPS_RESULT_DISABLED;
  } else {
    $('enable-sense').disabled = true;
    $('disable-sense').disabled = false;
    g_senseState = value.state;
  }
  $('sense-state').textContent = hpsResultToString(g_senseState);
  $('sense-state').className = hpsResultToString(g_senseState);
  updatePolling();
}

function onNotifyChanged(value) {
  if (value.disabled) {
    $('enable-notify').disabled = false;
    $('disable-notify').disabled = true;
    g_notifyState = HPS_RESULT_DISABLED;
  } else {
    $('enable-notify').disabled = true;
    $('disable-notify').disabled = false;
    g_notifyState = value.state;
  }
  $('notify-state').textContent = hpsResultToString(g_notifyState);
  $('notify-state').className = hpsResultToString(g_notifyState);
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
      g_notifyState !== HPS_RESULT_DISABLED ||
      g_senseState !== HPS_RESULT_DISABLED;
  if (shouldPoll && g_pollTimer === undefined) {
    g_pollTimer = setInterval(recordSample, POLL_INTERVAL_MS);
    recordSample();
  } else if (!shouldPoll && g_pollTimer !== undefined) {
    clearInterval(g_pollTimer);
    g_pollTimer = undefined;
  }
  $('root').dispatchEvent(new CustomEvent('state-updated-for-test'));
}

function pruneSamples(container) {
  while (container.childElementCount > MAX_HISTORY) {
    container.firstChild.remove();
  }
}

function recordSample() {
  if (g_senseState !== undefined) {
    let sample = document.createElement('span');
    sample.className = hpsResultToString(g_senseState);
    $('sense-history').appendChild(sample);
    pruneSamples($('sense-history'));
  }
  if (g_notifyState !== undefined) {
    let sample = document.createElement('span');
    sample.className = hpsResultToString(g_notifyState);
    $('notify-history').appendChild(sample);
    pruneSamples($('notify-history'));
  }
}

document.addEventListener('DOMContentLoaded', initialize);
