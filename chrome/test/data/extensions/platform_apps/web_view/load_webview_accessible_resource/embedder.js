// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.guestURL = '';

window.runTest = function(testName) {
  if (testName == 'testLoadWebviewAccessibleResource') {
    testLoadWebviewAccessibleResource();
  } else if (testName == 'testReloadWebviewAccessibleResource') {
    testReloadWebviewAccessibleResource();
  } else if (testName == 'testBlobInWebviewAccessibleResource') {
    testBlobInWebviewAccessibleResource();
  } else if (testName == 'testLoadWebviewInaccessibleResource') {
    testLoadWebviewInaccessibleResource();
  } else if (testName == 'testNavigateGuestToWebviewAccessibleResource') {
    testNavigateGuestToWebviewAccessibleResource();
  } else {
    window.console.log('Incorrect testName: ' + testName);
    chrome.test.sendMessage('TEST_FAILED');
  }
}

function testLoadWebviewAccessibleResource() {
  var webview = document.querySelector('webview');

  webview.addEventListener('loadstop', function() {
    webview.executeScript(
        {code: "document.querySelector('img').naturalWidth"}, function(result) {
          // If the test image loads successfully, it will have a |naturalWidth|
          // of 17, and the test passes.
          if (result[0] == 17)
            chrome.test.sendMessage('TEST_PASSED');
          else
            chrome.test.sendMessage('TEST_FAILED');
        });
  });

  webview.src = embedder.guestURL;
};

function testNavigateGuestToWebviewAccessibleResource() {
  var webview = document.querySelector('webview');

  webview.addEventListener('loadstop', function() {
    webview.executeScript(
        {code: 'document.body.innerText'}, function(result) {
          // If the test html loads successfully, it will have a body
          // containing the text "Foo" (and the test passes in this case).
          if (result == "Foo")
            chrome.test.sendMessage('TEST_PASSED');
          else
            chrome.test.sendMessage('TEST_FAILED');
        });
  });

  webview.src =
      'chrome-extension://lmnhajohhbenlbinimlfhbpnciehhmao/assets/foo.html';
};

function testReloadWebviewAccessibleResource() {
  var webview = document.querySelector('webview');
  var didReload = false;

  webview.addEventListener('loadstop', function() {
    if (didReload) {
      // Check that the webview loaded the content correctly.
      webview.executeScript(
          {code: 'document.body.innerText'}, function(result) {
            if (result == 'Foo')
              chrome.test.sendMessage('TEST_PASSED');
            else {
              console.log('webview content is incorrect: ' + result);
              chrome.test.sendMessage('TEST_FAILED');
            }
          });
    } else {
      webview.executeScript({code: 'location.reload();'});
      didReload = true;
    }
  });
  webview.src = '/assets/foo.html';
}

function testBlobInWebviewAccessibleResource() {
  var webview = document.querySelector('webview');
  var frameCreated = false;

  webview.addEventListener('loadstop', function() {
    if (frameCreated)
      return;
    var script =
        "var blob = new Blob(['<html><body>Blob content</body></html>']," +
        "                    {type: 'text/html'});" +
        "var blobURL = URL.createObjectURL(blob);" +
        "var frame = document.createElement('iframe');" +
        "document.body.appendChild(frame);" +
        "frame.onload = function() {" +
        "  chrome.test.sendMessage('TEST_PASSED');" +
        "};" +
        "frame.src = blobURL;";
    webview.executeScript({code: script});
    frameCreated = true;
  });
  webview.src = '/assets/foo.html';
}

function testLoadWebviewInaccessibleResource() {
  var webview = document.querySelector('webview');
  var didNavigate = false;

  // Once the webview loads /foo.html, instruct it to navigate to a
  // non-webview-accessible resource.
  webview.addEventListener('loadstop', function() {
    if (didNavigate)
      return;

    var inaccessibleURL = self.origin + "/inaccessible.html";
    webview.executeScript({code: 'location="' + inaccessibleURL + '";'});
    didNavigate = true;
  });

  // The inaccessible URL should be blocked, and the webview should stay at
  // foo.html.
  webview.addEventListener('loadabort', function(e) {
    if (e.reason != 'ERR_BLOCKED_BY_CLIENT') {
      console.log("incorrect error reason in loadabort: " + e.reason);
      chrome.test.sendMessage('TEST_FAILED');
    }

    // Check that the webview content hasn't changed.
    webview.executeScript({code: 'document.body.innerText'}, function(result) {
      if (result == 'Foo') {
        chrome.test.sendMessage('TEST_PASSED');
      } else {
        console.log('webview content is incorrect: ' + result);
        chrome.test.sendMessage('TEST_FAILED');
      }
    });
  });

  webview.src = '/assets/foo.html';
}

onload = function() {
  chrome.test.getConfig(function(config) {
    if (config.testServer) {
      embedder.guestURL =
          'http://localhost:' + config.testServer.port +
          '/extensions/platform_apps/web_view/' +
          'load_webview_accessible_resource/guest.html';
    }
    chrome.test.sendMessage('Launched');
  });
};
