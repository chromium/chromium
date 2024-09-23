// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the fileManagerPrivate API.

// Natives
var blobNatives = requireNative('blob_natives');
var fileManagerPrivateNatives = requireNative('file_manager_private');
var logging = requireNative('logging');

// Internals
var fileManagerPrivateInternal = getInternalApi('fileManagerPrivateInternal');

// Wrapper that ensures only that a single parameter is passed to the function
// so it can be used with Array.map.
function getExternalFileEntry(entry) {
  return fileManagerPrivateNatives.GetExternalFileEntry(entry);
}

// Adaptor to help propagating errors emitted by calls to internal API
// implementations.
function callbackAdaptor(successCallback, failureCallback, resultHandler) {
  return function(...results) {
    // No callback should take more than one result.
    logging.CHECK(results.length <= 1);
    let lastErrorMessage = bindingUtil.getLastErrorMessage();
    if (lastErrorMessage) {
      // We re-emit |lastError| through the |failureCallback| here to ensure it
      // gets correctly propagated to the top level caller either as a rejected
      // promise or |lastError| for a callback. We also clear it to ensure this
      // instance of the |lastError| isn't reported as unchecked.
      bindingUtil.clearLastError();
      failureCallback(lastErrorMessage);
      return;
    }
    // Invoke the success callback, optionally handling the result first.
    // Note that we ensure we call |successCallback| with the expected number of
    // arguments (as opposed to just calling with undefined if result are empty)
    // so that any callers using `arguments` are unaffected.
    if (resultHandler) {
      // If a function has a result handler, it must have a result.
      logging.CHECK(results.length == 1);
      let finalResult = resultHandler(results[0]);
      successCallback(finalResult);
    } else if (results.length == 1) {
      successCallback(results[0]);
    } else {
      successCallback();
    }
  }
}

