// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the webrtcDesktopCapturePrivate API.

var idGenerator = requireNative('id_generator');

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  var pendingRequests = {};

  function onRequestResult(id, result) {
    if (id in pendingRequests) {
      var callback = pendingRequests[id];
      delete pendingRequests[id];
      callback(result);
    }
  }

  apiFunctions.setHandleRequest('chooseDesktopMedia',
                                function(sources, request, callback) {
    var id = idGenerator.GetNextId();
    pendingRequests[id] = callback;
    bindingUtil.sendRequest(
        'webrtcDesktopCapturePrivate.chooseDesktopMedia',
        [id, sources, request, $Function.bind(onRequestResult, null, id)],
        undefined);
    return id;
  });

  apiFunctions.setHandleRequest('cancelChooseDesktopMedia', function(id) {
    if (id in pendingRequests) {
      delete pendingRequests[id];
      bindingUtil.sendRequest(
          'webrtcDesktopCapturePrivate.cancelChooseDesktopMedia', [id],
          undefined);
    }
  });
});
