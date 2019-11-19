// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the fileManagerPrivate API.

// Natives
var fileManagerPrivateNatives = requireNative('file_manager_private');

// Internals
var fileManagerPrivateInternal = getInternalApi('fileManagerPrivateInternal');

// Shorthands
var GetFileSystem = fileManagerPrivateNatives.GetFileSystem;
var GetExternalFileEntry = fileManagerPrivateNatives.GetExternalFileEntry;

apiBridge.registerCustomHook(function(bindingsAPI) {
  // For FilesAppEntry types that wraps a native entry, returns the native entry
  // to be able to send to fileManagerPrivate API.
  function getEntryURL(entry) {
    const nativeEntry = entry.getNativeEntry && entry.getNativeEntry();
    if (nativeEntry)
      entry = nativeEntry;

    return fileManagerPrivateNatives.GetEntryURL(entry);
  }

  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback('searchDrive',
      function(name, request, callback, response) {
    if (response && !response.error && response.entries) {
      response.entries = response.entries.map(function(entry) {
        return GetExternalFileEntry(entry);
      });
    }

    // So |callback| doesn't break if response is not defined.
    if (!response)
      response = {};

    if (callback)
      callback(response.entries, response.nextFeed);
  });

  apiFunctions.setCustomCallback('searchDriveMetadata',
      function(name, request, callback, response) {
    if (response && !response.error) {
      for (var i = 0; i < response.length; i++) {
        response[i].entry =
            GetExternalFileEntry(response[i].entry);
      }
    }

    // So |callback| doesn't break if response is not defined.
    if (!response)
      response = [];

    if (callback)
      callback(response);
  });

  apiFunctions.setHandleRequest('resolveIsolatedEntries',
                                function(entries, callback) {
    var urls = entries.map(function(entry) {
      return getEntryURL(entry);
    });
    fileManagerPrivateInternal.resolveIsolatedEntries(urls, function(
        entryDescriptions) {
      callback(entryDescriptions.map(function(description) {
        return GetExternalFileEntry(description);
      }));
    });
  });

  apiFunctions.setHandleRequest('getEntryProperties',
                                function(entries, names, callback) {
    var urls = entries.map(function(entry) {
      return getEntryURL(entry);
    });
    fileManagerPrivateInternal.getEntryProperties(urls, names, callback);
  });

  apiFunctions.setHandleRequest('addFileWatch', function(entry, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.addFileWatch(url, callback);
  });

  apiFunctions.setHandleRequest('removeFileWatch', function(entry, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.removeFileWatch(url, callback);
  });

  apiFunctions.setHandleRequest('getCustomActions', function(
        entries, callback) {
    var urls = entries.map(function(entry) {
      return getEntryURL(entry);
    });
    fileManagerPrivateInternal.getCustomActions(urls, callback);
  });

  apiFunctions.setHandleRequest('executeCustomAction', function(
        entries, actionId, callback) {
    var urls = entries.map(function(entry) {
      return getEntryURL(entry);
    });
    fileManagerPrivateInternal.executeCustomAction(urls, actionId, callback);
  });

  apiFunctions.setHandleRequest('computeChecksum', function(entry, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.computeChecksum(url, callback);
  });

  apiFunctions.setHandleRequest('getMimeType', function(entry, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.getMimeType(url, callback);
  });

  apiFunctions.setHandleRequest('pinDriveFile', function(entry, pin, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.pinDriveFile(url, pin, callback);
  });

  apiFunctions.setHandleRequest('executeTask',
      function(taskId, entries, callback) {
        var urls = entries.map(function(entry) {
          return getEntryURL(entry);
        });
        fileManagerPrivateInternal.executeTask(taskId, urls, callback);
      });

  apiFunctions.setHandleRequest('setDefaultTask',
      function(taskId, entries, mimeTypes, callback) {
        var urls = entries.map(function(entry) {
          return getEntryURL(entry);
        });
        fileManagerPrivateInternal.setDefaultTask(
            taskId, urls, mimeTypes, callback);
      });

  apiFunctions.setHandleRequest('getFileTasks', function(entries, callback) {
    var urls = entries.map(function(entry) {
      return getEntryURL(entry);
    });
    fileManagerPrivateInternal.getFileTasks(urls, callback);
  });

  apiFunctions.setHandleRequest('getDownloadUrl', function(entry, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.getDownloadUrl(url, callback);
  });

  apiFunctions.setHandleRequest('startCopy', function(
        entry, parentEntry, newName, callback) {
    var url = getEntryURL(entry);
    var parentUrl = getEntryURL(parentEntry);
    fileManagerPrivateInternal.startCopy(
        url, parentUrl, newName, callback);
  });

  apiFunctions.setHandleRequest('zipSelection', function(
        parentEntry, entries, destName, callback) {
    var parentUrl = getEntryURL(parentEntry);
    var urls = entries.map(function(entry) {
      return getEntryURL(entry);
    });
    fileManagerPrivateInternal.zipSelection(
        parentUrl, urls, destName, callback);
  });

  apiFunctions.setHandleRequest('validatePathNameLength', function(
        entry, name, callback) {

    var url = getEntryURL(entry);
    fileManagerPrivateInternal.validatePathNameLength(url, name, callback);
  });

  apiFunctions.setHandleRequest('getDirectorySize', function(
        entry, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.getDirectorySize(url, callback);
  });

  apiFunctions.setHandleRequest('getRecentFiles', function(
        restriction, callback) {
    fileManagerPrivateInternal.getRecentFiles(restriction, function(
          entryDescriptions) {
      callback(entryDescriptions.map(function(description) {
        return GetExternalFileEntry(description);
      }));
    });
  });

  apiFunctions.setHandleRequest(
      'sharePathsWithCrostini', function(vmName, entries, persist, callback) {
        const urls = entries.map((entry) => {
          return getEntryURL(entry);
        });
        fileManagerPrivateInternal.sharePathsWithCrostini(
            vmName, urls, persist, callback);
      });

  apiFunctions.setHandleRequest(
      'unsharePathWithCrostini', function(vmName, entry, callback) {
        fileManagerPrivateInternal.unsharePathWithCrostini(
            vmName, getEntryURL(entry), callback);
      });

  apiFunctions.setHandleRequest(
      'getCrostiniSharedPaths',
      function(observeFirstForSession, vmName, callback) {
        fileManagerPrivateInternal.getCrostiniSharedPaths(
            observeFirstForSession, vmName,
            function(entryDescriptions, firstForSession) {
              callback(entryDescriptions.map(function(description) {
                return GetExternalFileEntry(description);
              }), firstForSession);
            });
      });

  apiFunctions.setHandleRequest(
      'getLinuxPackageInfo', function(entry, callback) {
        var url = getEntryURL(entry);
        fileManagerPrivateInternal.getLinuxPackageInfo(url, callback);
      });

  apiFunctions.setHandleRequest('installLinuxPackage', function(
        entry, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.installLinuxPackage(url, callback);
  });

  apiFunctions.setHandleRequest('getThumbnail', function(
        entry, cropToSquare, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.getThumbnail(url, cropToSquare, callback);
  });

  apiFunctions.setCustomCallback('searchFiles',
      function(name, request, callback, response) {
    if (response && !response.error && response.entries) {
      response.entries = response.entries.map(function(entry) {
        return GetExternalFileEntry(entry);
      });
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
});

bindingUtil.registerEventArgumentMassager(
    'fileManagerPrivate.onDirectoryChanged', function(args, dispatch) {
  // Convert the entry arguments into a real Entry object.
  args[0].entry = GetExternalFileEntry(args[0].entry);
  dispatch(args);
});

bindingUtil.registerEventArgumentMassager(
    'fileManagerPrivate.onCrostiniChanged', function(args, dispatch) {
  // Convert entries arguments into real Entry objects.
  const entries = args[0].entries;
  for (let i = 0; i < entries.length; i++) {
    entries[i] = GetExternalFileEntry(entries[i]);
  }
  dispatch(args);
});
