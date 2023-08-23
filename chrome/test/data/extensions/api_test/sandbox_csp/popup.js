// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function checkExecuteScript() {
    chrome.tabs.query({ active: true, currentWindow: true }, tabs => {
      chrome.tabs.executeScript(
        tabs[0].id,
        { code: 'var x = 1;' },
        () => {
          let lastError = chrome.runtime.lastError;
          if (lastError) {
            chrome.test.fail();
          } else {
            chrome.test.succeed();
          }
        }
      );
    });
  },
]);
