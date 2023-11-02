// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.test = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';

window.runTest = function(testName) {
  if (!embedder.test.testList[testName]) {
    console.log('Incorrect testName: ' + testName);
    embedder.test.fail();
    return;
  }

  // Run the test.
  embedder.test.testList[testName]();
};
// window.* exported functions end.


embedder.test.succeed = function() {
  chrome.test.sendMessage('TEST_PASSED');
};

embedder.test.fail = function() {
  chrome.test.sendMessage('TEST_FAILED');
};

embedder.setUp = function(config) {
  embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
  embedder.guestURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/content_script_fetch/guest.html';
};

/** @private */
embedder.setUpGuest_ = function() {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"></webview>';
  var webview = document.querySelector('webview');
  if (!webview) {
    console.log('No <webview> element created');
    embedder.test.fail();
  }
  return webview;
};

// Tests begin.

function testContentScriptFetch() {
  var webview = embedder.setUpGuest_();
  webview.addContentScripts([{
      name: 'rule',
      matches: ['http://localhost/*'],
      js: { files: ['content_script.js'] },
      run_at: 'document_start'}]);

  webview.addEventListener('loadstop', function() {
    var msg = ['start-fetch'];
    webview.contentWindow.postMessage(JSON.stringify(msg), '*');
  });

  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    if (data == 'fetch-success') {
      embedder.test.succeed();
      return;
    }
    console.log('Unexpected message: \'' + data[0]  + '\'');
    embedder.test.fail();
  });

  webview.src = embedder.guestURL;
}

embedder.test.testList = {
  testContentScriptFetch: testContentScriptFetch
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp(config);
    chrome.test.sendMessage('Launched');
  });
};
