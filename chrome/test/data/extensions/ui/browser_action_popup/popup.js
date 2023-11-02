// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Wait for the window to fully load, since that's when the popup is shown.
window.onload = function() {
  window.setTimeout(function() {
    chrome.test.sendMessage('Popup opened');
  }, 0);
};
