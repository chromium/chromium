// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var baseUrl = 'http://example.com:' + config.testServer.port;
  chrome.test.runTests([async function makeRequest() {
    let url = `${baseUrl}/extensions/test_file.txt`;
    let response = await fetch(url);
    let text = await response.text();
    chrome.test.assertEq('Hello!', text);
    var baseUrl_websocket = 'ws://example.com:' + config.testServer.port;
    let socket = new WebSocket(baseUrl_websocket);
    socket.close();
    chrome.test.succeed();
  }]);
});
