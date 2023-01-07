// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    async function fetchFileSchemeResource() {
      var url = config.testDataDirectory + '/../test_file.txt';
      const response = await fetch(url);
      const text = await response.text();
      chrome.test.assertEq('Hello!', text);
      chrome.test.succeed();
    }
  ]);
});
