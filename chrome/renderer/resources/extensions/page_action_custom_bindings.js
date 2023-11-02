// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the pageAction API.

var getSetIconHandler = require('setIcon').getSetIconHandler;

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest(
      'setIcon', getSetIconHandler('pageAction.setIcon'));
});
