// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  if (request != 'worker->tab') {
    chrome.test.sendMessage('FAILURE');
    return;
  }
  sendResponse('worker->tab->worker');
});
