// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the fileBrowserHandler API.

const fileBrowserNatives = requireNative('file_browser_handler');
const fileSystemHelpers = requireNative('file_system_natives');
const entryIdManager = require('entryIdManager');

const GetExternalFileEntry = fileBrowserNatives.GetExternalFileEntry;
const fileBrowserHandlerInternal = getInternalApi('fileBrowserHandlerInternal');
const GetIsolatedFileSystem = fileSystemHelpers.GetIsolatedFileSystem;

// Using Promise-like interface: We're avoiding Promise for safety, and $Promise
// does not support the desired feature (Promise.allSettled()).
function GetFileEntry(resolve, reject, item) {
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
      fs.root.getFile(item.baseName, {}, (fileEntry) => {
        entryIdManager.registerEntry(item.entryId, fileEntry);
        resolve(fileEntry);
      }, (err) => {
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
    GetFileEntry(onResolve.bind(null, i), onReject, fileList[i]);
  }
});

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('selectFile',
                                function(selectionParams, callback) {
    function internalCallback(externalCallback, internalResult) {
      if (!externalCallback)
        return;
      var result = undefined;
      if (internalResult) {
        result = { success: internalResult.success, entry: null };
        if (internalResult.success)
          result.entry = GetExternalFileEntry(internalResult.entry);
      }

      externalCallback(result);
    }

    return fileBrowserHandlerInternal.selectFile(
        selectionParams, $Function.bind(internalCallback, null, callback));
  });
});
