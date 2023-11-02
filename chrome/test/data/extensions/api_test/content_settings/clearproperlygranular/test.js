// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content settings API test
// Run with browser_tests --gtest_filter=ExtensionApiTest.ClearProperlyGranular

var cs = chrome.contentSettings;

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

chrome.test.runTests([
  function setAndCheckContentSettings() {
    // Set settings for the camera and microphone to block.
    cs['camera'].set({
      'primaryPattern': '<all_urls>',
      'secondaryPattern': '<all_urls>',
      'setting': 'block'
    });

    cs['microphone'].set({
      'primaryPattern': '<all_urls>',
      'secondaryPattern': '<all_urls>',
      'setting': 'block'
    });

    // Clearing the camera settings should leave the microphone settings
    // unchanged.
    cs['camera'].clear({});
    var microphoneMessage = 'The microphone setting should be "block", but ' +
      'was reset.';
    cs['microphone'].get({
      'primaryUrl': 'http://www.example.com',
      'secondaryUrl': 'http://www.example.com'
    }, expect({'setting': 'block'}, microphoneMessage));

    var cameraMessage = 'The camera setting was reset and should be "ask"';
    cs['camera'].get({
      'primaryUrl': 'http://www.example.com',
      'secondaryUrl': 'http://www.example.com'
    }, expect({'setting': 'ask'}, cameraMessage));

    // Clear microphone and ensure that its setting updates properly.
    cs['microphone'].clear({});
    microphoneMessage = 'The microphone setting was reset and should be "ask"';
    cs['microphone'].get({
      'primaryUrl': 'http://www.example.com',
      'secondaryUrl': 'http://www.example.com'
    }, expect({'setting': 'ask'}, microphoneMessage));
  },
]);
