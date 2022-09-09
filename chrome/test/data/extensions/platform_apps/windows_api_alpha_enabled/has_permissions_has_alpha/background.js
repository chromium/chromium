// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testAlphaEnabled(testId, setOption, setValue, expectedValue) {
  var createOptions = { frame: 'none' };
  if (setOption)
    createOptions.alphaEnabled = setValue;

  chrome.app.window.create('index.html',
                           createOptions,
                           chrome.test.callbackPass(function(win) {
      chrome.test.assertEq(expectedValue, win.alphaEnabled());
  }));
}

// All these tests are run with app.window.alpha permission
// set and on a system with alpha (transparency) support.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([

    // Window is created with alphaEnabled set to true.
    function testAlphaEnabledPermTransInitTrue() {
      testAlphaEnabled('testAlphaEnabledPermTransInitTrue',
                       true, true, true);
    },

    // Window is created with alphaEnabled set to false.
    function testAlphaEnabledPermTransInitFalse() {
      testAlphaEnabled('testAlphaEnabledPermTransInitFalse',
                       true, false, false);
    },

    // Window is created with alphaEnabled not explicitly set.
    function testAlphaEnabledPermTransNoInit() {
      testAlphaEnabled('testAlphaEnabledPermTransNoInit',
                       false, false, false);
    }

  ]);
});
