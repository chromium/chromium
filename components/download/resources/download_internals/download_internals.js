// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

import {DownloadInternalsBrowserProxy, DownloadInternalsBrowserProxyImpl, ServiceEntry, ServiceEntryState, ServiceRequest, ServiceStatus} from './download_internals_browser_proxy.js';

/** @type {!DownloadInternalsBrowserProxy} */
const browserProxy = DownloadInternalsBrowserProxyImpl.getInstance();

/** @type {!Array<ServiceEntry>} */
const ongoingServiceEntries = [];

/** @type {!Array<ServiceEntry>} */
const finishedServiceEntries = [];

/** @type {!Array<ServiceRequest>} */
const serviceRequests = [];

/**
 * @param {!Array<ServiceEntry>} list A list to remove the entry from.
 * @param {string} guid The guid to remove from the list.
 */
function removeGuidFromList(list, guid) {
  const index = list.findIndex(entry => entry.guid == guid);
  if (index != -1) {
    list.splice(index, 1);
  }
}

/**
 * Replaces the ServiceEntry specified by guid in the list or, if it's not
 * found, adds a new entry.
 * @param {!Array<ServiceEntry>} list A list to update.
 * @param {!ServiceEntry} newEntry The new entry.
 */
function addOrUpdateEntryByGuid(list, newEntry) {
  const index = list.findIndex(entry => entry.guid == newEntry.guid);
  if (index != -1) {
    list[index] = newEntry;
  } else {
    list.unshift(newEntry);
  }
}

function updateEntryTables() {
  const ongoingInput = new JsEvalContext({entries: ongoingServiceEntries});
  jstProcess(ongoingInput, $('download-service-ongoing-entries-info'));

  const finishedInput = new JsEvalContext({entries: finishedServiceEntries});
  jstProcess(finishedInput, $('download-service-finished-entries-info'));
}

/**
 * @param {!ServiceStatus} state The current status of the download service.
 */
function onServiceStatusChanged(state) {
  $('service-state').textContent = state.serviceState;
  $('service-status-model').textContent = state.modelStatus;
  $('service-status-driver').textContent = state.driverStatus;
  $('service-status-file').textContent = state.fileMonitorStatus;
}

/**
 * @param {!Array<!ServiceEntry>} entries A list entries currently tracked by
 *     the download service.
 */
function onServiceDownloadsAvailable(entries) {
  for (let i = 0; i < entries.length; i++) {
    const entry = entries[i];
    if (entry.state == ServiceEntryState.COMPLETE) {
      finishedServiceEntries.unshift(entry);
    } else {
      ongoingServiceEntries.unshift(entry);
    }
  }

  updateEntryTables();
}

/**
 * @param {!ServiceEntry} entry The new state for a particular download
 *     service entry.
 */
function onServiceDownloadChanged(entry) {
  if (entry.state == ServiceEntryState.COMPLETE) {
    removeGuidFromList(ongoingServiceEntries, entry.guid);
    addOrUpdateEntryByGuid(finishedServiceEntries, entry);
  } else {
    addOrUpdateEntryByGuid(ongoingServiceEntries, entry);
  }

  updateEntryTables();
}

/**
 * @param {!ServiceEntry} entry The new state for a failed download service
 *     entry.
 */
function onServiceDownloadFailed(entry) {
  removeGuidFromList(ongoingServiceEntries, entry.guid);
  addOrUpdateEntryByGuid(finishedServiceEntries, entry);

  updateEntryTables();
}

/**
 * @param {!ServiceRequest} request The state for a newly issued download
 *     service request.
 */
function onServiceRequestMade(request) {
  serviceRequests.unshift(request);
  const input = new JsEvalContext({requests: serviceRequests});
  jstProcess(input, $('download-service-request-info'));
}

function initialize() {
  // Register all event listeners.
  addWebUIListener('service-status-changed', onServiceStatusChanged);
  addWebUIListener('service-downloads-available', onServiceDownloadsAvailable);
  addWebUIListener('service-download-changed', onServiceDownloadChanged);
  addWebUIListener('service-download-failed', onServiceDownloadFailed);
  addWebUIListener('service-request-made', onServiceRequestMade);

  $('start-download').onclick = function() {
    browserProxy.startDownload($('download-url').value);
  };

  // Kick off requests for the current system state.
  browserProxy.getServiceStatus().then(onServiceStatusChanged);
  browserProxy.getServiceDownloads().then(onServiceDownloadsAvailable);
}

document.addEventListener('DOMContentLoaded', initialize);
