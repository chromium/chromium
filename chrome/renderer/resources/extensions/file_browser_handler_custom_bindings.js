// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the fileBrowserHandler API.

const fileBrowserNatives = requireNative('file_browser_handler');
const fileSystemHelpers = requireNative('file_system_natives');
const entryIdManager = require('entryIdManager');

const GetExternalFileEntry = fileBrowserNatives.GetExternalFileEntry;
const GetIsolatedFileSystem = fileSystemHelpers.GetIsolatedFileSystem;

/**
 * Adapter to get a FileEntry or DirectoryEntry from |item| passed from API
 * callback, via calls to GetExternalFileEntry() or get{Directory,File}().
 * Ideally we'd use Promise.allSettled() to process multiple files, but since
 * $Promise.allSettled() does not exist, so callbacks are used instead.
 * @param {function(!Entry): void} resolve Receiver for resulting Entry.
 * @param {function(string): void} reject Receiver for error message.
 * @param {boolean} canCreate For getFile() flow only: Whether to grant
 *   permission to create file if it's missing. Side effect: The file gets
 *   created if missing.
 * @param {!Object} item Key-value pair passed from API callback, containing
 *   data for GetExternalFileEntry() or get{Directory,File}() flows.
 */
function GetFileEntry(resolve, reject, canCreate, item) {
  if (item.hasOwnProperty('fileSystemName')) {
    // Legacy flow for Ash. Errors (such as nonexistent file) are not detected
    // here. These only arise downstream when the resulting Entry gets used.
    resolve(GetExternalFileEntry(item));
  } else if (item.hasOwnProperty('fileSystemId')) {
    // New flow for Lacros. Some errors (such as nonexistent file) are detected
    // here, and require handling.
    const fs = GetIsolatedFileSystem(item.fileSystemId);
    if (item.isDirectory) {
      fs.root.getDirectory(item.baseName, {}, (dirEntry) => {
        entryIdManager.registerEntry(item.entryId, dirEntry);
        resolve(dirEntry);
      }, (err) => {
        reject(err.message);
      });
    } else {
      fs.root.getFile(
          item.baseName, canCreate ? {create: true} : {},
          (fileEntry) => {
            entryIdManager.registerEntry(item.entryId, fileEntry);
            resolve(fileEntry);
          },
          (err) => {
            reject(err.message);
          });
    }
  } else {
    reject('Unknown file entry object.');
  }
}

bindingUtil.registerEventArgumentMassager('fileBrowserHandler.onExecute',
                                          function(args, dispatch) {
  if (args.length < 2) {
    dispatch(args);
    return;
  }
  // The second param for this event's payload is file definition dictionary.
  const fileList = args[1].entries;
  if (!fileList) {
    dispatch(args);
    return;
  }

  // Construct File API's Entry instances. $Promise.allSettled() is unavailable,
  // so use a |barrier| counter and explicitly sort results.
  const results = [];
  let barrier = fileList.length;
  const onFinish = () => {
    results.sort((a, b) => a.key - b.key);
    args[1].entries = $Array.map(results, item => item.entry);
    dispatch(args);
  };
  const onResolve = (index, entry) => {
    results.push({key: index, entry});
    if (--barrier === 0) onFinish();
  };
  const onReject = (message) => {
    console.error(message);
    if (--barrier === 0) onFinish();
  };
  for (let i = 0; i < fileList.length; ++i) {
    GetFileEntry(
        onResolve.bind(null, i), onReject,
        /*canCreate*/ false, /*item*/ fileList[i]);
  }
});
