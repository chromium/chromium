// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the browserAction API.

var setIcon = require('setIcon').setIcon;
var getExtensionViews = requireNative('runtime').GetExtensionViews;

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('setIcon', function(details, callback) {
    setIcon(details, function(args) {
      bindingUtil.sendRequest(
          'browserAction.setIcon', [args, callback], undefined);
    }.bind(this));
  });

  apiFunctions.setCustomCallback('openPopup',
      function(name, request, callback, response) {
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
