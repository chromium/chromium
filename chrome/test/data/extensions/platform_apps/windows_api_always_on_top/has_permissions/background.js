// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

function testAlwaysOnTop(testId, initValue, setOption) {
  var options = { id: testId };
  if (setOption)
    options.alwaysOnTop = initValue;

  chrome.app.window.create('index.html',
                           options,
                           callbackPass(function(win) {
    // Check that isAlwaysOnTop() returns the initial value.
    chrome.test.assertEq(initValue, win.isAlwaysOnTop());

    // Toggle the current value.
    win.setAlwaysOnTop(!initValue);

    // setAlwaysOnTop is synchronous in the browser, so send an async request to
    // ensure we get the updated value of isAlwaysOnTop.
    chrome.test.waitForRoundTrip("msg", callbackPass(function(platformInfo) {
      // Check that isAlwaysOnTop() returns the new value.
      chrome.test.assertEq(!initValue, win.isAlwaysOnTop());

      win.contentWindow.close();
    }));
  }));
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([

    // Window is created with always on top enabled.
    function testAlwaysOnTopInitTrue() {
      testAlwaysOnTop('testAlwaysOnTopInitTrue', true, true);
    },

    // Window is created with always on top explicitly disabled.
    function testAlwaysOnTopInitFalse() {
      testAlwaysOnTop('testAlwaysOnTopInitFalse', false, true);
    },

    // Window is created with option not explicitly set.
    function testAlwaysOnTopNoInit() {
      testAlwaysOnTop('testAlwaysOnTopNoInit', false, false);
    }

  ]);
});
