// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import type {WebUiListener} from 'chrome://resources/js/cr.js';
import {addWebUiListener, removeWebUiListener} from 'chrome://resources/js/cr.js';

import {aboutInfo} from './about.js';
import type {SyncNode, SyncNodeMap} from './chrome_sync.js';
import {getAllNodes, requestIncludeSpecificsInitialState, requestListOfTypes} from './chrome_sync.js';
import {log} from './sync_log.js';

const dumpToTextButton = document.querySelector<HTMLElement>('#dump-to-text');
assert(dumpToTextButton);
const dataDump = document.querySelector<HTMLElement>('#data-dump');
assert(dataDump);
dumpToTextButton.addEventListener('click', function() {
  // TODO(akalin): Add info like Chrome version, OS, date dumped, etc.

  let data = '';
  data += '======\n';
  data += 'Status\n';
  data += '======\n';
  data += JSON.stringify(aboutInfo, null, 2);
  data += '\n';
  data += '\n';

  data += '===\n';
  data += 'Log\n';
  data += '===\n';
  data += JSON.stringify(log.entries, null, 2);
  data += '\n';

  dataDump.textContent = data;
});

const allFields = [
  'ID',
  'IS_UNSYNCED',
  'IS_UNAPPLIED_UPDATE',
  'BASE_VERSION',
  'BASE_VERSION_TIME',
  'SERVER_VERSION',
  'SERVER_VERSION_TIME',
  'PARENT_ID',
  'SERVER_PARENT_ID',
  'IS_DEL',
  'SERVER_IS_DEL',
  'dataType',
  'SERVER_SPECIFICS',
  'SPECIFICS',
];

function versionToDateString(version: string) {
  // TODO(mmontgomery): ugly? Hacky? Is there a better way?
  const epochLength = Date.now().toString().length;
  const epochTime = parseInt(version.slice(0, epochLength), 10);
  const date = new Date(epochTime);
  return date.toString();
}

/**
 * @param node A JavaScript represenation of a sync entity.
 * @return A string representation of the sync entity.
 */
function serializeNode(node: SyncNode): string[] {
  const includeSpecifics =
      document.querySelector<HTMLInputElement>('#include-specifics');
  assert(includeSpecifics);
  return allFields.map(function(field) {
    let fieldVal;
    if (field === 'SERVER_VERSION_TIME') {
      const version = node['SERVER_VERSION'];
      if (version != null) {
        fieldVal = versionToDateString(version);
      }
    }
    if (field === 'BASE_VERSION_TIME') {
      const version = node['BASE_VERSION'];
      if (version != null) {
        fieldVal = versionToDateString(version);
      }
    } else if (
        (field === 'SERVER_SPECIFICS' || field === 'SPECIFICS') &&
        (!includeSpecifics.checked)) {
      fieldVal = 'REDACTED';
    } else if (
        (field === 'SERVER_SPECIFICS' || field === 'SPECIFICS') &&
        includeSpecifics.checked) {
      fieldVal = JSON.stringify(node[field]);
    } else {
      fieldVal = (node as unknown as {[key: string]: string})[field];
    }
    return fieldVal || '';
  });
}

/**
 * @param type The name of a sync data type.
 * @return True if the type's checkbox is selected.
 */
function isSelectedDatatype(type: string): boolean {
  const typeCheckbox = document.querySelector<HTMLInputElement>(`#${type}`);
  // Some types, such as 'Top level folder', appear in the list of nodes
  // but not in the list of selectable items.
  if (typeCheckbox == null) {
    return false;
  }
  return typeCheckbox.checked;
}

function makeBlobUrl(data: string): string {
  const textBlob = new Blob([data], {type: 'octet/stream'});
  const blobUrl = window.URL.createObjectURL(textBlob);
  return blobUrl;
}

function makeDownloadName() {
  // Format is sync-data-dump-$epoch-$year-$month-$day-$OS.csv.
  const now = new Date();
  const friendlyDate =
      [now.getFullYear(), now.getMonth() + 1, now.getDate()].join('-');
  const name = [
    'sync-data-dump',
    friendlyDate,
    Date.now(),
    navigator.platform,
  ].join('-');
  return [name, 'csv'].join('.');
}

function makeDateUserAgentHeader() {
  const now = new Date();
  const userAgent = window.navigator.userAgent;
  const dateUaHeader = [now.toISOString(), userAgent].join(',');
  return dateUaHeader;
}

/**
 * Builds a summary of current state and exports it as a downloaded file.
 *
 * @param nodesMap Summary of local state by data type.
 */
function triggerDataDownload(nodesMap: SyncNodeMap) {
  // Prepend a header with ISO date and useragent.
  const output = [makeDateUserAgentHeader()];
  output.push('=====');

  const aboutInfoString = JSON.stringify(aboutInfo, null, 2);
  output.push(aboutInfoString);

  // Filter out non-selected types.
  const selectedTypesNodes = nodesMap.filter(function(x) {
    return isSelectedDatatype(x.type);
  });

  // Serialize the remaining nodes and add them to the output.
  selectedTypesNodes.forEach(function(typeNodes) {
    output.push('=====');
    output.push(typeNodes.nodes.map(serializeNode).join('\n'));
  });

  const outputString = output.join('\n');

  const anchor =
      document.querySelector<HTMLAnchorElement>('#dump-to-file-anchor');
  assert(anchor);
  anchor.href = makeBlobUrl(outputString);
  anchor.download = makeDownloadName();
  anchor.click();
}

function createTypesCheckboxes(types: string[]) {
  const containerElt =
      document.querySelector<HTMLElement>('#node-type-checkboxes');
  assert(containerElt);

  types.map(function(type: string) {
    const div = document.createElement('div');

    const checkbox = document.createElement('input');
    checkbox.id = type;
    checkbox.type = 'checkbox';
    checkbox.checked = true;
    div.appendChild(checkbox);

    const label = document.createElement('label');
    // Assigning to label.for doesn't work.
    label.setAttribute('for', type);
    label.innerText = type;
    div.appendChild(label);

    containerElt.appendChild(div);
  });
}

let listOfTypesListener: WebUiListener|null = null;

function onReceivedListOfTypes(response: {types: string[]}) {
  const types = response.types;
  types.sort();
  createTypesCheckboxes(types);
  assert(listOfTypesListener);
  removeWebUiListener(listOfTypesListener);
}

function onReceivedIncludeSpecificsInitialState(
    response: {includeSpecifics: boolean}) {
  const captureSpecifics =
      document.querySelector<HTMLInputElement>('#capture-specifics');
  assert(captureSpecifics);
  captureSpecifics.checked = response.includeSpecifics;
}

document.addEventListener('DOMContentLoaded', function() {
  listOfTypesListener =
      addWebUiListener('onReceivedListOfTypes', onReceivedListOfTypes);
  requestListOfTypes();

  addWebUiListener(
      'onReceivedIncludeSpecificsInitialState',
      onReceivedIncludeSpecificsInitialState);
  requestIncludeSpecificsInitialState();
});

const dumpToFileLink = document.querySelector<HTMLElement>('#dump-to-file');
assert(dumpToFileLink);
dumpToFileLink.addEventListener('click', function() {
  getAllNodes(triggerDataDownload);
});
