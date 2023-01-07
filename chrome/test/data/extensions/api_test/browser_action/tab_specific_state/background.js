// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var min = 1;
var max = 5;
var current = min;

// Called when the user clicks on the browser action.
chrome.browserAction.onClicked.addListener(function(tab) {
  current++;
  if (current > max)
    current = min;

  chrome.browserAction.setIcon({
    path: "icon" + current + ".png",
    tabId: tab.id
  });
  chrome.browserAction.setTitle({
    title: "Showing icon " + current,
    tabId: tab.id
  });
  chrome.browserAction.setBadgeText({
    text: String(current),
    tabId: tab.id
  });
  chrome.browserAction.setBadgeBackgroundColor({
    color: [50*current,0,0,255],
    tabId: tab.id
  });

  // Test that callbacks work as expected.
  chrome.browserAction.setIcon({
    imageData: new ImageData(1, 1),
    tabId: 133713371,
  }, function() {
    chrome.test.assertLastError("No tab with id: 133713371.");

    chrome.browserAction.setTitle({
      title: "Ignore because of invalid tabId",
      tabId: 133713372,
    }, function() {
      chrome.test.assertLastError("No tab with id: 133713372.");

      chrome.browserAction.setBadgeText({
        text: "Ignore because of invalid tabId",
        tabId: 133713373,
      }, function() {
        chrome.test.assertLastError("No tab with id: 133713373.");

        chrome.browserAction.setBadgeBackgroundColor({
          color: [12, 34, 56, 78],
          tabId: 133713374,
        }, function() {
          chrome.test.assertLastError("No tab with id: 133713374.");

          chrome.browserAction.enable(133713375, function() {
            chrome.test.assertLastError("No tab with id: 133713375.");

            chrome.browserAction.disable(133713376, function() {
              chrome.test.assertLastError("No tab with id: 133713376.");

              chrome.test.notifyPass();
            });
          });
        });
      });
    });
  });
});

chrome.test.notifyPass();
