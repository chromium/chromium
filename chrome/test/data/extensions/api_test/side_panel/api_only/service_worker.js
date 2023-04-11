// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // Setting a panel option without explicitly setting the optional enabled bool
  // value defaults to enabled being true. Bug fix for crbug.com/1432012.
  async function setAndGetPanelOptionAndEnsureEnabled() {
    await chrome.sidePanel.setOptions({path: 'path.html'});
    let result = await chrome.sidePanel.getOptions({});
    const expected = {path: 'path.html', enabled: true};
    chrome.test.assertEq(expected, result);
    chrome.test.succeed();
  },
]);
