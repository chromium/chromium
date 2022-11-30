// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.sendMessage('Launched', function(response) {
  if (response == "restart") {
    chrome.runtime.restart();
    chrome.test.sendMessage('restartRequested');
  }
});
