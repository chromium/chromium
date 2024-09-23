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

// Tests begin. All permissions will be denied.

function testAllowGeolocation() {
  navigator.permissions.query({name: 'geolocation'}).then(function(permission) {
    // TODO(crbug.com/40215363) Permission's state is `prompt` even despite it is
    // not declared in the manifest.
    if (permission.state === 'prompt') {
      var webview = embedder.setUpGuest_();

      var onPermissionRequest = function(e) {
        e.request.allow();
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

function testDenyGeolocation() {
  navigator.permissions.query({name: 'geolocation'}).then(function(permission) {
    // TODO(crbug.com/40215363) Permission's state is `prompt` even despite it is
    // not declared in the manifest.
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
    // TODO(crbug.com/40215363) Permission's state is `prompt` even despite it is
    // not declared in the manifest.
    if (permission.state === 'prompt') {
      var webview = embedder.setUpGuest_();
      var onPermissionRequest = function(e) {
        e.request.allow();
      };
      webview.addEventListener('permissionrequest', onPermissionRequest);

      embedder.setUpLoadStop_(webview, 'testCamera');
      embedder.registerAndWaitForPostMessage_('testCamera', 'access-denied');

    } else {
      embedder.test.fail();
    }
  });
}

function testDenyCamera() {
  navigator.permissions.query({name: 'camera'}).then(function(permission) {
    // TODO(crbug.com/40215363) Permission's state is `prompt` even despite it is
    // not declared in the manifest.
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
    // TODO(crbug.com/40215363) Permission's state is `prompt` even despite it is
    // not declared in the manifest.
    if (permission.state === 'prompt') {
      var webview = embedder.setUpGuest_();
      var onPermissionRequest = function(e) {
        e.request.allow();
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
function testDenyMicrophone() {
  navigator.permissions.query({name: 'microphone'}).then(function(permission) {
    // TODO(crbug.com/40215363) Permission's state is `prompt` even despite it is
    // not declared in the manifest.
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
  embedder.registerAndWaitForPostMessage_('testMedia', 'access-denied');
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

embedder.test.testList = {
  'testAllowGeolocation': testAllowGeolocation,
  'testDenyGeolocation': testDenyGeolocation,
  'testAllowCamera': testAllowCamera,
  'testDenyCamera': testDenyCamera,
  'testAllowMicrophone': testAllowMicrophone,
  'testDenyMicrophone': testDenyMicrophone,
  'testAllowMedia': testAllowMedia,
  'testDenyMedia': testDenyMedia
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp_(config);
    chrome.test.sendMessage('Launched');
  });
};
