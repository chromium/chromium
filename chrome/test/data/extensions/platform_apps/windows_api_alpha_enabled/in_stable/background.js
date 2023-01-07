// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error = "The alphaEnabled option requires dev channel or newer.";

function testAlphaEnabled(testId, setValue) {
  var createOptions = { frame: 'none' };
  createOptions.alphaEnabled = setValue;

  chrome.app.window.create('index.html',
                           createOptions,
                           chrome.test.callbackFail(error));
}

// All these tests are run in Stable channel with app.window.alpha
// permission set.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([

    // Window is created with alphaEnabled set to true.
    function testAlphaEnabledStableInitTrue() {
      testAlphaEnabled('testAlphaEnabledStableInitTrue', true);
    },

    // Window is created with alphaEnabled set to false.
    function testAlphaEnabledStableInitFalse() {
      testAlphaEnabled('testAlphaEnabledStableInitFalse', false);
    },

  ]);
});
