// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function messagesDeleted() {
      chrome.test.listenOnce(chrome.gcm.onMessagesDeleted, function() {
        chrome.test.assertTrue(true);
      });
    }
  ]);
};
