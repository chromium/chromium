// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.sendMessage('tab->worker', response => {
  if (response != 'tab->worker->tab') {
    chrome.test.sendMessage('FAILURE');
    return;
  }
  chrome.test.sendMessage('CONTENT_SCRIPT_RECEIVED_REPLY');
});
