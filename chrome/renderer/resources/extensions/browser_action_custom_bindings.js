// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the browserAction API.

var getSetIconHandler = require('setIcon').getSetIconHandler;
var getExtensionViews = requireNative('runtime').GetExtensionViews;

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest(
      'setIcon', getSetIconHandler('browserAction.setIcon'));

  apiFunctions.setCustomCallback('openPopup',
      function(callback, response) {
    if (!callback)
      return;

    if (bindingUtil.hasLastError()) {
      callback();
    } else {
      var views = getExtensionViews(-1, -1, 'POPUP');
      callback(views.length > 0 ? views[0] : null);
    }
  });
});
