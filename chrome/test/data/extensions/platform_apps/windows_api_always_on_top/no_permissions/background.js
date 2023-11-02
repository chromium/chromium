// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;

function setAlwaysOnTop(value) {
  chrome.app.window.create('index.html',
                           {},
                           callbackPass(function(win) {
    chrome.test.assertFalse(win.isAlwaysOnTop());

    // Attempt to use setAlwaysOnTop() to change the property.
    win.setAlwaysOnTop(value);

    // setAlwaysOnTop() is synchronous in the browser, so send an async request
    // to ensure we get the updated value of isAlwaysOnTop().
    chrome.test.waitForRoundTrip("", callbackPass(function() {
      // Check that isAlwaysOnTop() always returns false.
      chrome.test.assertFalse(win.isAlwaysOnTop());

      win.contentWindow.close();
    }));
  }));
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([

    // Attempting to create an alwaysOnTop window without the permission should
    // fail to create a window.
    function testCreateAlwaysOnTopEnabled() {
      var options = {
        alwaysOnTop: true
      };

      chrome.app.window.create(
          'index.html', options,
          callbackFail('The "app.window.alwaysOnTop" permission is required.'));
    },

    // Setting the alwaysOnTop property to false without the permission should
    // still result in a window being created.
    function testCreateAlwaysOnTopDisabled() {
      var options = {
        alwaysOnTop: false
      };

      chrome.app.window.create('index.html',
                               options,
                               callbackPass(function(win) {
        chrome.test.assertFalse(win.isAlwaysOnTop());
        win.contentWindow.close();
      }));
    },

    // Enabling the alwaysOnTop property after window creation without the
    // permission should fail.
    function testSetAlwaysOnTopEnabled() {
      setAlwaysOnTop(true);
    },

    // Disabling the alwaysOnTop property after window creation without the
    // permission should not change the property.
    function testSetAlwaysOnTopDisabled() {
      setAlwaysOnTop(false);
    }

  ]);
});
