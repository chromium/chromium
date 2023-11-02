// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the webrtcLoggingPrivate API.

var getBindDirectoryEntryCallback =
    require('fileEntryBindingUtil').getBindDirectoryEntryCallback;

apiBridge.registerCustomHook(function(binding, id, contextType) {
  var apiFunctions = binding.apiFunctions;
  apiFunctions.setCustomCallback('getLogsDirectory',
                                 getBindDirectoryEntryCallback());
});
