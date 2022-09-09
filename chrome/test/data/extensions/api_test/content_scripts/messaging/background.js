// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var testUrl =
      `http://localhost:${config.testServer.port}/extensions/test_file.html`;
  chrome.tabs.create({url: testUrl});
});
