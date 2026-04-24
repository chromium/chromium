// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
    function setStateRunning() {
      // Need to start the mocked media analytics process.
      chrome.mediaPerceptionPrivate.setState({
        status: 'RUNNING'
      }, chrome.test.callbackPass(function(state) {
        chrome.test.assertEq({ status: 'RUNNING' }, state);
      }));
    },
    function getDiagnostics() {
      chrome.mediaPerceptionPrivate.getDiagnostics(
          chrome.test.callbackPass(function(response) {
            chrome.test.assertEq({
              perceptionSamples: [{
                framePerception: {
                  frameId: 1
                }
              }]
            }, response);
          }));
    }
]);
