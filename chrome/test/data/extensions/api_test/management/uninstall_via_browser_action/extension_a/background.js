// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.browserAction.onClicked.addListener(() => {
  // We should be running with a user gesture.
  chrome.test.assertTrue(chrome.test.isProcessingUserGesture());

  // Uninstall the extension named 'ExtensionB'.
  chrome.management.getAll(chrome.test.callbackPass(items => {
    var extension = items.find(item => {
      return item.name == 'ExtensionB';
    });
    chrome.test.assertNe(undefined, extension);
    chrome.test.assertNe(undefined, extension.id);
    chrome.management.uninstall(extension.id, chrome.test.callbackPass(() => {
      chrome.test.notifyPass();
    }));
  }));
});

chrome.test.sendMessage('ready');
