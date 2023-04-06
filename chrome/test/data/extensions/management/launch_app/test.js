// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kChromeAppErrorPrefix = 'Chrome app';
const kChromeAppErrorSuffix = 'is deprecated on Window, Mac, and Linux. ' +
    'See https://support.google.com/chrome/?p=chrome_app_deprecation for more ' +
    'info';

// This extension is used by the management_api_browsertest.cc file to launch
// specific extensions that it installs. This usually involves checking if the
// launch of the extension succeeds or not. The 'packaged_app' extension emits
// an 'app_launched` message when launched.

chrome.management.getAll(function(items) {
  for (var i in items) {
    var item = items[i];
    if (item.name == 'packaged_app') {
      chrome.management.launchApp(item.id, function() {
        const lastError = chrome.runtime.lastError;
        if (lastError && lastError.message.startsWith(kChromeAppErrorPrefix) &&
            lastError.message.endsWith(kChromeAppErrorSuffix)) {
          chrome.test.sendMessage('got_chrome_apps_error');
        }
      });
    }
    if (item.name == 'simple_extension') {
      // Try launching a non-app extension, which should fail.
      var expected_error = 'Extension ' + item.id + ' is not an App.';
      chrome.management.launchApp(item.id, function() {
        if (chrome.runtime.lastError &&
            chrome.runtime.lastError.message == expected_error) {
          chrome.test.sendMessage('got_expected_error');
        }
      });
    }
  }
});
