// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// |baseUrl| will be equivalent to 'http://example.com:PORT';
var baseUrl = location.ancestorOrigins[0];

var x = new XMLHttpRequest();
x.open('GET', baseUrl + '/extensions/test_file.txt?framescript');
x.onloadend = function() {
  // Sanity check.
  chrome.test.assertEq('Hello!', x.responseText);

  chrome.test.sendMessage('framescript_done');
};
x.send();
