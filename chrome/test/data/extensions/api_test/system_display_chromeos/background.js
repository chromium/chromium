// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function waitForDisplayChangedEvent() {
    chrome.test.listenOnce(chrome.system.display.onDisplayChanged,
                           function() {
                             chrome.test.sendMessage("success");
                           });
  }
]);

chrome.test.sendMessage('ready');
