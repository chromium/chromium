// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// location.origin will be equivalent to 'http://example.com:PORT';
var baseUrl = location.origin;

chrome.test.sendMessage('contentscript_ready', function() {
  var x = new XMLHttpRequest();
  x.open('GET', baseUrl + '/extensions/test_file.txt?contentscript');
  x.onloadend = function() {
    chrome.test.assertEq('Hello!', x.responseText);

    chrome.test.sendMessage('contentscript_done', function() {
      var frame = document.createElement('iframe');
      frame.src = chrome.runtime.getURL('frame.html');
      document.body.appendChild(frame);
    });
  };
  x.send();
});
