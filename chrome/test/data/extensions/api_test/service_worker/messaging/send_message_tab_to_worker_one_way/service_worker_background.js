// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
  chrome.test.sendMessage(
      msg == 'tab->worker' ? 'WORKER_RECEIVED_MESSAGE' : 'FAILURE');
});

chrome.test.sendMessage('WORKER_RUNNING');
