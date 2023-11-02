// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getVolumeList() {
    chrome.fileSystem.getVolumeList(
        chrome.test.callbackFail('Operation only supported for kiosk apps ' +
            'running in a kiosk session.',
            function(volumeList) {
              chrome.test.assertFalse(!!volumeList);
            }));
  }
]);
