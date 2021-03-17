// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';

import {addWebUIListener, removeWebUIListener} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

import {requestDataAndRegisterForUpdates, requestStart, requestStopClearData, requestStopKeepData, setIncludeSpecifics, triggerRefresh} from './chrome_sync.js';

// Contains the latest snapshot of sync about info.
export let aboutInfo = {};

// For tests
window.getAboutInfoForTest = function() {
  return aboutInfo;
};

let aboutInfoListener = null;
let entityCountsUpdatedListener = null;

function highlightIfChanged(node, oldVal, newVal) {
  function clearHighlight() {
    this.removeAttribute('highlighted');
  }

  const oldStr = oldVal.toString();
  const newStr = newVal.toString();
  if (oldStr !== '' && oldStr !== newStr) {
    // Note the addListener function does not end up creating duplicate
    // listeners.  There can be only one listener per event at a time.
    // Reference: https://developer.mozilla.org/en/DOM/element.addEventListener
    node.addEventListener('webkitAnimationEnd', clearHighlight, false);
    node.setAttribute('highlighted', '');
  }
}

function refreshAboutInfo(newAboutInfo) {
  aboutInfo = newAboutInfo;
  const aboutInfoDiv = $('about-info');
  jstProcess(new JsEvalContext(aboutInfo), aboutInfoDiv);
}

function onEntityCountsUpdatedEvent(response) {
  if (!aboutInfo.type_status) {
    return;
  }
  for (const count of response.entityCounts) {
    const typeStatusRow =
        aboutInfo.type_status.find(row => row.name === count.modelType);
    if (typeStatusRow) {
      typeStatusRow.num_entries = count.entities;
      typeStatusRow.num_live = count.nonTombstoneEntities;
    }
  }
  jstProcess(
      new JsEvalContext({type_status: aboutInfo.type_status}), $('typeInfo'));
}

/**
 * Helper to determine if an element is scrolled to its bottom limit.
 * @param {Element} elem element to check
 * @return {boolean} true if the element is scrolled to the bottom
 */
function isScrolledToBottom(elem) {
  return elem.scrollHeight - elem.scrollTop === elem.clientHeight;
}

/**
 * Helper to scroll an element to its bottom limit.
 * @param {Element} elem element to be scrolled
 */
function scrollToBottom(elem) {
  elem.scrollTop = elem.scrollHeight - elem.clientHeight;
}

/** Container for accumulated sync protocol events. */
const protocolEvents = [];

/** We may receive re-delivered events.  Keep a record of ones we've seen. */
const knownEventTimestamps = {};

/**
 * Callback for incoming protocol events.
 * @param {Object} response The protocol event response.
 */
function onReceivedProtocolEvent(response) {
  // Return early if we've seen this event before.  Assumes that timestamps
  // are sufficiently high resolution to uniquely identify an event.
  if (knownEventTimestamps.hasOwnProperty(response.time)) {
    return;
  }

  knownEventTimestamps[response.time] = true;
  protocolEvents.push(response);

  const trafficContainer = $('traffic-event-container');

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
  const includeSpecificsCheckbox = $('capture-specifics');
  includeSpecificsCheckbox.addEventListener('change', function(event) {
    setIncludeSpecifics(includeSpecificsCheckbox.checked);
  });

  addWebUIListener('onProtocolEvent', onReceivedProtocolEvent);

  // Make the prototype jscontent element disappear.
  jstProcess({}, $('traffic-event-container'));

  const triggerRefreshButton = $('trigger-refresh');
  triggerRefreshButton.addEventListener('click', function(event) {
    triggerRefresh();
  });
}

/**
 * Initializes listeners for status dump and import UI.
 */
function initStatusDumpButton() {
  $('status-data').hidden = true;

  const dumpStatusButton = $('dump-status');
  dumpStatusButton.addEventListener('click', function(event) {
    const aboutInfoCopy = aboutInfo;
    if (!$('include-ids').checked) {
      aboutInfoCopy.details = aboutInfo.details.filter(function(el) {
        return !el.is_sensitive;
      });
    }
    let data = '';
    data += new Date().toString() + '\n';
    data += '======\n';
    data += 'Status\n';
    data += '======\n';
    data += JSON.stringify(aboutInfoCopy, null, 2) + '\n';

    $('status-text').value = data;
    $('status-data').hidden = false;
  });

  const importStatusButton = $('import-status');
  importStatusButton.addEventListener('click', function(event) {
    $('status-data').hidden = false;
    if ($('status-text').value.length === 0) {
      $('status-text').value = 'Paste sync status dump here then click import.';
      return;
    }

    // First remove any characters before the '{'.
    let data = $('status-text').value;
    const firstBrace = data.indexOf('{');
    if (firstBrace < 0) {
      $('status-text').value = 'Invalid sync status dump.';
      return;
    }
    data = data.substr(firstBrace);

    // Remove listeners to prevent sync events from overwriting imported data.
    if (aboutInfoListener) {
      removeWebUIListener(aboutInfoListener);
      aboutInfoListener = null;
    }

    if (entityCountsUpdatedListener) {
      removeWebUIListener(entityCountsUpdatedListener);
      entityCountsUpdatedListener = null;
    }

    const aboutInfo = JSON.parse(data);
    refreshAboutInfo(aboutInfo);
  });
}

/**
 * Toggles the given traffic event entry div's "expanded" state.
 * @param {MouseEvent} e the click event that triggered the toggle.
 */
function expandListener(e) {
  if (e.target.classList.contains('proto')) {
    // We ignore proto clicks to keep it copyable.
    return;
  }
  let trafficEventDiv = e.target;
  // Click might be on div's child.
  if (trafficEventDiv.nodeName !== 'DIV') {
    trafficEventDiv = trafficEventDiv.parentNode;
  }
  trafficEventDiv.classList.toggle('traffic-event-entry-expanded');
}

/**
 * Attaches a listener to the given traffic event entry div.
 * @param {HTMLElement} element the element to attach the listener to.
 */
function addAboutExpandListener(element) {
  element.addEventListener('click', expandListener, false);
}

function onLoad() {
  initStatusDumpButton();
  initProtocolEventLog();

  aboutInfoListener = addWebUIListener('onAboutInfoUpdated', refreshAboutInfo);

  entityCountsUpdatedListener =
      addWebUIListener('onEntityCountsUpdated', onEntityCountsUpdatedEvent);

  $('request-start').addEventListener('click', function(event) {
    requestStart();
  });
  $('request-stop-keep-data').addEventListener('click', function(event) {
    requestStopKeepData();
  });
  $('request-stop-clear-data').addEventListener('click', function(event) {
    requestStopClearData();
  });

  // Request initial data for the page and listen to updates.
  requestDataAndRegisterForUpdates();
}

// For JS eval.
window.addAboutExpandListener = addAboutExpandListener;
window.highlightIfChanged = highlightIfChanged;

document.addEventListener('DOMContentLoaded', onLoad, false);
