// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getRootURL(port) {
  return new URL('http://127.0.0.1:' + port +
      '/extensions/platform_apps/web_view/download_cookie_isolation/');
}

// Creates a new WebView using |props| and returns a Promise that resolves once
// the 'loadstop' event is observed for the WebView.
function createWebView(props) {
  var webview = document.createElement('webview');
  webview.id = props.id;
  webview.partition = props.partition;
  webview.addEventListener('permissionrequest', function(e) {
    console.log("Permission request for " + e.permission);
    if (e.permission === 'download') {
      e.request.allow();
    };
  });
  webview.addEventListener('consolemessage', function(e) {
    console.log(props.id + " : " + e.message);
  });
  document.getElementById('container').appendChild(webview);

  return new Promise(function(accept, reject) {
    webview.addEventListener('loadstop', function() {
      console.log('loadstop received with src=' + webview.src);
      accept(true);
    });
    webview.src = props.url.href;
  });
}

// Creates the WebView elements and returns a promise that resolves once all
// webviews have finished loading.
function createWebViews(rootUrl) {
  var webviews = [];

  return Promise.all([
      createWebView({
        url: new URL('guest.html#cookie=first', rootUrl),
        id: 'first',
        partition: 'persist:p'
      }),
      createWebView({
        url: new URL('guest.html#cookie=second', rootUrl),
        id: 'second',
        partition: 'q'
      })
  ]);
}

// Called from test runner. Sends a message to the contained WebView to
// initiate a download.
function startDownload(id, url) {
  console.log("Received download for " + url + " on " + id);
  var webview = document.getElementById(id);
  webview.contentWindow.postMessage({
    command: 'start-download',
    url: url
  }, (new URL(webview.src)).origin);
}

function run() {
  var p = new Promise(function(accept, reject) {
    chrome.test.getConfig(function(config) {
      accept(config);
    });
  }).then(function(config) {
    return createWebViews(getRootURL(config.testServer.port));
  }).then(function() {
    chrome.test.sendMessage('created-webviews');
  });
}
window.onload = run;
