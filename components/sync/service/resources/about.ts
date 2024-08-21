// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';

import {assert} from 'chrome://resources/js/assert.js';
import type {WebUiListener} from 'chrome://resources/js/cr.js';
import {addWebUiListener, removeWebUiListener} from 'chrome://resources/js/cr.js';

import {requestDataAndRegisterForUpdates, requestStart, setIncludeSpecifics, triggerRefresh} from './chrome_sync.js';
import type {ProtocolEvent} from './traffic_log.js';

// Contains the latest snapshot of sync about info.
interface TypeStatus {
  name: string;
  num_entries: number;
  num_live: number;
}

interface Detail {
  is_sensitive: boolean;
  data?: Data[];
}

interface Data {
  stat_name: string;
  stat_value: string;
  stat_status: string;
}

interface AboutInfo {
  details?: Detail[];
  type_status?: TypeStatus[];
}

export let aboutInfo: AboutInfo = {};

// For tests
export function getAboutInfoForTest(): AboutInfo {
  return aboutInfo;
}

let aboutInfoListener: WebUiListener|null = null;
let entityCountsUpdatedListener: WebUiListener|null = null;

function highlightIfChanged(node: HTMLElement, oldVal: number, newVal: number) {
  const oldStr = oldVal.toString();
  const newStr = newVal.toString();
  if (oldStr !== '' && oldStr !== newStr) {
    // Note the addListener function does not end up creating duplicate
    // listeners.  There can be only one listener per event at a time.
    // Reference: https://developer.mozilla.org/en/DOM/element.addEventListener
    node.addEventListener('webkitAnimationEnd', function() {
      node.removeAttribute('highlighted');
    }, false);
    node.setAttribute('highlighted', '');
  }
}

function refreshAboutInfo(newAboutInfo: AboutInfo) {
  aboutInfo = newAboutInfo;
  const aboutInfoDiv = document.querySelector<HTMLElement>('#about-info');
  assert(aboutInfoDiv);
  jstProcess(new JsEvalContext(aboutInfo), aboutInfoDiv);
}

interface EntityCount {
  dataType: string;
  entities: number;
  nonTombstoneEntities: number;
}

let updateEntityCountsTimeoutID = -1;

function updateEntityCounts() {
  updateEntityCountsTimeoutID = -1;

  const typeInfo = document.querySelector<HTMLElement>('#typeInfo');
  assert(typeInfo);
  jstProcess(new JsEvalContext({type_status: aboutInfo.type_status}), typeInfo);
}

function onEntityCountsUpdatedEvent(response: {entityCounts: EntityCount}) {
  if (!aboutInfo.type_status) {
    return;
  }

  const typeStatusRow = aboutInfo.type_status.find(
      row => row.name === response.entityCounts.dataType);
  if (typeStatusRow) {
    typeStatusRow.num_entries = response.entityCounts.entities;
    typeStatusRow.num_live = response.entityCounts.nonTombstoneEntities;
  }

  // onEntityCountsUpdatedEvent() typically gets called multiple times in quick
  // succession (once for each data type). To avoid lots of almost-simultaneous
  // updates to the HTML table (which would result in UI jank), delay updating
  // just a bit.
  if (updateEntityCountsTimeoutID === -1) {
    updateEntityCountsTimeoutID = setTimeout(updateEntityCounts, 10);
  }
}

/**
 * Helper to determine if an element is scrolled to its bottom limit.
 * @param elem element to check
 * @return true if the element is scrolled to the bottom
 */
function isScrolledToBottom(elem: HTMLElement): boolean {
  return elem.scrollHeight - elem.scrollTop === elem.clientHeight;
}

/**
 * Helper to scroll an element to its bottom limit.
 */
function scrollToBottom(elem: HTMLElement) {
  elem.scrollTop = elem.scrollHeight - elem.clientHeight;
}

/** Container for accumulated sync protocol events. */
const protocolEvents: ProtocolEvent[] = [];

/** We may receive re-delivered events.  Keep a record of ones we've seen. */
const knownEventTimestamps: {[key: string]: boolean} = {};

/**
 * Callback for incoming protocol events.
 * @param response The protocol event response.
 */
function onReceivedProtocolEvent(response: ProtocolEvent) {
  // Return early if we've seen this event before.  Assumes that timestamps
  // are sufficiently high resolution to uniquely identify an event.
  if (knownEventTimestamps.hasOwnProperty(response.time)) {
    return;
  }

  knownEventTimestamps[response.time] = true;
  protocolEvents.push(response);

  const trafficContainer =
      document.querySelector<HTMLElement>('#traffic-event-container');
  assert(trafficContainer);

  // Scroll to the bottom if we were already at the bottom.  Otherwise, leave
  // the scrollbar alone.
  const shouldScrollDown = isScrolledToBottom(trafficContainer);

  const context = new JsEvalContext({events: protocolEvents});
  jstProcess(context, trafficContainer);

  if (shouldScrollDown) {
    scrollToBottom(trafficContainer);
  }
}

