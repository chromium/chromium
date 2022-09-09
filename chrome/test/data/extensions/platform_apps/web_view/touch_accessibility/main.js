// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function CreateWebViewAndGuest(callback) {
  var webview = document.createElement('webview');
  var onLoadStop = function(e) {
    chrome.test.sendMessage('WebViewTest.LAUNCHED');
    webview.removeEventListener('loadstop', onLoadStop);
    webview.removeEventListener('loadabort', onLoadAbort);
    callback();
  };
  webview.addEventListener('loadstop', onLoadStop);

  var onLoadAbort = function(e) {
    chrome.test.sendMessage('WebViewTest.FAILURE');
    webview.removeEventListener('loadstop', onLoadStop);
    webview.removeEventListener('loadabort', onLoadAbort);
  };
  webview.src = 'data:text/html,' +
      '<html>' +
      '<head>' +
      '  <style>' +
      '    body {' +
      '      border: 0;' +
      '      margin: 0;' +
      '      padding: 0;' +
      '    }' +
      '    button {' +
      '      border: 0;' +
      '      margin: 0;' +
      '      padding: 0;' +
      '      width: 200px;' +
      '      height: 100px;' +
      '    }' +
      '  </style>' +
      '</head>' +
      '<body>' +
      '<button>Guest button</button>'
      '</body>'
      '</html>';
  return webview;
}

onload = function() {
  var webview = CreateWebViewAndGuest(function() {
    webview.addEventListener('newwindow', function(e) {
      var newwebview = document.createElement('webview');
      newwebview.addEventListener('loadstop', function(e) {
        chrome.test.sendMessage('WebViewTest.NEWWINDOW');
      });
      e.window.attach(newwebview);
      document.body.appendChild(newwebview);
    });

    webview.addEventListener('loadstop', function(e) {
      chrome.test.sendMessage('WebViewTest.LOADSTOP');
    });
  });
  document.body.appendChild(webview);
};
