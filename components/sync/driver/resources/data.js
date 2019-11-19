// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
const dumpToTextButton = $('dump-to-text');
const dataDump = $('data-dump');
dumpToTextButton.addEventListener('click', function(event) {
  // TODO(akalin): Add info like Chrome version, OS, date dumped, etc.

  let data = '';
  data += '======\n';
  data += 'Status\n';
  data += '======\n';
  data += JSON.stringify(chrome.sync.aboutInfo, null, 2);
  data += '\n';
  data += '\n';

  data += '=============\n';
  data += 'Notifications\n';
  data += '=============\n';
  data += JSON.stringify(chrome.sync.notifications, null, 2);
  data += '\n';
  data += '\n';

  data += '===\n';
  data += 'Log\n';
  data += '===\n';
  data += JSON.stringify(chrome.sync.log.entries, null, 2);
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
  'modelType',
  'SERVER_SPECIFICS',
  'SPECIFICS',
];

function versionToDateString(version) {
  // TODO(mmontgomery): ugly? Hacky? Is there a better way?
  const epochLength = Date.now().toString().length;
  const epochTime = parseInt(version.slice(0, epochLength), 10);
  const date = new Date(epochTime);
  return date.toString();
}

/**
 * @param {!Object} node A JavaScript represenation of a sync entity.
 * @return {!Array<string>} A string representation of the sync entity.
 */
function serializeNode(node) {
  return allFields.map(function(field) {
    let fieldVal;
    if (field == 'SERVER_VERSION_TIME') {
      const version = node['SERVER_VERSION'];
      if (version != null) {
        fieldVal = versionToDateString(version);
      }
    }
    if (field == 'BASE_VERSION_TIME') {
      const version = node['BASE_VERSION'];
      if (version != null) {
        fieldVal = versionToDateString(version);
      }
    } else if (
        (field == 'SERVER_SPECIFICS' || field == 'SPECIFICS') &&
        (!$('include-specifics').checked)) {
      fieldVal = 'REDACTED';
    } else if (
        (field == 'SERVER_SPECIFICS' || field == 'SPECIFICS') &&
        $('include-specifics').checked) {
      fieldVal = JSON.stringify(node[field]);
    } else {
      fieldVal = node[field];
    }
    return fieldVal;
  });
}

/**
 * @param {string} type The name of a sync model type.
 * @return {boolean} True if the type's checkbox is selected.
 */
function isSelectedDatatype(type) {
  const typeCheckbox = $(type);
  // Some types, such as 'Top level folder', appear in the list of nodes
  // but not in the list of selectable items.
  if (typeCheckbox == null) {
    return false;
  }
  return typeCheckbox.checked;
}

function makeBlobUrl(data) {
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
    'sync-data-dump', friendlyDate, Date.now(), navigator.platform
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
 * @param {!Array<{type: string, nodes: !Array<!Object>}>} nodesMap
 *     Summary of local state by model type.
 */
function triggerDataDownload(nodesMap) {
  // Prepend a header with ISO date and useragent.
  let output = [makeDateUserAgentHeader()];
  output.push('=====');

  const aboutInfo = JSON.stringify(chrome.sync.aboutInfo, null, 2);
  output.push(aboutInfo);

  // Filter out non-selected types.
  const selectedTypesNodes = nodesMap.filter(function(x) {
    return isSelectedDatatype(x.type);
  });

  // Serialize the remaining nodes and add them to the output.
  selectedTypesNodes.forEach(function(typeNodes) {
    output.push('=====');
    output.push(typeNodes.nodes.map(serializeNode).join('\n'));
  });

  output = output.join('\n');

  const anchor = $('dump-to-file-anchor');
  anchor.href = makeBlobUrl(output);
  anchor.download = makeDownloadName();
  anchor.click();
}

function createTypesCheckboxes(types) {
  const containerElt = $('node-type-checkboxes');

  types.map(function(type) {
    const div = document.createElement('div');

    const checkbox = document.createElement('input');
    checkbox.id = type;
    checkbox.type = 'checkbox';
    checkbox.checked = 'yes';
    div.appendChild(checkbox);

    const label = document.createElement('label');
    // Assigning to label.for doesn't work.
    label.setAttribute('for', type);
    label.innerText = type;
    div.appendChild(label);

    containerElt.appendChild(div);
  });
}

function onReceivedListOfTypes(e) {
  const types = e.details.types;
  types.sort();
  createTypesCheckboxes(types);
  chrome.sync.events.removeEventListener(
      'onReceivedListOfTypes',
      onReceivedListOfTypes);
}

function onReceivedIncludeSpecificsInitialState(e) {
  $('capture-specifics').checked = e.details.includeSpecifics;
}

document.addEventListener('DOMContentLoaded', function() {
  chrome.sync.events.addEventListener(
      'onReceivedListOfTypes',
      onReceivedListOfTypes);
  chrome.sync.requestListOfTypes();

  chrome.sync.events.addEventListener(
    'onReceivedIncludeSpecificsInitialState',
    onReceivedIncludeSpecificsInitialState);
  chrome.sync.requestIncludeSpecificsInitialState();
});

const dumpToFileLink = $('dump-to-file');
dumpToFileLink.addEventListener('click', function(event) {
  chrome.sync.getAllNodes(triggerDataDownload);
});
})();
