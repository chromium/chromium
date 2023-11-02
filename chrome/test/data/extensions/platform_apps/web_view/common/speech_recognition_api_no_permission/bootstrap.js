// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

config.IS_CHROME_TEST = true;
// No TestServer.
config.IS_JS_ONLY_GUEST = true;
config.TEST_DIR = 'speech_recognition_api_no_permission';

var g_allowRequest;
var speechTests = {};

var LOG = function(msg) {
  window.console.log(msg);
};

var startTest = function() {
  var container = document.createElement('div');
  container.id = 'webview-tag-container';
  document.body.appendChild(container);

  chrome.test.getConfig(function(chromeConfig) {
    LOG('getConfig: ' + chromeConfig);
    utils.setUp(chromeConfig, config);
    embedder.loadGuest(function() {
      LOG('loadGuest done, start Running test');
      chrome.test.runTests([
        speechTests.testAllow,
        speechTests.testDeny
      ]);
    }, function(data) {
      if (data[0] != 'recognition') {
        utils.test.fail();
        return;
      }
      LOG('embedder.onPostMessageReceived: ' + data[1]);
      switch (data[1]) {
        case 'onerror':
          utils.test.succeed();
          break;
        case 'onresult':
        case 'onstart':
        default:
          utils.test.fail();
          break;

      }
      return /* handled */ true;
    });
  });
};


var onPermissionRequest = function(e) {
  LOG('onPermissionRequest');
  utils.test.assertEq('media', e.permission);
  if (g_allowRequest) {
    e.request.allow();
  } else {
    e.request.deny();
  }
};

speechTests.testHelper_ = function(expectSpeech, allowRequest) {
  g_allowRequest = allowRequest;
  embedder.webview.addEventListener(
      'permissionrequest', onPermissionRequest);
  embedder.webview.contentWindow.postMessage(
      JSON.stringify(['runSpeechRecognitionAPI']), '*');
};

// Tests.
speechTests.testAllow = function testAllow() {
  speechTests.testHelper_(false /* expectSpeech */, true /* allowRequest */);
};

speechTests.testDeny = function testDeny() {
  speechTests.testHelper_(false /* expectSpeech */, false /* allowRequest */);
};

// Run test(s).
startTest();
