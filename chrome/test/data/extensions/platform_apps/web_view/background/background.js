// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;

var WEBVIEW_SRC = "data:text/html,<body>One</body>";

chrome.test.runTests([
  // Tests that embedding <webview> inside a background page loads.
  function inDOM() {
    var webview = document.querySelector('webview');
    webview.addEventListener('contentload', pass());
    webview.setAttribute('src', WEBVIEW_SRC);
  },

  // Tests that creating and attaching a WebView element inside a background
  // page loads.
  function newWebView() {
    var webview = new WebView();
    webview.addEventListener('contentload', pass());
    webview.src = WEBVIEW_SRC;
    document.body.appendChild(webview);
  },
  // Tests that requests from <webview> that require auth are cancelled properly
  // and there is no crash.
  function webViewResourceNeedsAuth() {
    chrome.test.getConfig(function(config) {
      var port = config.testServer.port;
      var url = 'http://localhost:' + port +
          '/extensions/platform_apps/web_view/background/webview_auth.html';
      var authUrl = 'http://localhost:' + port + '/auth-basic';
      var webview = document.createElement('webview');
      webview.request.onCompleted.addListener(function(details) {
        if (authUrl == details.url) {
          chrome.test.assertEq(401, details.statusCode);
          chrome.test.succeed();
        }
      }, {urls: [authUrl]});
      webview.onloadstop = function(e) {
        webview.contentWindow.postMessage({request: 'xhr', url: authUrl}, '*');
      };
      webview.setAttribute('src', url);
      document.body.appendChild(webview);
    });
  }
]);
