// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) {
    window.console.log(msg);
};

var startTest = function() {
  var webview = document.createElement('webview');
  var onLoadStop = function(e) {
    webview.contentWindow.postMessage(JSON.stringify(['connect']),'*');
  };

  webview.addEventListener('loadstop', onLoadStop);
  webview.addEventListener('consolemessage', function(e) {
    LOG('g: ' + e.message);
  });
  webview.partition = 'partition1';
  webview.style.width = '300px';
  webview.style.height = '200px';
  webview.style.margin = '0px';
  webview.style.padding = '0px';
  webview.style.position = 'absolute';
  webview.style.left = '50px';
  webview.style.top = '100px';
  webview.src = 'guest.html';
  document.querySelector('#webview-tag-container').appendChild(webview);
};

window.addEventListener('message', function(e) {
  var data = JSON.parse(e.data);
  LOG('data: ' + data);
    switch (data[0]) {
      case 'connected':
        chrome.test.sendMessage('WebViewTest.LAUNCHED');
        break;
      case 'overflow_is_hidden':
        chrome.test.sendMessage('overflow_is_hidden');
        break;
    }
});

if (chrome.test !== undefined) {
  chrome.test.getConfig(function(config) {
    startTest();
  });
} else {
  // Allow interactive debugging.
  // Need timeout to allow the WebView prototype to be setup.
  window.setTimeout(startTest, 100);
}
