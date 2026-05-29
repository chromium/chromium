// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.addEventListener('message', function(e) {
  if (e.data === 'WebViewTest.LAUNCHED') {
    chrome.test.sendMessage('WebViewTest.LAUNCHED');
  } else if (e.data === 'WebViewTest.FAILURE') {
    chrome.test.sendMessage('WebViewTest.FAILURE');
  }
});
