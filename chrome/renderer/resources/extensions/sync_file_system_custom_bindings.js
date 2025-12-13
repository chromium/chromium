// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the syncFileSystem API.

const fileSystemNatives = requireNative('file_system_natives');
const syncFileSystemNatives = requireNative('sync_file_system');

apiBridge.registerCustomHook(function(bindingsAPI) {
  const apiFunctions = bindingsAPI.apiFunctions;

  // Functions which take in an [instanceOf=FileEntry].
  function bindFileEntryFunction(functionName) {
    apiFunctions.setUpdateArgumentsPostValidate(
        functionName, function(entry, callback) {
          const fileSystemUrl = entry.toURL();
          return [fileSystemUrl, callback];
        });
  }
  $Array.forEach(['getFileStatus'], bindFileEntryFunction);

  // Functions which take in a FileEntry array.
  function bindFileEntryArrayFunction(functionName) {
    apiFunctions.setUpdateArgumentsPostValidate(
        functionName, function(entries, callback) {
          const fileSystemUrlArray = [];
          for (let i = 0; i < entries.length; i++) {
            $Array.push(fileSystemUrlArray, entries[i].toURL());
          }
          return [fileSystemUrlArray, callback];
        });
  }
  $Array.forEach(['getFileStatuses'], bindFileEntryArrayFunction);

  // Functions which take in an [instanceOf=DOMFileSystem].
  function bindFileSystemFunction(functionName) {
    apiFunctions.setUpdateArgumentsPostValidate(
        functionName, function(filesystem, callback) {
          const fileSystemUrl = filesystem.root.toURL();
          return [fileSystemUrl, callback];
        });
  }
  $Array.forEach(['getUsageAndQuota'], bindFileSystemFunction);

  // Functions which return an [instanceOf=DOMFileSystem].
  apiFunctions.setCustomCallback(
      'requestFileSystem', function(callback, response) {
        let result = null;
        if (response) {
          result = syncFileSystemNatives.GetSyncFileSystemObject(
              response.name, response.root);
        }
        if (callback) {
          callback(result);
        }
      });

  // Functions which return an array of FileStatusInfo object
  // which has [instanceOf=FileEntry].
  apiFunctions.setCustomCallback(
      'getFileStatuses', function(callback, response) {
        const results = [];
        if (response) {
          for (let i = 0; i < response.length; i++) {
            const result = {};
            const entry = response[i].entry;
            result.fileEntry = fileSystemNatives.GetFileEntry(
                entry.fileSystemType, entry.fileSystemName, entry.rootUrl,
                entry.filePath, entry.isDirectory);
            result.status = response[i].status;
            result.error = response[i].error;
            $Array.push(results, result);
          }
        }
        if (callback) {
          callback(results);
        }
      });
});

bindingUtil.registerEventArgumentMassager(
    'syncFileSystem.onFileStatusChanged', function(args, dispatch) {
      // Make FileEntry object using all the base string fields.
      const fileEntry = fileSystemNatives.GetFileEntry(
          args[0].fileSystemType, args[0].fileSystemName, args[0].rootUrl,
          args[0].filePath, args[0].isDirectory);

      // Combine into a single dictionary.
      const fileInfo = new Object();
      fileInfo.fileEntry = fileEntry;
      fileInfo.status = args[1];
      if (fileInfo.status === 'synced') {
        fileInfo.action = args[2];
        fileInfo.direction = args[3];
      }
      dispatch([fileInfo]);
    });
