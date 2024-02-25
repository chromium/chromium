// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function withForeground() {
    chrome.app.window.create('test.html', {},
      chrome.test.callbackPass(function (appWindow) {
        chrome.fileSystem.requestFileSystem(
          { volumeId: 'testing:read-only' },
          chrome.test.callbackPass(function (fileSystem) {
            chrome.test.assertFalse(!!chrome.runtime.lastError);
            chrome.test.assertTrue(!!fileSystem);
            fileSystem.root.getFile('open_existing.txt', {},
                chrome.test.callbackPass(function (entry) {
                  checkEntry(entry, 'open_existing.txt', false, false);
                }), function (err) {
                  console.log(err.message);
                });
          }));
      }));
  },
]);
