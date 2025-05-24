// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A test that exercises the API method calls.
function testBasics() {
  chrome.power.requestKeepAwake('system');
  chrome.power.releaseKeepAwake();

  chrome.power.requestKeepAwake('display')
  chrome.power.releaseKeepAwake();

  chrome.test.succeed();
}

chrome.test.runTests([testBasics]);
