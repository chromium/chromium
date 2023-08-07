// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// A 1x1 transparent GIF in 42 bytes.
const GIF_DATA = new Uint8Array([
  71, 73, 70,  56,  57,  97, 1,   0, 1, 0, 128, 0,  0, 0,
  0,  0,  255, 255, 255, 33, 249, 4, 1, 0, 0,   0,  0, 44,
  0,  0,  0,   0,   1,   0,  1,   0, 0, 2, 1,   68, 0, 59
]);
const GIF_FILE = new File([GIF_DATA], 'readwrite.gif', {type: 'image/gif'});

// A 1x1 transparent PNG in 95 bytes.
const PNG_DATA = new Uint8Array([
  137, 80,  78,  71, 13,  10, 26,  10,  0,  0,  0,   13, 73, 72,  68,
  82,  0,   0,   0,  1,   0,  0,   0,   1,  1,  3,   0,  0,  0,   37,
  219, 86,  202, 0,  0,   0,  3,   80,  76, 84, 69,  0,  0,  0,   167,
  122, 61,  218, 0,  0,   0,  1,   116, 82, 78, 83,  0,  64, 230, 216,
  102, 0,   0,   0,  10,  73, 68,  65,  84, 8,  215, 99, 96, 0,   0,
  0,   2,   0,   1,  226, 33, 188, 51,  0,  0,  0,   0,  73, 69,  78,
  68,  174, 66,  96, 130
]);
const PNG_FILE = new File([PNG_DATA], 'readonly.png', {type: 'image/png'});

const TXT_FILE = new File(['txt_data'], 'readonly.txt', {type: 'text/plain'});

const PROVIDER_NAME = "provided-file-system-provider";

const GIF_ENTRY = Object.freeze({
  isDirectory: false,
  name: GIF_FILE.name,
  size: GIF_FILE.size,
  modificationTime: new Date(),
  mimeType: GIF_FILE.type,
  file: GIF_FILE,
  writable: true,
  cloudIdentifier: {
    providerName: PROVIDER_NAME,
    id: "readwrite-gif-id"
  }
});
const PNG_ENTRY = Object.freeze({
  isDirectory: false,
  name: PNG_FILE.name,
  size: PNG_FILE.size,
  modificationTime: new Date(),
  mimeType: PNG_FILE.type,
  file: PNG_FILE,
  writable: false
  // does not have `cloudIdentifier` on purpose
});
const TXT_ENTRY = Object.freeze({
  isDirectory: false,
  name: TXT_FILE.name,
  size: TXT_FILE.size,
  modificationTime: new Date(),
  mimeType: TXT_FILE.type,
  file: TXT_FILE,
  writable: false,
  cloudIdentifier: {
    providerName: PROVIDER_NAME,
    id: "readonly-txt-id"
  }
});
const ROOT_ENTRY = Object.freeze({
  isDirectory: true,
  name: '',
  size: 0,
  modificationTime: new Date(),
  mimeType: 'text/directory',
  cloudIdentifier: {
    providerName: PROVIDER_NAME,
    id: "root-id"
  }
});

const ENTRY_PATHS = {
  ['/']: ROOT_ENTRY,
  [`/${GIF_ENTRY.name}`]: GIF_ENTRY,
  [`/${PNG_ENTRY.name}`]: PNG_ENTRY,
  [`/${TXT_ENTRY.name}`]: TXT_ENTRY,
};

const METADATA_FIELD_NAMES = [
  'name',
  'mimeType',
  'modificationTime',
  'isDirectory',
  'size',
  'cloudIdentifier'
];

// A mapping from |requestId| to file entry. Used to respond to subsequent file
// read requests.
let requestIdToFileEntry = new Map();

function trace(...args) {
  console.log(...args);
}

function mountFileSystem() {
  chrome.fileSystemProvider.mount({
    fileSystemId: 'test-image-provider-fs',
    displayName: 'Test Image Provider FS'
  });
}

