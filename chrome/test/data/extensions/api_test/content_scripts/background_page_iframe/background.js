// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var iframe = document.createElement('iframe');
  iframe.src =
      'http://a.com:' + config.testServer.port + '/extensions/test_file.html';
  iframe.onload = function() { chrome.test.sendMessage('iframe loaded'); }
  document.body.appendChild(iframe);
});
