// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

navigator.webkitPersistentStorage.requestQuota(1, pass, fail);

function pass() {
  console.log("PASS");
  if (window.chrome && chrome.test && chrome.test.succeed)
    chrome.test.succeed();
}

function fail() {
  console.log("FAIL");
  if (window.chrome && chrome.test && chrome.test.fail)
    chrome.test.fail();
}
