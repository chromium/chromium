// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  var port = location.search.slice(1);
  setTimeout(function() {
    var f = document.createElement('iframe');
    window.onmessage = function(event) {
      chrome.test.assertEq(f.contentWindow, event.source);
      chrome.test.assertEq('http://a.com:' + port, event.origin);
      chrome.test.assertEq('a.com: go to b.com', event.data);
      f.src = f.src.replace('a.com', 'b.com');
    };
    f.src = 'http://a.com:' + port +
        '/extensions/api_test/webnavigation/crossProcessIframe/frame.html';
    document.body.appendChild(f);
  }, 0);
};
