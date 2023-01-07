// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error = 'The alphaEnabled option can only be used with "frame: \'none\'".';

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([
    function testAlphaEnabledFrameNone() {
      chrome.app.window.create('index.html', {
        frame: 'none',
        alphaEnabled: true,
      }, chrome.test.callbackPass(function (win) {}));
    },
    function testAlphaEnabledFrameChrome() {
      chrome.app.window.create('index.html', {
        alphaEnabled: true,
      }, chrome.test.callbackFail(error));
    },
    function testAlphaDisabledFrameChrome() {
      chrome.app.window.create('index.html', {
      }, chrome.test.callbackPass(function (win) {}));
    },
  ]);
});