/**
 * Initializes state and callbacks for the protocol event log UI.
 */
function initProtocolEventLog() {
  const includeSpecificsCheckbox =
      document.querySelector<HTMLInputElement>('#capture-specifics');
  assert(includeSpecificsCheckbox);
  includeSpecificsCheckbox.addEventListener('change', () => {
    setIncludeSpecifics(includeSpecificsCheckbox.checked);
  });

  addWebUiListener('onProtocolEvent', onReceivedProtocolEvent);

  // Make the prototype jscontent element disappear.
  const container =
      document.querySelector<HTMLElement>('#traffic-event-container');
  assert(container);
  jstProcess({}, container);

  const triggerRefreshButton =
      document.querySelector<HTMLElement>('#trigger-refresh');
  assert(triggerRefreshButton);
  triggerRefreshButton.addEventListener('click', () => {
    triggerRefresh();
  });
}

/**
 * Initializes listeners for status dump and import UI.
 */
function initStatusDumpButton() {
  const statusData = document.querySelector<HTMLElement>('#status-data');
  assert(statusData);
  statusData.hidden = true;

  const dumpStatusButton = document.querySelector<HTMLElement>('#dump-status');
  assert(dumpStatusButton);
  dumpStatusButton.addEventListener('click', () => {
    const aboutInfoCopy = aboutInfo;
    const includeIds = document.querySelector<HTMLInputElement>('#include-ids');
    assert(includeIds);
    if (!includeIds.checked) {
      aboutInfoCopy.details = aboutInfo.details!.filter(function(el) {
        return !el.is_sensitive;
      });
    }
    let data = '';
    data += new Date().toString() + '\n';
    data += '======\n';
    data += 'Status\n';
    data += '======\n';
    data += JSON.stringify(aboutInfoCopy, null, 2) + '\n';
    data += '\n';
    data += '===\n';
    data += 'Log\n';
    data += '===\n';
    data += JSON.stringify(protocolEvents, null, 2);

    const statusText =
        document.querySelector<HTMLTextAreaElement>('#status-text');
    assert(statusText);
    statusText.value = data;
    const statusData = document.querySelector<HTMLElement>('#status-data');
    assert(statusData);
    statusData.hidden = false;
  });

  const importStatusButton =
      document.querySelector<HTMLElement>('#import-status');
  assert(importStatusButton);
  importStatusButton.addEventListener('click', () => {
    const statusData = document.querySelector<HTMLElement>('#status-data');
    assert(statusData);
    statusData.hidden = false;
    const statusText =
        document.querySelector<HTMLTextAreaElement>('#status-text');
    assert(statusText);
    if (statusText.value.length === 0) {
      statusText.value = 'Paste sync status dump here then click import.';
      return;
    }

    // First remove any characters before the '{'.
    let data = statusText.value;
    const firstBrace = data.indexOf('{');
    if (firstBrace < 0) {
      statusText.value = 'Invalid sync status dump.';
      return;
    }
    data = data.substr(firstBrace);

    // Remove listeners to prevent sync events from overwriting imported data.
    if (aboutInfoListener) {
      removeWebUiListener(aboutInfoListener);
      aboutInfoListener = null;
    }

    if (entityCountsUpdatedListener) {
      removeWebUiListener(entityCountsUpdatedListener);
      entityCountsUpdatedListener = null;
    }

    const aboutInfo = JSON.parse(data);
    refreshAboutInfo(aboutInfo);
  });
}

/**
 * Toggles the given traffic event entry div's "expanded" state.
 * @param e the click event that triggered the toggle.
 */
function expandListener(e: MouseEvent) {
  if ((e.target as HTMLElement).classList.contains('proto')) {
    // We ignore proto clicks to keep it copyable.
    return;
  }
  let trafficEventDiv = e.target as HTMLElement;
  // Click might be on div's child.
  if (trafficEventDiv.nodeName !== 'DIV' && trafficEventDiv.parentNode) {
    trafficEventDiv = trafficEventDiv.parentNode as HTMLElement;
  }
  trafficEventDiv.classList.toggle('traffic-event-entry-expanded');
}

/**
 * Attaches a listener to the given traffic event entry div.
 * @param element the element to attach the listener to.
 */
function addAboutExpandListener(element: HTMLElement) {
  element.addEventListener('click', expandListener, false);
}

function onLoad() {
  initStatusDumpButton();
  initProtocolEventLog();

  aboutInfoListener = addWebUiListener('onAboutInfoUpdated', refreshAboutInfo);

  entityCountsUpdatedListener =
      addWebUiListener('onEntityCountsUpdated', onEntityCountsUpdatedEvent);

  const requestStartEl = document.querySelector<HTMLElement>('#request-start');
  assert(requestStartEl);
  requestStartEl.addEventListener('click', requestStart);

  // Request initial data for the page and listen to updates.
  requestDataAndRegisterForUpdates();
}

// For JS eval.
Object.assign(window, {addAboutExpandListener, highlightIfChanged});

document.addEventListener('DOMContentLoaded', onLoad, false);
