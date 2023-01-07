// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error = "The alphaEnabled option requires app.window.alpha permission.";

function testAlphaEnabled(testId, setValue) {
  var createOptions = { frame: 'none' };
  createOptions.alphaEnabled = setValue;

  chrome.app.window.create('index.html',
                           createOptions,
                           chrome.test.callbackFail(error));
}

// All these tests are run without app.window.alpha permission
// set. Test results are the same regardless of whether or not
// alpha (transparency) is supported by the platform.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([

    // Window is created with alphaEnabled set to true.
    function testAlphaEnabledNoPermInitTrue() {
      testAlphaEnabled('testAlphaEnabledNoPermInitTrue', true);
    },

    // Window is created with alphaEnabled set to false.
    function testAlphaEnabledNoPermInitFalse() {
      testAlphaEnabled('testAlphaEnabledNoPermInitFalse', false);
    },

  ]);
});

