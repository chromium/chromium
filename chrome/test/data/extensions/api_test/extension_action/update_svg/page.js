// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  async function setIconFromPage() {
    await chrome.action.setIcon({path: 'icon.svg'});
    chrome.test.succeed();
  },
]);
