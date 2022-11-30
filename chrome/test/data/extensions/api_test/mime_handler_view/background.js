// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testComplete = false;

function startBackgroundPageTest(onSuspend) {
  chrome.runtime.onSuspend.addListener(() => {
    if (!testComplete) {
      onSuspend();
    }
  });
}

function endBackgroundPageTest() {
  testComplete = true;
}
