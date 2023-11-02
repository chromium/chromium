// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.test = {};
embedder.triggerNavUrl =
    'data:text/html,<html><body>trigger navigation<body></html>';

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

/** @private */
embedder.setUpGuest_ = function(partitionName) {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"></webview>';
  var webview = document.querySelector('webview');
  if (partitionName) {
    webview.partition = partitionName;
  }
  if (!webview) {
    embedder.test.fail('No <webview> element created');
  }
  return webview;
};

embedder.test = {};
embedder.test.succeed = function() {
  chrome.test.sendMessage('TEST_PASSED');
};

embedder.test.fail = function() {
  chrome.test.sendMessage('TEST_FAILED');
};

embedder.test.assertEq = function(a, b) {
  if (a != b) {
    console.log('assertion failed: ' + a + ' != ' + b);
    embedder.test.fail();
  }
};

embedder.test.assertTrue = function(condition) {
  if (!condition) {
    console.log('assertion failed: true != ' + condition);
    embedder.test.fail();
  }
};

embedder.test.assertFalse = function(condition) {
  if (condition) {
    console.log('assertion failed: false != ' + condition);
    embedder.test.fail();
  }
};

// Tests begin.
// This test verifies that a webview loses pointer lock when it loses focus.
function testPointerLockLostWithFocus() {
  var webview1 = document.createElement('webview');
  var webview2 = document.createElement('webview');

  // Wait until both webviews finish loading.
  var loadstopCount = 0;
  var onLoadStop = function(e) {
    if (++loadstopCount < 2) {
      return;
    }
    console.log('webview1 and webview2 have loaded.');
    webview1.focus();
    webview1.executeScript(
      {file: 'inject_pointer_lock.js'},
      function(results) {
        console.log('Injected script into webview1.');
        // Establish a communication channel with the webview1's guest.
        var msg = ['connect'];
        webview1.contentWindow.postMessage(JSON.stringify(msg), '*');
      });
  };
  webview1.addEventListener('loadstop', onLoadStop);
  webview2.addEventListener('loadstop', onLoadStop);

  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == 'connected') {
      console.log('Established communication channel with webview1.');
      // Once a communication channel has been established, we start the test.
      var msg = ['start-pointerlock'];
      webview1.contentWindow.postMessage(JSON.stringify(msg), '*');
      return;
    }

    if (data[0] == 'acquired-pointerlock') {
      console.log('webview1 has successfully acquired pointer lock.');
      webview2.focus();
      console.log('webview2 has taken focus.');
      return;
    }

    embedder.test.assertEq('lost-pointerlock', data[0]);
    console.log('webview1 has lost pointer lock.');
    embedder.test.succeed();
  });

  webview1.addEventListener('permissionrequest', function(e) {
    console.log('webview1 has requested pointer lock.');
    e.request.allow();
    console.log('webview1 has been granted pointer lock.');
  });

  webview1.setAttribute('src', embedder.triggerNavUrl);
  webview2.setAttribute('src', embedder.triggerNavUrl);
  document.body.appendChild(webview1);
  document.body.appendChild(webview2);
}

embedder.test.testList = {
  'testPointerLockLostWithFocus': testPointerLockLostWithFocus,
};

onload = function() {
  chrome.test.getConfig(function(config) {
    chrome.test.sendMessage('Launched');
  });
};
