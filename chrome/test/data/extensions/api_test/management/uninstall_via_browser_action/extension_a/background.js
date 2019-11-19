// Copyright 2019 The Chromium Authors. All rights reserved.
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
    chrome.test.assertTrue(extension != undefined);
    chrome.test.assertTrue(extension.id != undefined);
    chrome.management.uninstall(extension.id, chrome.test.callbackPass(() => {
      chrome.test.notifyPass();
    }));
  }));
});

chrome.test.sendMessage('ready');
