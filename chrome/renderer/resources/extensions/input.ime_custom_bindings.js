// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the input ime API. Only injected into the
// v8 contexts for extensions which have permission for the API.

var appWindowNatives = requireNative('app_window_natives');

var keyEventHandled;
bindingUtil.registerEventArgumentMassager('input.ime.onKeyEvent',
                                          function(args, dispatch) {
  var keyData = args[1];
  var result = undefined;
  try {
    // dispatch() is weird - it returns an object {results: array<results>} iff
    // there is at least one result value that !== undefined. Since onKeyEvent
    // has a maximum of one listener, we know that any result we find is the one
    // we're interested in.
    var dispatchResult = dispatch(args);
    if (dispatchResult && dispatchResult.results)
      result = dispatchResult.results[0];
  } catch (e) {
    result = false;
    console.error('Error in event handler for onKeyEvent: ' + e.stack);
  }
  if (result !== undefined) {
    keyEventHandled(keyData.requestId, !!result);
  }
});

apiBridge.registerCustomHook(function(api) {
  keyEventHandled = api.compiledApi.keyEventHandled;

  // TODO(shuchen): override onKeyEvent.addListener only for compatibility.
  // This should be removed after the IME extension doesn't rely on the
  // additional "async" parameter.
  var originalAddListener = api.compiledApi.onKeyEvent.addListener;
  api.compiledApi.onKeyEvent.addListener = function(cb, opt_extraInfo) {
    $Function.call(originalAddListener, this, cb);
  };

  api.apiFunctions.setCustomCallback('createWindow',
      function(name, request, callback, windowParams) {
    if (!callback) {
      return;
    }
    var view;
    if (windowParams && windowParams.frameId) {
      view = appWindowNatives.GetFrame(
          windowParams.frameId, false /* notifyBrowser */);
      view.id = windowParams.frameId;
    }
    callback(view);
  });
});
