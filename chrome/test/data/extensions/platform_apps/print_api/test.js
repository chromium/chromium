// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.getConfig(function(config) {
    var options = {innerBounds: {width: 200, height: 100}};
    chrome.app.window.create('test.html', options, function(appWindow) {
      appWindow.contentWindow.onload = function() {
        appWindow.contentWindow.print();
        chrome.test.notifyPass();
      };
    });
  });
});
