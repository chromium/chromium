// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function saveFile() {
    chrome.fileSystem.chooseEntry({type: 'saveFile'}, chrome.test.callbackFail(
        'Operation requires fileSystem.write permission',  function() {}));
  }
]);
