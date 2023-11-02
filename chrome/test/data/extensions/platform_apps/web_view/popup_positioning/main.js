// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(var_args) {
  window.console.log.apply(window.console, arguments);
};

window.runTest = function(testName) {
  LOG('window.runTest: ' + testName);
  if (testName == 'testBasic') {
    testBasic();
  } else if (testName == 'testMoved') {
    testMoved();
  }
};

var testHelper = function(onLoadStopHook) {
  var webview = document.querySelector('webview');
  var loaded = false;
  webview.addEventListener('loadstop', function(e) {
    LOG('webview.loadstop');
    if (!loaded) {
      loaded = true;
      webview.focus();
      onLoadStopHook(webview);
      var msg = 'request-connect';
      webview.contentWindow.postMessage(JSON.stringify([msg]), '*');
    }
  });
  window.addEventListener('message', function(e) {
    LOG('message');
    var data = JSON.parse(e.data);
    LOG('data:' + data);
    if (data[0] == 'response-connect') {
      chrome.test.sendMessage('TEST_PASSED');
    }
  });
  webview.addEventListener('consolemessage', function(e) {
    LOG('g:' + e.message);
  });

  webview.style.width = '300px';
  webview.style.height = '200px';
  webview.partition = 'popup-partition';
  webview.setAttribute('src', 'guest.html');
};

// Tests.
function testBasic() {
  LOG('testBasic');
  testHelper(function(webview) {});
}

function testMoved() {
  LOG('testMoved');
  testHelper(function(webview) {
    // We move the <webview> in a way, this would trigger no resize
    // but would require popups to render in a different place.
    webview.style.paddingLeft = '20px';
  });
}

onload = function() {
  chrome.test.getConfig(function(config) {
    chrome.test.sendMessage('Launched');
  });
};
