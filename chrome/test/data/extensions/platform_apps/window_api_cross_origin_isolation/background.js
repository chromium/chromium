// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([
    function testCreate() {
      chrome.app.window.create('index.html',
                               {id: 'testId'},
                               chrome.test.callbackPass(function(win) {
        chrome.test.assertEq(typeof win.contentWindow.window, 'object');
        chrome.test.assertTrue(
          typeof win.contentWindow.document !== 'undefined');
        chrome.test.assertFalse(
          'about:blank' === win.contentWindow.location.href);
        var cw = win.contentWindow.chrome.app.window.current();
        chrome.test.assertEq(cw, win);
        chrome.test.assertEq('testId', cw.id);
        win.contentWindow.close();
      }));
    },
  ]);
});

