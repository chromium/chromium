// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var guestUrl = 'http://localhost:' + config.testServer.port +
      '/extensions/platform_apps/web_view/autoplay/guest.html';

  var webview = document.querySelector('webview');
  webview.addEventListener('loadstop', function() {
    webview.onconsolemessage = function(e) {
      chrome.test.assertEq('NotAllowedError', e.message);
      chrome.test.succeed();
    };

    webview.contentWindow.postMessage(JSON.stringify('start'), '*');
  }, { once: true });

  webview.src = guestUrl;
});
