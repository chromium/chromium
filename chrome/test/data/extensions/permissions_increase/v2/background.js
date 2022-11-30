// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onInstalled.addListener(function(detail) {
  if (detail.previousVersion === '1') {
    chrome.test.sendMessage('v2.onInstalled');
  } else {
    chrome.test.sendMessage('FAILED');
  }
});