apiBridge.registerCustomHook(function(bindingsAPI) {
  // For FilesAppEntry types that wraps a native entry, returns the native entry
  // to be able to send to fileManagerPrivate API.
  function getEntryURL(entry) {
    const nativeEntry = entry.getNativeEntry && entry.getNativeEntry();
    if (nativeEntry) {
      entry = nativeEntry;
    }
    return fileManagerPrivateNatives.GetEntryURL(entry);
  }

  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback('searchDrive', function(callback, response) {
    if (response && !response.error && response.entries) {
      response.entries = response.entries.map(getExternalFileEntry);
    }

    // So |callback| doesn't break if response is not defined.
    if (!response) {
      response = {};
    }

    if (callback) {
      callback({entries: response.entries, nextFeed: response.nextFeed});
    }
  });

  apiFunctions.setCustomCallback('searchDriveMetadata',
      function(callback, response) {
    if (response && !response.error) {
      for (var i = 0; i < response.length; i++) {
        response[i].entry =
            getExternalFileEntry(response[i].entry);
      }
    }

    // So |callback| doesn't break if response is not defined.
    if (!response) {
      response = [];
    }

    if (callback) {
      callback(response);
    }
  });

  apiFunctions.setHandleRequest(
      'resolveIsolatedEntries',
      function(entries, successCallback, failureCallback) {
        var urls = entries.map(getEntryURL);
        let resultHandler = function(entryDescriptions) {
          return entryDescriptions.map(getExternalFileEntry);
        };
        fileManagerPrivateInternal.resolveIsolatedEntries(
            urls,
            callbackAdaptor(successCallback, failureCallback, resultHandler));
      });

  apiFunctions.setHandleRequest(
      'getVolumeRoot', function(options, successCallback, failureCallback) {
        let resultHandler = function(entry) {
          return entry ? getExternalFileEntry(entry) : undefined;
        };
        fileManagerPrivateInternal.getVolumeRoot(
            options,
            callbackAdaptor(successCallback, failureCallback, resultHandler));
      });

  apiFunctions.setHandleRequest(
      'getEntryProperties',
      function(entries, names, successCallback, failureCallback) {
        var urls = entries.map(getEntryURL);
        fileManagerPrivateInternal.getEntryProperties(
            urls, names, callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'addFileWatch', function(entry, successCallback, failureCallback) {
        var url = getEntryURL(entry);
        fileManagerPrivateInternal.addFileWatch(
            url, callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'removeFileWatch', function(entry, successCallback, failureCallback) {
        var url = getEntryURL(entry);
        fileManagerPrivateInternal.removeFileWatch(
            url, callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'getCustomActions', function(entries, successCallback, failureCallback) {
        var urls = entries.map(getEntryURL);
        fileManagerPrivateInternal.getCustomActions(
            urls, callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'executeCustomAction',
      function(entries, actionId, successCallback, failureCallback) {
        var urls = entries.map(getEntryURL);
        fileManagerPrivateInternal.executeCustomAction(
            urls, actionId, callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'searchFiles', function(params, successCallback, failureCallback) {
        const newParams = {
          query: params.query,
          types: params.types,
          maxResults: params.maxResults,
          modifiedTimestamp: params.modifiedTimestamp || 0,
          category:
              params.category || chrome.fileManagerPrivate.FileCategory.ALL
        };
        if (params.rootDir) {
          newParams.rootUrl = getEntryURL(params.rootDir);
        }
        let resultHandler = function(entryList) {
          return (entryList || []).map(getExternalFileEntry);
        };
        fileManagerPrivateInternal.searchFiles(
            newParams,
            callbackAdaptor(successCallback, failureCallback, resultHandler));
      });

  apiFunctions.setHandleRequest('getContentMimeType',
      function(fileEntry, successCallback, failureCallback) {
    fileEntry.file(blob => {
      var blobUUID = blobNatives.GetBlobUuid(blob);

      if (!blob || !blob.size) {
        successCallback(undefined);
        return;
      }

      var resultHandler = function(blob, mimeType) {
        return mimeType;
      }.bind(this, blob);  // Bind a blob reference: crbug.com/415792#c12

      fileManagerPrivateInternal.getContentMimeType(
          blobUUID,
          callbackAdaptor(successCallback, failureCallback, resultHandler));
    }, (error) => {
      failureCallback(`fileEntry.file() blob error: ${error.message}`);
    });
  });

  apiFunctions.setHandleRequest('getContentMetadata', function(
      fileEntry, mimeType, includeImages, successCallback, failureCallback) {
    fileEntry.file(blob => {
      var blobUUID = blobNatives.GetBlobUuid(blob);

      if (!blob || !blob.size) {
        successCallback(undefined);
        return;
      }

      var resultHandler = function(blob, metadata) {
        return metadata;
      }.bind(this, blob);  // Bind a blob reference: crbug.com/415792#c12

      fileManagerPrivateInternal.getContentMetadata(
          blobUUID, mimeType, !!includeImages,
          callbackAdaptor(successCallback, failureCallback, resultHandler));
    }, (error) => {
      failureCallback(`fileEntry.file() blob error: ${error.message}`);
    });
  });

  apiFunctions.setHandleRequest(
      'pinDriveFile', function(entry, pin, successCallback, failureCallback) {
        var url = getEntryURL(entry);
        fileManagerPrivateInternal.pinDriveFile(
            url, pin, callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'executeTask',
      function(descriptor, entries, successCallback, failureCallback) {
        var urls = entries.map(getEntryURL);
        fileManagerPrivateInternal.executeTask(
            descriptor, urls,
            callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'setDefaultTask',
      function(
          descriptor, entries, mimeTypes, successCallback, failureCallback) {
        var urls = entries.map(getEntryURL);
        fileManagerPrivateInternal.setDefaultTask(
            descriptor, urls, mimeTypes,
            callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'getFileTasks',
      function(entries, dlpSourceUrls, successCallback, failureCallback) {
        var urls = entries.map(getEntryURL);
        fileManagerPrivateInternal.getFileTasks(
            urls, dlpSourceUrls,
            callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'getDownloadUrl', function(entry, successCallback, failureCallback) {
        var url = getEntryURL(entry);
        fileManagerPrivateInternal.getDownloadUrl(
            url, callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'getDisallowedTransfers',
      function(
          entries, destinationEntry, isMove, successCallback, failureCallback) {
        var sourceUrls = entries.map(getEntryURL);
        var destinationUrl = getEntryURL(destinationEntry);
        fileManagerPrivateInternal.getDisallowedTransfers(
            sourceUrls, destinationUrl, isMove,
            callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'getDlpMetadata', function(entries, successCallback, failureCallback) {
        var sourceUrls = entries.map(getEntryURL);
        fileManagerPrivateInternal.getDlpMetadata(
            sourceUrls, callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'getDriveQuotaMetadata',
      function(entry, successCallback, failureCallback) {
        var url = getEntryURL(entry);
        fileManagerPrivateInternal.getDriveQuotaMetadata(
            url, callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'zipSelection',
      function(
          entries, parentEntry, destName, successCallback, failureCallback) {
        fileManagerPrivateInternal.zipSelection(
            getEntryURL(parentEntry), entries.map(getEntryURL), destName,
            callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'validatePathNameLength',
      function(entry, name, successCallback, failureCallback) {
        var url = getEntryURL(entry);
        fileManagerPrivateInternal.validatePathNameLength(
            url, name, callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'getDirectorySize', function(entry, successCallback, failureCallback) {
        var url = getEntryURL(entry);
        fileManagerPrivateInternal.getDirectorySize(
            url, callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'getRecentFiles',
      function(
          restriction, query, cutoffDays, file_type, invalidate_cache,
          successCallback, failureCallback) {
        let resultHandler = function(entryDescriptions) {
          return entryDescriptions.map(getExternalFileEntry);
        };
        // Due to C++integer limits we bind the JavaScript value to 0 .. 65535.
        // Negative values are not accepted as this means files modified in the
        // future. This limit means that x days after June 6, 2149 users will
        // not be able to ask for files modified before Jan 01 + x, 1970.
        const clampedCutoffDays = Math.min(Math.max(0, cutoffDays), 65535);
        fileManagerPrivateInternal.getRecentFiles(
            restriction, query, clampedCutoffDays, file_type, invalidate_cache,
            callbackAdaptor(successCallback, failureCallback, resultHandler));
      });

  apiFunctions.setHandleRequest(
      'sharePathsWithCrostini',
      function(vmName, entries, persist, successCallback, failureCallback) {
        const urls = entries.map(getEntryURL);
        fileManagerPrivateInternal.sharePathsWithCrostini(
            vmName, urls, persist,
            callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'unsharePathWithCrostini',
      function(vmName, entry, successCallback, failureCallback) {
        fileManagerPrivateInternal.unsharePathWithCrostini(
            vmName, getEntryURL(entry),
            callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'getCrostiniSharedPaths',
      function(
          observeFirstForSession, vmName, successCallback, failureCallback) {
        fileManagerPrivateInternal.getCrostiniSharedPaths(
            observeFirstForSession, vmName, function(response) {
              const {entries, firstForSession} = response;
              successCallback({
                entries: entries.map(getExternalFileEntry),
                firstForSession,
              });
            });
      });

  apiFunctions.setHandleRequest(
      'getLinuxPackageInfo', function(entry, successCallback, failureCallback) {
        var url = getEntryURL(entry);
        fileManagerPrivateInternal.getLinuxPackageInfo(
            url, callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'installLinuxPackage', function(entry, successCallback, failureCallback) {
        var url = getEntryURL(entry);
        fileManagerPrivateInternal.installLinuxPackage(
            url, callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setCustomCallback('searchFiles',
      function(callback, response) {
    if (response && !response.error && response.entries) {
      response.entries = response.entries.map(getExternalFileEntry);
    }

    // So |callback| doesn't break if response is not defined.
    if (!response) {
      response = {};
    }

    if (callback) {
      callback(response.entries);
    }
  });

  apiFunctions.setHandleRequest('importCrostiniImage', function(entry) {
    const url = getEntryURL(entry);
    fileManagerPrivateInternal.importCrostiniImage(url);
  });

  apiFunctions.setHandleRequest(
      'toggleAddedToHoldingSpace',
      function(entries, added, successCallback, failureCallback) {
        const urls = entries.map(getEntryURL);
        fileManagerPrivateInternal.toggleAddedToHoldingSpace(
            urls, added, callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'startIOTask',
      function(type, entries, params, successCallback, failureCallback) {
        const urls = entries.map(getEntryURL);
        let newParams = {};
        if (params.destinationFolder) {
          newParams.destinationFolderUrl =
              getEntryURL(params.destinationFolder);
        }
        if (params.password) {
          newParams.password = params.password;
        }
        if (params.showNotification !== undefined) {
          newParams.showNotification = params.showNotification;
        }
        fileManagerPrivateInternal.startIOTask(
            type, urls, newParams,
            callbackAdaptor(successCallback, failureCallback));
      });

  apiFunctions.setHandleRequest(
      'parseTrashInfoFiles',
      function(entries, successCallback, failureCallback) {
        const urls = entries.map(getEntryURL);
        let resultHandler = function(entryDescriptions) {
          return entryDescriptions.map(description => {
            description.restoreEntry =
                getExternalFileEntry(description.restoreEntry);
            return description;
          });
        };
        fileManagerPrivateInternal.parseTrashInfoFiles(
            urls,
            callbackAdaptor(successCallback, failureCallback, resultHandler));
      });
});

bindingUtil.registerEventArgumentMassager(
    'fileManagerPrivate.onDirectoryChanged', function(args, dispatch) {
  // Convert the entry arguments into a real Entry object.
  args[0].entry = getExternalFileEntry(args[0].entry);
  dispatch(args);
});

bindingUtil.registerEventArgumentMassager(
    'fileManagerPrivate.onCrostiniChanged', function(args, dispatch) {
  // Convert entries arguments into real Entry objects.
  const entries = args[0].entries;
  for (let i = 0; i < entries.length; i++) {
    entries[i] = getExternalFileEntry(entries[i]);
  }
  dispatch(args);
});

bindingUtil.registerEventArgumentMassager(
    'fileManagerPrivate.onIOTaskProgressStatus', function(args, dispatch) {
      // Convert outputs arguments into real Entry objects if they exist.
      const outputs = args[0].outputs;
      if (outputs) {
        for (let i = 0; i < outputs.length; i++) {
          outputs[i] = getExternalFileEntry(outputs[i]);
        }
      }
      dispatch(args);
    });
