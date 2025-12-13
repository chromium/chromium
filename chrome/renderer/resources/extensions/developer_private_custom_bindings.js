// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the developerPrivate API.

apiBridge.registerCustomHook(function(bindingsAPI) {
  const apiFunctions = bindingsAPI.apiFunctions;

  // Converts the argument of |functionName| from DirectoryEntry to URL.
  function bindFileSystemFunction(functionName) {
    apiFunctions.setUpdateArgumentsPostValidate(
        functionName, function(directoryEntry, callback) {
          const fileSystemName = directoryEntry.filesystem.name;
          const relativePath = $String.slice(directoryEntry.fullPath, 1);
          const url = directoryEntry.toURL();
          return [fileSystemName, relativePath, url, callback];
        });
  }

  bindFileSystemFunction('loadDirectory');

  apiFunctions.setHandleRequest('inspect', function(options, callback) {
    let renderViewId = options.render_view_id;
    if (typeof renderViewId === 'string') {
      renderViewId = parseInt(renderViewId);
      if (isNaN(renderViewId)) {
        throw new Error('Invalid value for render_view_id');
      }
    }
    let renderProcessId = options.render_process_id;
    if (typeof renderProcessId === 'string') {
      renderProcessId = parseInt(renderProcessId);
      if (isNaN(renderProcessId)) {
        throw new Error('Invalid value for render_process_id');
      }
    }
    chrome.developerPrivate.openDevTools(
        {
          extensionId: options.extension_id,
          renderProcessId: renderProcessId,
          renderViewId: renderViewId,
          incognito: options.incognito,
        },
        callback);
  });
});
