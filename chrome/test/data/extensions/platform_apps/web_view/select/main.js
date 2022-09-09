// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function CreateWebViewAndGuest() {
  var webview = document.createElement('webview');
  webview.allowtransparency = true;
  webview.allowscaling = true;
  var onLoadStop = function(e) {
    chrome.test.sendMessage('WebViewTest.LAUNCHED');
    webview.removeEventListener('loadstop', onLoadStop);
    webview.removeEventListener('loadabort', onLoadAbort);
  };
  webview.addEventListener('loadstop', onLoadStop);

  var onLoadAbort = function(e) {
    chrome.test.sendMessage('WebViewTest.FAILURE');
    webview.removeEventListener('loadstop', onLoadStop);
    webview.removeEventListener('loadabort', onLoadAbort);
  };

  webview.src = 'data:text/html,<!DOCTYPE html>\n' +
      '<style>\n' +
      'select {\n' +
      '  position: absolute;\n' +
      '  top: 9px;\n' +
      '  left: 9px;\n' +
      '  height: 25px;\n' +
      '  width: 80px;\n' +
      '}\n' +
      '</style>\n' +
      '<html>\n' +
      '  <body>\n' +
      '    <select>\n' +
      '      <option selected>Apple</option>\n' +
      '      <option>Orange</option>\n' +
      '      <option>Banana</option>\n' +
      '    </select>\n' +
      '  </body>\n' +
      '</html>\n';

  return webview;
}

onload = function() {
  var webview = CreateWebViewAndGuest();
  document.body.appendChild(webview);
};
