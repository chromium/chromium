// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) {
  window.console.log(msg);
};

var speechTests = {};

speechTests.testHelper_ = function(expectSpeech, allowRequest) {
  var webview = document.createElement('webview');
  var permissionRequested = false;

  webview.addEventListener('loadstop', function(e) {
    LOG('loadstop');
    webview.executeScript(
        {file: 'guest.js'},
        function(results) {
          LOG('done executeScript');
          if (!results || !results.length) {
            LOG('Error injecting JavaScript to guest');
            chrome.test.fail();
            return;
          }
          // Send message to establish channel.
          webview.contentWindow.postMessage(
              JSON.stringify(['create-channel']), '*');
          LOG('done postMessage');
        });
  });
  webview.addEventListener('permissionrequest', function(e) {
    permissionRequested = true;
    chrome.test.assertEq('media', e.permission);
    LOG('allowRequest?: ' + allowRequest);
    if (allowRequest) {
      // Note that since we mock speech, this path actually doesn't get
      // exercised because FakeSpeechRecognitionManager just sends down
      // the response without checking for permission.
      // TODO(lazyboy): Make FakeSpeechRecognitionManager better to address
      // this issue.
      e.request.allow();
    } else {
      e.request.deny();
    }
  });

  webview.addEventListener('consolemessage', function(e) {
    LOG('[guest]: ' + e.message + ', line: ' + e.line);
  });

  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    LOG('embedder.onPostMessageReceived: ' + data[0]);

    if (data[0] != 'recognition') {
      chrome.test.fail();
      return;
    }

    LOG('embedder.onPostMessageReceived.status: ' + data[1]);
    switch (data[1]) {
      case 'onerror':
        if (!expectSpeech) {
          // Won't happen, see TODO above.
          //chrome.test.assertTrue(permissionRequested);
          chrome.test.succeed();
        } else {
          chrome.test.assertTrue(permissionRequested);
          chrome.test.fail();
        }
        break;
      case 'onresult':
        if (expectSpeech) {
          var transcript = data[2];
          chrome.test.assertEq('Pictures of the moon', transcript);
          chrome.test.succeed();
        } else {
          chrome.test.fail();
        }
        break;
      case 'onstart':
        if (expectSpeech) {
          chrome.test.succeed();
        } else {
          chrome.test.fail();
        }
        break;
      default:
        chrome.test.fail();  // Unknown.
        break;
    }
  };
  window.addEventListener('message', onPostMessageReceived);

  webview.setAttribute('src', 'about:blank');
  document.body.appendChild(webview);
};

speechTests.denyTest = function() {
  LOG('speechTests.denyTest');
  speechTests.testHelper_(false /* expectSpeech */, false /* allowRequest */);
};

speechTests.allowTest = function() {
  LOG('speechTests.allowTest');
  speechTests.testHelper_(true /* expectSpeech */, true /* allowRequest */);
};

window.onload = function() {
  chrome.test.getConfig(function(config) {
    var testsToRun = [];
    switch (config.customArg) {
      case 'allowTest':
        testsToRun.push(speechTests.allowTest);
        break;
      case 'denyTest':
        testsToRun.push(speechTests.denyTest);
        break;
      default:
        chrome.test.fail();
        break;
    }
    chrome.test.runTests(testsToRun);
  });
};
