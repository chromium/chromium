// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function pageScript() {
  fetch(location.href)
      .then(
          (res) => {
            chrome.runtime.sendMessage(
                '', res.status === 200 ? 'PASS' : 'FAIL');
          },
          () => {
            chrome.runtime.sendMessage('', 'FAIL');
          });
}

chrome.action.onClicked.addListener(function(tab) {
  chrome.runtime.onMessage.addListener(function listener(message) {
    chrome.runtime.onMessage.removeListener(listener);
    chrome.test.assertEq('PASS', message);
    chrome.test.notifyPass();
  });
  chrome.scripting.executeScript({target: {tabId: tab.id}, func: pageScript});
});

chrome.test.sendMessage('ready');
