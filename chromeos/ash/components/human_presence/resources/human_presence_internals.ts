// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

const POLL_INTERVAL_MS = 500;  // Matches hpsd polling rate.
const MAX_HISTORY = 512 / 4;

// The field number of the config in the hps::FeatureConfig proto.
const CONSECUTIVE_RESULTS_FILTER_CONFIG_INDEX_IN_PROTO = 2;
const AVERAGE_FILTER_CONFIG_INDEX_IN_PROTO = 3;
// THe index of the config in the selection list in the ui.
// This does not need to match the field above; but matching them causes less
// confusion.
const CONSECUTIVE_RESULTS_FILTER_CONFIG_INDEX_IN_SELECT_LIST = 2;
const AVERAGE_FILTER_CONFIG_INDEX_IN_SELECT_LIST = 3;
// The style to show the "Apply" button and the config panels.
const CONFIG_DISPLAY_STYLE = 'inline-block';

const FEATURE_NAME_SENSE = 'sense';
const FEATURE_NAME_NOTIFY = 'notify';

enum HpsResult {
  DISABLED = -1,
  UNKNOWN = 0,
  NEGATIVE = 1,
  POSITIVE = 2,
}

interface ConnectionState {
  connected: boolean;
}

interface IncomingHpsResult {
  state?: number;
  disabled?: boolean;
  inference_result?: number;
  inference_result_valid?: boolean;
}

interface HpsResultState {
  result: HpsResult;
  inference_result?: number;
  inference_result_valid?: boolean;
}

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
  getRequiredElement<HTMLButtonElement>(selector).disabled = !enabled;
}

function onConnected(state: ConnectionState) {
  const connected = state.connected;
  onSenseChanged({disabled: true});
  onNotifyChanged({disabled: true});
  enableButton('enable-sense', false);
  enableButton('disable-sense', false);
  enableButton('enable-notify', false);
  enableButton('disable-notify', false);
  getRequiredElement('connection-error').style.display =
      connected ? 'none' : 'block';
  if (connected) {
    // Query the state of each feature to see if they are enabled or not.
    chrome.send('query_sense');
    chrome.send('query_notify');
  }
  updatePolling();
}

function onEnableError() {
  getRequiredElement('enable-error').style.display = 'block';
}

function onManifest(manifest: string) {
  getRequiredElement('manifest').textContent = manifest;
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
  getRequiredElement('sense-state').textContent =
      hpsResultToString(g_senseState.result);
  getRequiredElement('sense-state').className =
      hpsResultToClass(g_senseState.result);
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
  getRequiredElement('notify-state').textContent =
      hpsResultToString(g_notifyState.result);
  getRequiredElement('notify-state').className =
      hpsResultToClass(g_notifyState.result);
  updatePolling();
}

function initialize() {
  addWebUiListener('connected', onConnected);
  addWebUiListener('sense_changed', onSenseChanged);
  addWebUiListener('notify_changed', onNotifyChanged);
  addWebUiListener('enable_error', onEnableError);
  addWebUiListener('manifest', onManifest);
  getRequiredElement('enable-sense').onclick = enableSense;
  getRequiredElement('disable-sense').onclick = disableSense;
  getRequiredElement('enable-notify').onclick = enableNotify;
  getRequiredElement('disable-notify').onclick = disableNotify;
  getRequiredElement('select-sense').onchange =
      () => selectFilter(FEATURE_NAME_SENSE);
  getRequiredElement('select-notify').onchange =
      () => selectFilter(FEATURE_NAME_NOTIFY);
  getRequiredElement('apply-select-sense').onclick =
      () => applySelectFilter(FEATURE_NAME_SENSE);
  getRequiredElement('apply-select-notify').onclick = () =>
      applySelectFilter(FEATURE_NAME_NOTIFY);
  getRequiredElement('show-info').onclick = showInfo;
  onConnected({connected: false});
  chrome.send('connect');
}

function enableSense() {
  getRequiredElement('enable-error').style.display = 'none';
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
  getRequiredElement('enable-error').style.display = 'none';
  chrome.send('enable_notify');
}

function disableNotify() {
  chrome.send('disable_notify');
  // Query the feature state immediately after toggling it off to make sure the
  // UI always reflects the latest state. This is needed because hpsd sends an
  // UNKNOWN result whenever a feature is turned off.
  chrome.send('query_notify');
}

function parseFilterConfig(featureName: String) {
  const selectFeature =
      getRequiredElement<HTMLSelectElement>(`select-${featureName}`);
  if (selectFeature.selectedIndex !==
          CONSECUTIVE_RESULTS_FILTER_CONFIG_INDEX_IN_SELECT_LIST &&
      selectFeature.selectedIndex !==
          AVERAGE_FILTER_CONFIG_INDEX_IN_SELECT_LIST) {
    return undefined;
  }

  const filterName = selectFeature.options[selectFeature.selectedIndex]!.text;
  const fields = new Map();
  let filedNames: String[] = [];
  if (selectFeature.selectedIndex ===
      CONSECUTIVE_RESULTS_FILTER_CONFIG_INDEX_IN_SELECT_LIST) {
    fields.set(
        'filter_config_case', CONSECUTIVE_RESULTS_FILTER_CONFIG_INDEX_IN_PROTO);
    filedNames = [
      'positive_score_threshold', 'negative_score_threshold',
      'positive_count_threshold', 'negative_count_threshold',
      'uncertain_count_threshold'
    ]
  }

  if (selectFeature.selectedIndex ===
      AVERAGE_FILTER_CONFIG_INDEX_IN_SELECT_LIST) {
    fields.set('filter_config_case', AVERAGE_FILTER_CONFIG_INDEX_IN_PROTO);
    filedNames = [
      'average_window_size', 'positive_score_threshold',
      'negative_score_threshold', 'default_uncertain_score'
    ]
  }

  // Extract each field based on the config selected.
  for (const field of filedNames) {
    const fieldInput =
        getRequiredElement<HTMLInputElement>(
            `${featureName}-${filterName}-${field}`);
    fields.set(field, parseInt(fieldInput.value, 10));
  }
  // Return the result as a JSON map.
  return [Object.fromEntries(fields)];
}

