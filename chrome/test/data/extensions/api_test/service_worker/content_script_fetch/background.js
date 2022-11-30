// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener(function(port) {
  chrome.test.log('got connect');
  port.onMessage.addListener(function(msg) {
    chrome.test.log('got message: ' + msg);
    chrome.test.assertEq('Success', msg);
    chrome.test.notifyPass();
  });
});

chrome.test.getConfig(function(config) {
  chrome.test.log('Creating tab...');
  chrome.tabs.create({
    url: 'http://127.0.0.1:PORT/extensions/api_test/service_worker/content_script_fetch/controlled_page/index.html'
             .replace(/PORT/, config.testServer.port)
  });
});
