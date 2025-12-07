// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.windows.create({'url': 'hello.html'}).then((window) => {
  chrome.windows.update(window.id, {'state': 'fullscreen'}).then(() => {
    chrome.test.sendMessage('ready');
  });
});
