// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.browserAction.onClicked.addListener(() => {
  chrome.test.assertEq(undefined, chrome.storage);
  chrome.permissions.request(
      {permissions:['storage']},
      (granted) => {
        chrome.test.assertNoLastError();
        chrome.test.assertTrue(granted);
        chrome.test.assertTrue(!!chrome.storage);
        chrome.permissions.contains({permissions: ['storage']}, (result) => {
          chrome.test.assertNoLastError();
          chrome.test.assertTrue(result);
          chrome.test.notifyPass();
        });
      });
});

chrome.test.sendMessage('ready');
