// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the input ime API. Only injected into the
// v8 contexts for extensions which have permission for the API.

const appWindowNatives = requireNative('app_window_natives');

let keyEventHandled;
bindingUtil.registerEventArgumentMassager(
    'input.ime.onKeyEvent', function(args, dispatch) {
      const keyData = args[1];
      let result = undefined;
      try {
        // dispatch() is weird - it returns an object {results: array<results>}
        // iff there is at least one result value that !== undefined. Since
        // onKeyEvent has a maximum of one listener, we know that any result we
        // find is the one we're interested in.
        const dispatchResult = dispatch(args);
        if (dispatchResult && dispatchResult.results) {
          result = dispatchResult.results[0];
        }
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
  const originalAddListener = api.compiledApi.onKeyEvent.addListener;
  api.compiledApi.onKeyEvent.addListener = function(cb, opt_extraInfo) {
    $Function.call(originalAddListener, this, cb);
  };

  api.apiFunctions.setCustomCallback(
      'createWindow', function(callback, windowParams) {
        if (!callback) {
          return;
        }
        let view;
        if (windowParams && windowParams.frameToken) {
          view = appWindowNatives.GetFrame(
              windowParams.frameToken, false /* notifyBrowser */);
          view.id = windowParams.frameToken;
        }
        callback(view);
      });
});
