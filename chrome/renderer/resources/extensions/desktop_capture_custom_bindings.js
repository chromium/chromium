// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the desktopCapture API.

var idGenerator = requireNative('id_generator');

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  var pendingRequests = {};

  function onRequestResult(id, result, options) {
    if (id in pendingRequests) {
      var callback = pendingRequests[id];
      delete pendingRequests[id];
      callback(result, options);
    }
  }

  apiFunctions.setHandleRequest('chooseDesktopMedia',
                                function(sources, target_tab, options,
                                         callback) {
    // |target_tab| is an optional parameter.
    if (callback === undefined) {
      callback = target_tab;
      target_tab = undefined;
    }
    var id = idGenerator.GetNextId();
    pendingRequests[id] = callback;
    bindingUtil.sendRequest('desktopCapture.chooseDesktopMedia',
                            [id, sources, target_tab, options,
                            $Function.bind(onRequestResult, null, id)],
                            undefined);
    return id;
  });

  apiFunctions.setHandleRequest('cancelChooseDesktopMedia', function(id) {
    if (id in pendingRequests) {
      delete pendingRequests[id];
      bindingUtil.sendRequest(
          'desktopCapture.cancelChooseDesktopMedia',
          [id], undefined, undefined);
    }
  });
});

