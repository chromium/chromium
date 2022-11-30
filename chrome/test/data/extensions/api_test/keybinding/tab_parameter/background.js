// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called when the user activates the command.
chrome.commands.onCommand.addListener(function(command, tab) {
  try {
    chrome.test.assertEq('toggle-feature', command);
    chrome.test.assertEq('complete', tab.status);
    chrome.test.assertEq(
        '/extensions/test_file.txt', (new URL(tab.url).pathname));
    chrome.test.notifyPass();
  } catch (e) {
    chrome.test.notifyFail(e.message);
  }
});

chrome.test.notifyPass();