function selectFilter(featureName: String) {
  hideFilterConfigPanel(featureName);

  const selectElement =
      getRequiredElement<HTMLSelectElement>(`select-${featureName}`);
  // Show the "Apply" button.
  if (selectElement.selectedIndex !== 0) {
    getRequiredElement(`apply-select-${featureName}`).style.display =
        CONFIG_DISPLAY_STYLE;
  }

  // Show the config panel based on the selection.
  if (selectElement.selectedIndex ===
      CONSECUTIVE_RESULTS_FILTER_CONFIG_INDEX_IN_SELECT_LIST) {
    getRequiredElement(
        `select-${featureName}-consecutive_results_filter_config`)
        .style.display =
        CONFIG_DISPLAY_STYLE;
  }

  if (selectElement.selectedIndex ==
      AVERAGE_FILTER_CONFIG_INDEX_IN_SELECT_LIST) {
    getRequiredElement(
        `select-${featureName}-average_filter_config`).style.display =
        CONFIG_DISPLAY_STYLE;
  }
}

function applySelectFilter(featureName: String) {
  hideFilterConfigPanel(featureName);
  const selectElement =
      getRequiredElement<HTMLSelectElement>(`select-${featureName}`);
  // Apply new config if selected.
  if (selectElement.selectedIndex !== 0) {
    chrome.send(`disable_${featureName}`);
    chrome.send(
        `enable_${featureName}`, parseFilterConfig(featureName));
    getRequiredElement(`apply-select-${featureName}-complete`).style.display =
        CONFIG_DISPLAY_STYLE;
  }
  // Reset the selection.
  selectElement.selectedIndex = 0;
}

function hideFilterConfigPanel(featureName: String) {
  // Hide the label for indicating applying was complete.
  getRequiredElement(`apply-select-${featureName}-complete`).style.display =
      'none';
  // Hide the "Apply" button.
  getRequiredElement(`apply-select-${featureName}`).style.display = 'none';
  // Hid both config panels.
  getRequiredElement(
      `select-${featureName}-average_filter_config`).style.display = 'none';
  getRequiredElement(
      `select-${featureName}-consecutive_results_filter_config`).style.display =
      'none';
}

function showInfo() {
  getRequiredElement<HTMLDialogElement&{showModal: () => void}>(
      'info-dialog').showModal();
}

function updatePolling() {
  const shouldPoll = g_notifyState.result !== HpsResult.DISABLED ||
      g_senseState.result !== HpsResult.DISABLED;
  if (shouldPoll && g_pollTimer === undefined) {
    g_pollTimer = setInterval(recordSample, POLL_INTERVAL_MS);
    recordSample();
  } else if (!shouldPoll && g_pollTimer !== undefined) {
    clearInterval(g_pollTimer);
    g_pollTimer = undefined;
  }
  getRequiredElement('root').dispatchEvent(
      new CustomEvent('state-updated-for-test'));
}

function pruneSamples(container: HTMLElement) {
  while (container.childElementCount > MAX_HISTORY) {
    container.firstChild!.remove();
  }
}

function recordSampleForFeature(state: HpsResultState, featureName: String) {
  if (state.result === undefined) return;
  let sample = document.createElement('span');
  sample.className = hpsResultToClass(state.result);
  getRequiredElement(`${featureName}-history`).appendChild(sample);

  sample = document.createElement('span');
  let height = '64px';
  if (state.inference_result !== undefined) {
    let score = state.inference_result!;
    height = Math.max(0, Math.min(128, Math.floor(score / 2) + 64)) + 'px';
    getRequiredElement(`${featureName}-inference-result`).textContent =
        score.toString();
  } else {
    getRequiredElement(`${featureName}-inference-result`).textContent = '—';
  }
  if (!state.inference_result_valid) {
    sample.classList.add('invalid');
  }
  sample.style.height = height;
  getRequiredElement(`${featureName}-inference-result`).style.height = height;
  getRequiredElement(`${featureName}-inference-history`).appendChild(sample);

  pruneSamples(getRequiredElement(`${featureName}-history`));
  pruneSamples(getRequiredElement(`${featureName}-inference-history`));
}

function recordSample() {
  recordSampleForFeature(g_senseState, FEATURE_NAME_SENSE);
  recordSampleForFeature(g_notifyState, FEATURE_NAME_NOTIFY);
}

document.addEventListener('DOMContentLoaded', initialize);
