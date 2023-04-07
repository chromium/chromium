// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kChromeAppErrorPrefix = 'Chrome app';
const kChromeAppErrorSuffix = 'is deprecated on Window, Mac, and Linux. ' +
    'See https://support.google.com/chrome/?p=chrome_app_deprecation for more ' +
    'info';

// This extension is used by the management_api_browsertest.cc to launch the
// 'packaged_app' extension from the background, and checks to see if that
// succeeded or not.

chrome.management.getAll(function(items) {
  for (var i in items) {
    var item = items[i];
    if (item.name == 'packaged_app') {
      launchFromBackground(item.id);
      break;
    }
  }
});

function launchFromBackground(appId) {
  // Create a new 'popup' window so the last active window isn't 'normal'.
  chrome.windows.create({url: 'about:blank', type: 'popup'}, function(win) {
    chrome.management.launchApp(appId, function() {
      const lastError = chrome.runtime.lastError;
      if (lastError && lastError.message.startsWith(kChromeAppErrorPrefix) &&
          lastError.message.endsWith(kChromeAppErrorSuffix)) {
        chrome.test.sendMessage('got_chrome_apps_error');
      }
      chrome.windows.getAll({populate: true}, function(wins) {
        if (wins.length != 2)
          return;

        // This test passes if the 'popup' window still has only 1 tab,
        // and if the 'normal' window now has 2 tabs. (The app tab was
        // added to the 'normal' window even if it wasn't focused.)
        for (var x = 0; x < wins.length; x++) {
          var w = wins[x];
          if (w.id == win.id) {
            if (w.tabs.length > 1)
              return;
            if (w.tabs[0].url != 'about:blank')
              return;

          } else if (w.type == 'normal') {
            if (w.tabs.length == 2) {
              chrome.test.sendMessage('success');
            } else {
              chrome.test.sendMessage('not_launched');
            }
          }
        }
      });
    });
  });
}
