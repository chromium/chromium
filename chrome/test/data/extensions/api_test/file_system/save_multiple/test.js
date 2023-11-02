// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function saveFile() {
    chrome.fileSystem.chooseEntry(
        {type: 'saveFile', acceptsMultiple: true}, chrome.test.callbackFail(
            "acceptsMultiple: true is only supported for 'openFile'"));
  }
]);
