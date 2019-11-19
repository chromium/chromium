// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

  apiFunctions.setHandleRequest('speak', function() {
    var args = $Array.from(arguments);
    if (args.length > 1 && args[1]) {
      if (args[1].onEvent) {
        var id = idGenerator.GetNextId();
        args[1].srcId = id;
        handlers[id] = args[1].onEvent;
        // Keep the page alive until the event finishes.
        // Balanced in eventHandler.
        lazyBG.IncrementKeepaliveCount();
      }
      if (args[1].gender) {
        console.warn(
            'chrome.tts.speak: ' +
            'Voice gender is deprecated and values will be ignored starting ' +
            'in Chrome 71.');
      }
    }
    bindingUtil.sendRequest('tts.speak', args, undefined);
    return id;
  });
});
