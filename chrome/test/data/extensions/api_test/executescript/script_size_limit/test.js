// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getFileTooLargeError(fileName) {
  return `Could not load file: '${fileName}'. Resource size exceeded.`;
}

chrome.test.getConfig(config => {
  chrome.test.runTests([
    function scriptFileWithinLimit() {
      const url = `http://example.com:${config.testServer.port}/simple.html`;
      chrome.tabs.create({url}, tab => {
        chrome.tabs.executeScript(tab.id, {file: 'small.js'}, () => {
          chrome.tabs.get(tab.id, ({title}) => {
            chrome.test.assertEq('small', title);
            chrome.test.succeed();
          });
        });
      });
    },

    function scriptFileExceedsLimit() {
      const url = `http://example.com:${config.testServer.port}/simple.html`;
      chrome.tabs.create({url}, tab => {
        chrome.tabs.executeScript(tab.id, {file: 'big.js'}, () => {
          chrome.test.assertLastError(getFileTooLargeError('big.js'));
          chrome.test.succeed();
        });
      });
    },
  ]);
});
