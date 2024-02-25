// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the tts API.

var idGenerator = requireNative('id_generator');
var lazyBG = requireNative('lazy_background_page');

apiBridge.registerCustomHook(function(api) {
  var apiFunctions = api.apiFunctions;
  var tts = api.compiledApi;
  var handlers = {};

  function ttsEventListener(event) {
    var eventHandler = handlers[event.srcId];
    if (eventHandler) {
      eventHandler({
                     type: event.type,
                     charIndex: event.charIndex,
                     length: event.length,
                     errorMessage: event.errorMessage
                   });
      if (event.isFinalEvent) {
        delete handlers[event.srcId];
        // Balanced in 'speak' handler.
        lazyBG.DecrementKeepaliveCount();
      }
    }
  }

  // This file will get run if an extension needs the ttsEngine permission, but
  // it doesn't necessarily have the tts permission. If it doesn't, trying to
  // add a listener to chrome.tts.onEvent will fail.
  // See http://crbug.com/122474.
  try {
    tts.onEvent.addListener(ttsEventListener);
  } catch (e) {}

  apiFunctions.setHandleRequest(
      'speak', function(utterance, options, callback) {
        if (options && options.onEvent) {
          var id = idGenerator.GetNextId();
          options.srcId = id;
          handlers[id] = options.onEvent;
          // Keep the page alive until the event finishes.
          // Balanced in eventHandler.
          lazyBG.IncrementKeepaliveCount();
        }
        bindingUtil.sendRequest(
            'tts.speak', [utterance, options, callback], undefined);
      });
});
