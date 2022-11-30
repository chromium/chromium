// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessage.addListener((msg) => {
  if (msg != 'worker->tab') {
    chrome.runtime.sendMessage('failure');
    return;
  }
  chrome.runtime.sendMessage('worker->tab->worker');
});

