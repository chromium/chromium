// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.storage.local.get('sentinel', (val) => {
  if (val.sentinel !== undefined) {
    // Note: We don't expect the event page to be invoked a second time,
    // because it doesn't register for any relevant events. If it were, it would
    // re-set the action properties, which would invalidate the text.
    chrome.test.notifyFail('Unexpected Sentinel Value: ' + val.sentinel);
    chrome.browserAction.setTitle({title: 'FAILED'});
    return;
  }

  chrome.storage.local.set({sentinel: true}, () => {
    chrome.browserAction.setPopup({popup: 'modified_popup.html'}, () => {
      chrome.test.assertNoLastError();
      chrome.browserAction.setTitle({title: 'modified title'}, () => {
        chrome.test.assertNoLastError();
        chrome.browserAction.setBadgeText({text: 'custom badge text'}, () => {
          chrome.test.assertNoLastError();
          chrome.test.notifyPass();
        });
      });
    });
  });
});
