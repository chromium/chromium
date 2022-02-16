// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the systemIndicator API.
// TODO(dewittj) Refactor custom binding to reduce redundancy between the
// extension action APIs.

var setIcon = require('setIcon').setIcon;

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest(
      'setIcon', function(details, successCallback, failureCallback) {
        var onIconRetrieved = function(iconSpec) {
          bindingUtil.sendRequest(
              'systemIndicator.setIcon', [iconSpec, successCallback],
              /*options=*/ undefined);
        };
        setIcon(details, onIconRetrieved, failureCallback);
      });
});
