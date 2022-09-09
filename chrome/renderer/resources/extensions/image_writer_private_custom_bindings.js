// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the image writer private API.

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setUpdateArgumentsPostValidate(
      'writeFromFile', function(device, fileEntry, callback) {
    var fileSystemName = fileEntry.filesystem.name;
    var relativePath = $String.slice(fileEntry.fullPath, 1);
    return [device, fileSystemName, relativePath, callback];
  });
});
