// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the ttsEngine API.

bindingUtil.registerEventArgumentMassager('ttsEngine.onSpeak',
                                          function(args, dispatch) {
  var text = args[0];
  var options = args[1];
  var requestId = args[2];
  var sendTtsEvent = function(event) {
    chrome.ttsEngine.sendTtsEvent(requestId, event);
  };
  dispatch([text, options, sendTtsEvent]);
});

apiBridge.registerCustomHook(function(api) {
  // Provide a warning if deprecated parameters are used.
  api.apiFunctions.setHandleRequest('updateVoices', function(voices) {
    for (var i = 0; i < voices.length; i++) {
      if (voices[i].gender) {
        console.warn(
            'chrome.ttsEngine.updateVoices: ' +
            'Voice gender is deprecated and values will be ignored ' +
            'starting in Chrome 71.');
        break;
      }
    }
    bindingUtil.sendRequest(
        'ttsEngine.updateVoices', [voices], undefined);
  });
}.bind(this));
