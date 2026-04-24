// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Set the state to STARTED to automatically start listening to
// MediaPerceptionDetection signals.

chrome.test.runTests([
    function setStateRunning() {
      // Need to start the mocked media analytics process.
      chrome.mediaPerceptionPrivate.setState({
        status: 'RUNNING'
      }, chrome.test.callbackPass(function(state) {
        chrome.test.assertEq({ status: 'RUNNING' }, state);
      }));
    },
    function registerListener() {
      chrome.test.listenOnce(
          chrome.mediaPerceptionPrivate.onMediaPerception,
          function(evt) {
            chrome.test.assertEq({
              framePerceptions: [{
                frameId: 1
              }]
            }, evt);
          });
      // By sending this message, we trigger the fake D-Bus client to fire an
      // onMediaPerception event.
      chrome.test.sendMessage('mediaPerceptionListenerSet');
    }
]);

