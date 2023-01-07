// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.test = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';
embedder.guestNumber = 3;
var actualReceivedMessage = 0;

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
      '/extensions/platform_apps/web_view/filesystem/shared_worker/multiple' +
      '/guest_shared_worker.html';
  chrome.test.log('Guest url is: ' + embedder.guestURL);
};

/** @private */
embedder.setUpGuest_ = function() {
  var webview = [];
  var container = document.querySelector('#webview-tag-container');
  for (var i = 0;  i < embedder.guestNumber; ++i) {
    webview[i] = document.createElement('webview');
    webview[i].style.width = '100px';
    webview[i].style.height = '100px';
    webview[i].setAttribute('partition', "persist:a");
    container.appendChild(webview[i]);
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
embedder.setUpLoadStop_ = function(webview, testName, id) {
  var onWebViewLoadStop = function(e) {
    window.console.log('embedder.onWebViewLoadStop of guest No.' + id);
    // Send post message to <webview> when it's ready to receive them.
    var msgArray = ['check-filesystem-permission', '' + testName];
    window.console.log('embedder.webview[' + id + '].postMessage');
    webview.contentWindow.postMessage(JSON.stringify(msgArray), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
};

/** @private */
embedder.registerAndWaitForPostMessage_ = function(
    webview, expectedData) {
  var testName = expectedData[0];
  var expectedMessageNumber = embedder.guestNumber;

  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == '' + testName)
      actualReceivedMessage ++;
      embedder.test.assertEq(expectedData.length, data.length);
      for (var i = 0; i < expectedData.length; ++i) {
        embedder.test.assertEq(expectedData[i], data[i]);
      }
      if (actualReceivedMessage == expectedMessageNumber)
        embedder.test.succeed();
  };
  window.addEventListener('message', onPostMessageReceived);
};

/** @private */
embedder.assertCorrectEvent_ = function(e) {
  embedder.test.assertEq('filesystem', e.permission);
  embedder.test.assertTrue(!!e.url);
  embedder.test.assertTrue(e.url.indexOf(embedder.baseGuestURL) == 0);
};

// Tests begin.

// Once the guest is allowed or denied filesystem, the guest notifies the
// embedder about the fact via post message.
// The embedder has to initiate a post message so that the guest can get a
// reference to embedder to send the reply back.

// Loads multiple guests each of which creates a shared worker to request
// filesystem. The embedder explicitly allows acccess to filesystem for guests.
function testAllow() {
  var webview = embedder.setUpGuest_();

  var onPermissionRequest = function(e) {
    chrome.test.log('Embedder notified on permissionRequest');
    embedder.assertCorrectEvent_(e);
    e.request.allow();
  };
  for(var i = 0; i < embedder.guestNumber; ++i) {
    webview[i].addEventListener('permissionrequest', onPermissionRequest);
    embedder.setUpLoadStop_(webview[i], 'test1', i);
    webview[i].setAttribute('src', embedder.guestURL);
  }
  actualReceivedMessage = 0;
  embedder.registerAndWaitForPostMessage_(
      webview, ['test1', 'access-granted']);
}

// Loads multiple guests each of which creates a shared worker to request
// filesystem. The embedder explicitly denies access to filesystem for
// guests.
function testDeny() {
  var webview = embedder.setUpGuest_();

  var onPermissionRequest = function(e) {
    chrome.test.log('Embedder notified on permissionRequest');
    embedder.assertCorrectEvent_(e);
    e.request.deny();
  };
  for (var i = 0; i < embedder.guestNumber; ++i) {
    webview[i].addEventListener('permissionrequest', onPermissionRequest);
    embedder.setUpLoadStop_(webview[i], 'test2', i);
    webview[i].setAttribute('src', embedder.guestURL);
  }
  actualReceivedMessage = 0;
  embedder.registerAndWaitForPostMessage_(
      webview, ['test2', 'access-denied']);
}

// Loads multiple guests each of which creates a shared worker to request
// filesystem. The embedder does not perform an explicit action, and the
// default permission according to cookie setting is allowed.
function testDefaultAllow() {
  var webview = embedder.setUpGuest_();

  var onPermissionRequest = function(e) {
    embedder.assertCorrectEvent_(e);
  };
  for (var i = 0; i < embedder. guestNumber; ++i) {
    webview[i].addEventListener('permissionrequest', onPermissionRequest);
    embedder.setUpLoadStop_(webview[i], 'test3', i);
    webview[i].setAttribute('src', embedder.guestURL);
  }
  actualReceivedMessage = 0;
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
