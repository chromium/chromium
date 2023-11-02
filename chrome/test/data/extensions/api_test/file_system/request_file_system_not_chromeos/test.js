// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function readOnlyVolume() {
    chrome.fileSystem.requestFileSystem(
        {volumeId: 'testing:read-only'},
        chrome.test.callbackFail('Operation not supported on the current ' +
            'platform.',
            function(fileSystem) {
              chrome.test.assertFalse(!!fileSystem);
            }));
  },
]);
