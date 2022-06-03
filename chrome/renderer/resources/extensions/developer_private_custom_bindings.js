// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the developerPrivate API.

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  // Converts the argument of |functionName| from DirectoryEntry to URL.
  function bindFileSystemFunction(functionName) {
    apiFunctions.setUpdateArgumentsPostValidate(
        functionName, function(directoryEntry, callback) {
          var fileSystemName = directoryEntry.filesystem.name;
          var relativePath = $String.slice(directoryEntry.fullPath, 1);
          var url = directoryEntry.toURL();
          return [fileSystemName, relativePath, url, callback];
    });
  }

  bindFileSystemFunction('loadDirectory');

  // developerPrivate.enable is the same as chrome.management.setEnabled.
  // TODO(devlin): Migrate callers off developerPrivate.enable.
  bindingsAPI.compiledApi.enable = chrome.management.setEnabled;

  apiFunctions.setHandleRequest('allowFileAccess',
                                function(id, allow, callback) {
    chrome.developerPrivate.updateExtensionConfiguration(
        {extensionId: id, fileAccess: allow}, callback);
  });

  apiFunctions.setHandleRequest('allowIncognito',
                                function(id, allow, callback) {
    chrome.developerPrivate.updateExtensionConfiguration(
        {extensionId: id, incognitoAccess: allow}, callback);
  });

  apiFunctions.setHandleRequest('inspect', function(options, callback) {
    var renderViewId = options.render_view_id;
    if (typeof renderViewId === 'string') {
      renderViewId = parseInt(renderViewId);
      if (isNaN(renderViewId))
        throw new Error('Invalid value for render_view_id');
    }
    var renderProcessId = options.render_process_id;
    if (typeof renderProcessId === 'string') {
      renderProcessId = parseInt(renderProcessId);
      if (isNaN(renderProcessId))
        throw new Error('Invalid value for render_process_id');
    }
    chrome.developerPrivate.openDevTools({
        extensionId: options.extension_id,
        renderProcessId: renderProcessId,
        renderViewId: renderViewId,
        incognito: options.incognito
    }, callback);
  });
});
