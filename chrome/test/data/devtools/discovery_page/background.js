// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var REMOTE_DEBUGGER_HOST = 'localhost:9222';

chrome.test.runTests([
  async function discoveryPageNotEmbeddable() {
    const response = await fetch(`http://${REMOTE_DEBUGGER_HOST}/`);
    chrome.test.assertEq(response.headers.get('X-Frame-Options'), 'DENY');
    chrome.test.succeed();
  }
]);
