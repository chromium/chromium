// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.addEventListener('load', function() {
  window.webview = document.createElement('webview');
  document.querySelector('#webview-tag-container').appendChild(webview);

  webview.style.width = "100%";
  webview.style.height = "100%";
  webview.addEventListener('loadstop', function() {
    webview.contentWindow.postMessage({}, '*');
  });

  chrome.test.getConfig(function(config) {
    var url = 'http://localhost:' + config.testServer.port +
        '/extensions/platform_apps/web_view/ime/guest.html';
    webview.src = url;
  });
});

window.addEventListener('message', function(e) {
  if (e.data.type === 'init') {
    chrome.test.sendMessage('WebViewImeTest.Launched');
  } else if (e.data.type === 'focus') {
    chrome.test.sendMessage('WebViewImeTest.InputFocused')
  } else if (e.data.type === 'input') {
    chrome.test.sendMessage('WebViewImetest.InputReceived')
  }
});