/** Copies and adjusts `entryTemplate` to suit the request in `options`. */
function makeEntry(entryTemplate, options) {
  const entry = {};
  for (const prop of METADATA_FIELD_NAMES) {
    if (options[prop]) {
      entry[prop] = entryTemplate[prop]
    }
  }
  return entry;
}

/** Find an entry. Invokes `onError('NOT_FOUND')` if `entry` is unknown. */
function findEntry(entryPath, onError, options, operation) {
  trace(operation, entryPath, JSON.stringify(options));
  const entry = ENTRY_PATHS[entryPath];
  if (!entry) {
    console.log(
        `Request for '${entryPath}': NOT_FOUND. ${JSON.stringify(options)}`);
    onError('NOT_FOUND');
  }
  return entry;
}

chrome.fileSystemProvider.onGetMetadataRequested.addListener(function(
    options, onSuccess, onError) {
  let entry = findEntry(options.entryPath, onError, options, 'metadata');
  if (entry) {
    onSuccess(makeEntry(entry, options));
  }
});

chrome.fileSystemProvider.onOpenFileRequested.addListener(function(
    options, onSuccess, onError) {
  const entry = findEntry(options.filePath, onError, options, 'open');
  if (entry) {
    if (options.mode === 'WRITE' && !entry.writable) {
      return onError('ACCESS_DENIED');
    }

    requestIdToFileEntry.set(options.requestId, entry);
    trace('open-success', options.requestId, entry.name);
    onSuccess();
  }
});

chrome.fileSystemProvider.onCloseFileRequested.addListener(function(
    options, onSuccess, onError) {
  trace('close-file', options);
  requestIdToFileEntry.delete(options.openRequestId);
  onSuccess();
  trace('close-success', options.requestId);
});

chrome.fileSystemProvider.onReadFileRequested.addListener(function(
    options, onSuccess, onError) {
  trace('read-file', options.requestId);
  const fileEntry = requestIdToFileEntry.get(options.openRequestId);
  if (!fileEntry) {
    onError("INVALID_OPERATION");
  }

  fileEntry.file.arrayBuffer().then(arrayBuffer => {
    onSuccess(arrayBuffer, /* hasMore */ false);
    trace('read-success', fileEntry.name);
  });
});

chrome.fileSystemProvider.onReadDirectoryRequested.addListener(function(
    options, onSuccess, onError) {
  trace('open, ', options);
  // For anything other than root, return no entries.
  if (options.directoryPath !== '/') {
    onSuccess([], false /* hasMore */);
    return;
  }
  const entries = [
    makeEntry(GIF_ENTRY, options),
    makeEntry(PNG_ENTRY, options)
  ];
  onSuccess(entries, false /* hasMore */);
});

chrome.fileSystemProvider.onUnmountRequested.addListener(function(
    options, onSuccess, onError) {
  trace('unmount', options);
  chrome.fileSystemProvider.unmount(
      {fileSystemId: options.fileSystemId}, onSuccess)
});

chrome.fileSystemProvider.onGetActionsRequested.addListener(function(
    options, onSuccess, onError) {
  trace('actions-requested', options);
  onSuccess([]);
});

chrome.fileSystemProvider.onWriteFileRequested.addListener(function(
    options, onSuccess, onError) {
  trace('write-file', options.openRequestId);

  const fileEntry = requestIdToFileEntry.get(options.openRequestId);
  if (!fileEntry) {
    onError("INVALID_OPERATION");
  }

  // For now, no need to update the actual file content.
  onSuccess();
});

chrome.fileSystemProvider.onDeleteEntryRequested.addListener(function(
    options, onSuccess, onError) {
  trace('delete-entry', options.entryPath);

  // Don't support deleting.
  onError('ACCESS_DENIED');
});

// Hook onInstalled rather than onLaunched so it appears immediately.
chrome.runtime.onInstalled.addListener(mountFileSystem);
