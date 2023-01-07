// Copyright 2014 The Chromium Authors
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


/** @private */
embedder.setUp_ = function(config) {
  embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
  embedder.guestURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/filesystem/main' +
      '/guest_main.html';
  chrome.test.log('Guest url is: ' + embedder.guestURL);
};

/** @private */
embedder.setUpGuest_ = function() {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"' +
      ' src="' + embedder.guestURL + '"' +
      ' partition = "persist:a"' +
      '></webview>';
  var webview = document.querySelector('webview');
  if (!webview) {
    console.log('No <webview> element created');
    embedder.test.fail();
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

/** @private */
embedder.setUpLoadStop_ = function(webview, testName) {
  window.console.log('embedder.setUpLoadStop_');
  var onWebViewLoadStop = function(e) {
    window.console.log('embedder.onWebViewLoadStop');
    // Send post message to <webview> when it's ready to receive them.
    var msgArray = ['check-filesystem-permission', '' + testName];
    window.console.log('embedder.webview.postMessage');
    webview.contentWindow.postMessage(JSON.stringify(msgArray), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
};

/** @private */
embedder.registerAndWaitForPostMessage_ = function(
    webview, expectedData) {
  var testName = expectedData[0];
  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == '' + testName) {
      embedder.test.assertEq(expectedData.length, data.length);
      for (var i = 0; i < expectedData.length; ++i) {
        embedder.test.assertEq(expectedData[i], data[i]);
      }
      embedder.test.succeed();
    }
  };
  window.addEventListener('message', onPostMessageReceived);
};

/** @private */
embedder.assertCorrectEvent_ = function(e) {
  embedder.test.assertEq('filesystem', e.permission);
  embedder.test.assertTrue(!!e.url);
  embedder.test.assertEq(e.url, e.request.url);
  embedder.test.assertTrue(e.url.indexOf(embedder.baseGuestURL) == 0);
};

// Tests begin.

// Once the guest is allowed or denied filesystem, the guest notifies the
// embedder about the fact via post message.
// The embedder has to initiate a post message so that the guest can get a
// reference to embedder to send the reply back.

// Loads a guest which requests filesystem. The embedder explicitly
// allows acccess to filesystem for the guest.
function testAllow() {
  var webview = embedder.setUpGuest_();

  var onPermissionRequest = function(e) {
    chrome.test.log('Embedder notified on permissionRequest');
    embedder.assertCorrectEvent_(e);
    e.request.allow();
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadStop_(webview, 'test1');
  embedder.registerAndWaitForPostMessage_(
      webview, ['test1', 'access-granted']);
}

// Loads a guest which requests filesystem. The embedder explicitly
// denies access to filesystem for the guest.
function testDeny() {
  var webview = embedder.setUpGuest_();

  var onPermissionRequest = function(e) {
    chrome.test.log('Embedder notified on permissionRequest');
    embedder.assertCorrectEvent_(e);
    e.request.deny();
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadStop_(webview, 'test2');
  embedder.registerAndWaitForPostMessage_(
      webview, ['test2', 'access-denied']);
}

// Loads a guest which requests filesystem. The embedder does not
// perform an explicit action, and the default permission according
// to cookie setting is allowed.
function testDefaultAllow() {
  var webview = embedder.setUpGuest_();

  var onPermissionRequest = function(e) {
    embedder.assertCorrectEvent_(e);
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadStop_(webview, 'test3');
  embedder.registerAndWaitForPostMessage_(
      webview, ['test3', 'access-granted']);
}

embedder.test.testList = {
  'testAllow': testAllow,
  'testDeny': testDeny,
  'testDefaultAllow': testDefaultAllow
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp_(config);
    chrome.test.sendMessage('Launched');
  });
};
