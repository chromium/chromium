// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testAutoSize() {
  var webview = document.querySelector('webview');

  var minw = 1600;
  var maxw = 3000;
  var minh = 1600;
  var maxh = 3000;

  webview.addEventListener('loadstop', function(e) {
    window.console.log('guest.loadstop');
    // Note that we set the autosize attribute first to check incorrect
    // damage buffer allocation.
    //
    // Consider a case where:
    // Initially, <webview>.width = 300 (default width).
    // Then we set
    // <webview>.autosize = true
    // And then we set
    // <webview>.minwidth = 200
    // and
    // <webview>.maxwidth = 800
    // If the autosize logic decided to set the width to 400 (something > 300),
    // then we won't have enough memory in damage buffer to render.
    // When tried manually, this would cause garbage to be rendered (around the
    // bottom portion of plugin). In test we set the value to high so increase
    // the chance of invalid memory access.
    webview.autosize = true;

    // Ask for a larger size so we can expose memory access bug.
    webview.maxwidth = maxw;
    webview.maxheight = maxh;
    webview.minwidth = minw;
    webview.minheight = minh;
  });

  webview.addEventListener('sizechanged', function(e) {
    window.console.log('sizechanged: dimension: ' + e.newWidth + ' X ' +
                       e.newHeight);
    chrome.test.assertTrue(e.newWidth >= minw);
    chrome.test.assertTrue(e.newWidth <= maxw);
    chrome.test.assertTrue(e.newHeight >= minh);
    chrome.test.assertTrue(e.newHeight <= maxh);

    chrome.test.succeed();
  });

  var longString = 'a b c d e f g h i j k l m o p q r s t u v w x y z';
  for (var i = 0; i < 4; ++i) {
    longString += longString;
  }
  webview.setAttribute('src',
      'data:text/html,<body bgcolor="red">' +
      '<div style="width: 400px; height: 200px;">' + longString + '</div>' +
      longString + '</body>');
}

onload = function() {
  chrome.test.runTests([testAutoSize]);
};
