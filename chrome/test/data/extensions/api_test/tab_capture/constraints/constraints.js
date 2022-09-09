// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function assertBindingsPassedWebKitErrorMessage() {
  // Note: This lastError.message is being passed from WebKit via the tabCapture
  // API custom bindings.  Thus, there is no check for a specific error message
  // string.  Instead, the following checks that a non-empty string message has
  // been set, indicating an error occurred.
  chrome.test.assertTrue(!!chrome.runtime.lastError.message);
}

chrome.test.runTests([
  function supportsMediaConstraints() {
    chrome.tabCapture.capture({
      video: true,
      audio: true,
      videoConstraints: {
          mandatory: {
            maxWidth: 1000,
            minWidth: 300
          }
      }
    }, function(stream) {
      chrome.test.assertTrue(!!stream);
      stream.getVideoTracks()[0].stop();
      stream.getAudioTracks()[0].stop();
      chrome.test.succeed();
    });
  },

  function rejectsOptionalMediaConstraints() {
    chrome.tabCapture.capture({
      video: true,
      audio: true,
      videoConstraints: {
        mandatory: {
        },
        optional: {
          maxWidth: 1000,
          minWidth: 300
        }
      }
    }, function(stream) {
      assertBindingsPassedWebKitErrorMessage();
      chrome.test.assertTrue(!stream);
      chrome.test.succeed();
    });
  },

  function ignoresInvalidConstraints() {
    chrome.tabCapture.capture({
      video: true,
      audio: true,
      videoConstraints: {
        mandatory: {
          notValid: '123'
        }
      }
    }, function(stream) {
      chrome.test.assertTrue(!!stream);
      stream.getVideoTracks()[0].stop();
      stream.getAudioTracks()[0].stop();
      chrome.test.succeed();
    });
  }
]);
