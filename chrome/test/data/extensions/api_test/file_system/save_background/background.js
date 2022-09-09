// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function openFile() {
    chrome.fileSystem.chooseEntry({type: 'saveFile'}, chrome.test.callbackFail(
        "Invalid calling page. This function can't be called from a " +
        "background page.", function(entry) {}));
  }
]);
