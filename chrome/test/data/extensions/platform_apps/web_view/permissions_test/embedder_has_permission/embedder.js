// Copyright 2022 The Chromium Authors
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
      '/extensions/platform_apps/web_view/permissions_test' +
      '/permissions_access_guest.html';
  chrome.test.log('Guest url is: ' + embedder.guestURL);
};

/** @private */
embedder.setUpGuest_ = function() {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"' +
      ' src="' + embedder.guestURL + '"' +
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

/** @private */
embedder.setUpLoadStop_ = function(webview, testName) {
  window.console.log('embedder.setUpLoadStop_');
  var onWebViewLoadStop = function(e) {
    window.console.log('embedder.onWebViewLoadStop');
    // User activation is required to use the `requestDevice` method of HID API.
    // The guest script doesn't have access to `chrome.test.runWithUserGesture`,
    // so we have an ad hoc handler in the browser to do the activation.
    chrome.test.sendMessage('performUserActivationInWebview');
    // Send post message to <webview> when it's ready to receive them.
    var msgArray = ['check-permissions', '' + testName];
    window.console.log('embedder.webview.postMessage');
    webview.contentWindow.postMessage(JSON.stringify(msgArray), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
};

/** @private */
embedder.registerAndWaitForPostMessage_ = function(testName, expectedResult) {
  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == '' + testName) {
      if (data[1] == expectedResult) {
        embedder.test.succeed();
      } else {
        embedder.test.fail();
      }
    }
  };
  window.addEventListener('message', onPostMessageReceived);
};

// Tests begin.

// Once the guest is allowed or denied geolocation, the guest notifies the
// embedder about the fact via post message.
// The embedder has to initiate a post message so that the guest can get a
// reference to embedder to send the reply back.
//
// Loads a guest which requests geolocation. The embedder (platform app) has
// access to geolocation and allows geolocation for the guest.
function testAllowGeolocation() {
  navigator.permissions.query({name: 'geolocation'}).then(function(permission) {
    // TODO(crbug.com/40215363) Geolocation state is `prompt` even despite it is
    // declared in the manifest.
    if (permission.state === 'prompt') {
      var webview = embedder.setUpGuest_();

      var onPermissionRequest = function(e) {
        e.request.allow();
      };
      webview.addEventListener('permissionrequest', onPermissionRequest);

      embedder.setUpLoadStop_(webview, 'testGeolocation');
      embedder.registerAndWaitForPostMessage_(
          'testGeolocation', 'access-granted');
    } else {
      embedder.test.fail();
    }
  });
}

function testDenyGeolocation() {
  navigator.permissions.query({name: 'geolocation'}).then(function(permission) {
    // TODO(crbug.com/40215363) Geolocation state is `prompt` even despite it is
    // declared in the manifest.
    if (permission.state === 'prompt') {
      var webview = embedder.setUpGuest_();

      var onPermissionRequest = function(e) {
        e.request.deny();
      };
      webview.addEventListener('permissionrequest', onPermissionRequest);

      embedder.setUpLoadStop_(webview, 'testGeolocation');
      embedder.registerAndWaitForPostMessage_(
          'testGeolocation', 'access-denied');
    } else {
      embedder.test.fail();
    }
  });
}

function testAllowCamera() {
  navigator.permissions.query({name: 'camera'}).then(function(permission) {
    // TODO(crbug.com/40215363) Camera state is `prompt` even despite it is
    // declared in the manifest.
    if (permission.state === 'prompt') {
      var webview = embedder.setUpGuest_();
      var onPermissionRequest = function(e) {
        e.request.allow();
      };
      webview.addEventListener('permissionrequest', onPermissionRequest);

      embedder.setUpLoadStop_(webview, 'testCamera');
      embedder.registerAndWaitForPostMessage_('testCamera', 'access-granted');

    } else {
      embedder.test.fail();
    }
  });
}

function testDenyCamera() {
  navigator.permissions.query({name: 'camera'}).then(function(permission) {
    // TODO(crbug.com/40215363) Camera state is `prompt` even despite it is
    // declared in the manifest.
    if (permission.state === 'prompt') {
      var webview = embedder.setUpGuest_();
      var onPermissionRequest = function(e) {
        e.request.deny();
      };
      webview.addEventListener('permissionrequest', onPermissionRequest);

      embedder.setUpLoadStop_(webview, 'testCamera');
      embedder.registerAndWaitForPostMessage_('testCamera', 'access-denied');

    } else {
      console.log('Camera is not prompt. Fail');
      embedder.test.fail();
    }
  });
}

function testAllowMicrophone() {
  navigator.permissions.query({name: 'microphone'}).then(function(permission) {
    // TODO(crbug.com/40215363) Microphone state is `prompt` even despite it is
    // declared in the manifest.
    if (permission.state === 'prompt') {
      var webview = embedder.setUpGuest_();
      var onPermissionRequest = function(e) {
        e.request.allow();
      };
      webview.addEventListener('permissionrequest', onPermissionRequest);

      embedder.setUpLoadStop_(webview, 'testMicrophone');
      embedder.registerAndWaitForPostMessage_(
          'testMicrophone', 'access-granted');

    } else {
      embedder.test.fail();
    }
  });
}
function testDenyMicrophone() {
  navigator.permissions.query({name: 'microphone'}).then(function(permission) {
    // TODO(crbug.com/40215363) Microphone state is `prompt` even despite it is
    // declared in the manifest.
    if (permission.state === 'prompt') {
      var webview = embedder.setUpGuest_();
      var onPermissionRequest = function(e) {
        e.request.deny();
      };
      webview.addEventListener('permissionrequest', onPermissionRequest);

      embedder.setUpLoadStop_(webview, 'testMicrophone');
      embedder.registerAndWaitForPostMessage_(
          'testMicrophone', 'access-denied');

    } else {
      embedder.test.fail();
    }
  });
}

function testAllowMedia() {
  var webview = embedder.setUpGuest_();
  var onPermissionRequest = function(e) {
    e.request.allow();
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadStop_(webview, 'testMedia');
  embedder.registerAndWaitForPostMessage_('testMedia', 'access-granted');
}

function testDenyMedia() {
  var webview = embedder.setUpGuest_();
  var onPermissionRequest = function(e) {
    e.request.deny();
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadStop_(webview, 'testMedia');
  embedder.registerAndWaitForPostMessage_('testMedia', 'access-denied');
}

function testAllowHid() {
  const webview = embedder.setUpGuest_();
  const onPermissionRequest = function(e) {
    e.request.allow();
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadStop_(webview, 'testHid');
  embedder.registerAndWaitForPostMessage_('testHid', 'access-granted');
}

function testDenyHid() {
  var webview = embedder.setUpGuest_();
  var onPermissionRequest = function(e) {
    e.request.deny();
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadStop_(webview, 'testHid');
  embedder.registerAndWaitForPostMessage_('testHid', 'access-denied');
}

// Tests that closing the app window before the HID request is answered will
// work correctly.
// This is meant to verify that no mojo callbacks will be dropped in such case.
function testHidCloseWindow() {
  var webview = embedder.setUpGuest_();
  var onPermissionRequest = function(e) {
    // Intentionally leave the request pending. The test will continue on the
    // C++ side.
    e.preventDefault();
    embedder.test.succeed();
    // Prevent the automatic denial that would happen if the request were
    // garbage collected.
    window.keepRequestPending = e.request;
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadStop_(webview, 'testHid');
}

embedder.test.testList = {
  'testAllowGeolocation': testAllowGeolocation,
  'testDenyGeolocation': testDenyGeolocation,
  'testAllowCamera': testAllowCamera,
  'testDenyCamera': testDenyCamera,
  'testAllowMicrophone': testAllowMicrophone,
  'testDenyMicrophone': testDenyMicrophone,
  'testAllowMedia': testAllowMedia,
  'testDenyMedia': testDenyMedia,
  'testAllowHid': testAllowHid,
  'testDenyHid': testDenyHid,
  'testHidCloseWindow': testHidCloseWindow,
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp_(config);
    chrome.test.sendMessage('Launched');
  });
};
