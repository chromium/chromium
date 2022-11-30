// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var appUrl = 'http://localhost:' + config.testServer.port +
        '/extensions/api_test/app_process/path1/empty.html';
  var redirectUrl = 'http://localhost:' + config.testServer.port +
      '/server-redirect?' + appUrl;
  chrome.tabs.create({
    url: redirectUrl
  });
});
