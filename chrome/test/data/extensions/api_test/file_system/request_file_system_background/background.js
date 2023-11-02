// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function withoutForeground() {
    chrome.fileSystem.requestFileSystem(
        {volumeId: 'testing:read-only'},
        chrome.test.callbackFail('Impossible to ask for user consent as ' +
            'there is no app window visible.', function(fs) {}));
  },
  function withForeground() {
    chrome.app.window.create('test.html', {},
        chrome.test.callbackPass(function(appWindow) {
          chrome.fileSystem.requestFileSystem(
              {volumeId: 'testing:read-only'},
              chrome.test.callbackPass(function(fileSystem) {
                chrome.test.assertFalse(!!chrome.runtime.lastError);
                chrome.test.assertTrue(!!fileSystem);
              }));
        }));
  },
]);
