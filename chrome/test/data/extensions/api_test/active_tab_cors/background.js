// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.browserAction.onClicked.addListener(function(tab) {
  const script = `
fetch(location.href).then((res) => {
  chrome.runtime.sendMessage('', res.status === 200 ? 'PASS' : 'FAIL');
}, () => {
  chrome.runtime.sendMessage('', 'FAIL');
});
`;

  chrome.runtime.onMessage.addListener(function listener(message) {
    chrome.runtime.onMessage.removeListener(listener);
    chrome.test.assertEq('PASS', message);
    chrome.test.notifyPass();
  });
  chrome.tabs.executeScript({code: script});
});

chrome.test.sendMessage('ready');
