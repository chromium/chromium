// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Tests don't start running until an async call to
// chrome.mimeHandlerPrivate.getStreamInfo() completes, so queue any messages
// received until that point.
var queuedMessages = [];

function queueMessage(event) {
  queuedMessages.push(event);
}

window.addEventListener('message', queueMessage, false);

var streamDetails;

function fetchUrl(url) {
  return new Promise(function(resolve, reject) {
    var request = new XMLHttpRequest();
    request.onload = function() {
      resolve({
        status: request.status,
        data: request.responseText,
      });
    };
    request.onerror = function() {
      resolve({
        status: request.status,
        data: 'error',
      });
    };
    request.open('GET', streamDetails.streamUrl, true);
    request.send();
  });
}

function expectSuccessfulRead(response) {
  chrome.test.assertEq('content to read\n', response.data);
}

function expectSuccessfulReadLong(response) {
  chrome.test.assertTrue(response.data.startsWith('content to read\n'));
}

function checkStreamDetails(name, embedded) {
  checkStreamDetailsNoFile();
  chrome.test.assertEq(embedded, streamDetails.embedded);
  chrome.test.assertNe(-1, streamDetails.originalUrl.indexOf(name));
  chrome.test.assertEq('text/csv',
                       streamDetails.responseHeaders['Content-Type']);
}

function checkStreamDetailsNoFile() {
  chrome.test.assertEq('text/csv', streamDetails.mimeType);
  chrome.test.assertNe(-1, streamDetails.tabId);
}

// The following helper methods are used in BrowserPlugin-specific tests.
function dummyTouchStartHandler(e) {
}

function ensurePageIsScrollable() {
  document.body.style = " width: 100%; height: 100%; overflow: scroll;";
  let div = document.createElement("div");
  div.style = "width: 1000px; height: 500px; margin: 50%;";
  document.body.appendChild(div);
  window.scrollTo(0, 0);
}

var tests = [
  function testBasic() {
    checkStreamDetails('testBasic.csv', false);
    fetchUrl(streamDetails.streamUrl)
        .then(expectSuccessfulRead)
        .then(chrome.test.succeed);
  },

  function testEmbedded() {
    checkStreamDetails('testEmbedded.csv', true);
    fetchUrl(streamDetails.streamUrl)
        .then(expectSuccessfulRead)
        .then(chrome.test.succeed);
  },

  function testIframe() {
    checkStreamDetails('testIframe.csv', true);
    var printMessageArrived = new Promise(function(resolve, reject) {
      window.addEventListener('message', function(event) {
        chrome.test.assertEq('print', event.data.type);
        resolve();
      }, false);
    });
    var contentRead = fetchUrl(streamDetails.streamUrl)
        .then(expectSuccessfulRead);
    Promise.all([printMessageArrived, contentRead]).then(chrome.test.succeed);
  },

  function testIframeBasic() {
    checkStreamDetails('testIframeBasic.csv', true);
    fetchUrl(streamDetails.streamUrl)
        .then(expectSuccessfulRead)
        .then(chrome.test.succeed);
  },

  function testNonAsciiHeaders() {
    checkStreamDetails('testNonAsciiHeaders.csv', false);
    chrome.test.assertEq(undefined,
                         streamDetails.responseHeaders['Content-Disposition']);
    chrome.test.assertEq(undefined,
                         streamDetails.responseHeaders['ü']);
    chrome.test.succeed();
  },

  function testPostMessage() {
    var expectedMessages = ['hey', 100, 25.0];
    var messagesReceived = 0;
    function handleMessage(event) {
      if (event.data == 'succeed' &&
          messagesReceived == expectedMessages.length) {
        chrome.test.succeed();
      } else if (event.data == 'fail') {
        chrome.test.fail();
      } else if (event.data == expectedMessages[messagesReceived]) {
        event.source.postMessage(event.data, '*');
        messagesReceived++;
      } else if (event.data != 'initBeforeUnload') {
        chrome.test.fail('unexpected message ' + event.data);
      }
    }
    window.addEventListener('message', handleMessage, false);
    while (queuedMessages.length) {
      handleMessage(queuedMessages.shift());
    }
  },

  function testPostMessageUMA() {
    // The actual testing is done on the browser side. Pass so long as the
    // resource is properly fetched.
    fetchUrl(streamDetails.streamUrl)
        .then(expectSuccessfulRead)
        .then(chrome.test.succeed);
  },

  function testDataUrlLong() {
    checkStreamDetailsNoFile();
    fetchUrl(streamDetails.streamUrl)
        .then(expectSuccessfulReadLong)
        .then(chrome.test.succeed);
  },

  function testResizeBeforeAttach() {
    checkStreamDetails('testResizeBeforeAttach.csv', true);
    chrome.test.succeed();
  },

  function testFullscreen() {
    function waitForFullscreenAnimation() {
      return new Promise(resolve => {
        chrome.runtime.getPlatformInfo(info => {
          if (info.os != 'mac') {
            resolve();
            return;
          }
          // Switching in and out of fullscreen doesn't finish until the
          // animation finishes on Mac.
          window.setTimeout(resolve, 1000);
        });
      });
    }
    checkStreamDetails('testFullscreen.csv', false);
    var calls = 0;
    var windowId;
    window.addEventListener('webkitfullscreenchange', async e => {
      switch (calls) {
        case 0:  // On fullscreen entered.
          chrome.test.assertTrue(document.webkitIsFullScreen);
          chrome.test.assertEq(document.body, document.webkitFullscreenElement);
          await waitForFullscreenAnimation();
          chrome.tabs.get(streamDetails.tabId, tab => {
            chrome.test.assertNe(null, tab);
            windowId = tab.windowId;
            chrome.windows.get(windowId, currentWindow => {
              chrome.test.assertEq('fullscreen', currentWindow.state);
              // The mime handler decides to exit fullscreen.
              document.webkitExitFullscreen();
            });
          });
          break;
        case 1:  // On fullscreen exited.
          chrome.test.assertFalse(document.webkitIsFullScreen);
          chrome.test.assertEq(null, document.webkitFullscreenElement);
          await waitForFullscreenAnimation();
          chrome.windows.get(windowId, currentWindow => {
            chrome.test.assertFalse('fullscreen' == currentWindow.state,
                                    currentWindow.state);
            chrome.test.runWithUserGesture(
                () => document.body.webkitRequestFullscreen());
          });
          break;
        case 2:  // On fullscreen entered again.
          chrome.test.assertTrue(document.webkitIsFullScreen);
          chrome.test.assertEq(document.body, document.webkitFullscreenElement);
          await waitForFullscreenAnimation();
          chrome.windows.get(windowId, currentWindow => {
            chrome.test.assertEq('fullscreen', currentWindow.state);
            // Emulate the user pressing escape to exit fullscreen.
            chrome.windows.update(windowId, {state: 'normal'});
          });
          break;
        case 3:  // On fullscreen exited.
          chrome.test.assertFalse(document.webkitIsFullScreen);
          chrome.test.assertEq(null, document.webkitFullscreenElement);
          await waitForFullscreenAnimation();
          chrome.windows.get(windowId, currentWindow => {
            chrome.test.assertEq('normal', currentWindow.state);
            chrome.test.succeed();
          });
          break;
      }
      calls++;
    });
    chrome.test.runWithUserGesture(
        () => document.body.webkitRequestFullscreen());
  },

  function testFullscreenEscape() {
    checkStreamDetails('testFullscreenEscape.csv', false);
    var calls = 0;
    var windowId;
    window.addEventListener('webkitfullscreenchange', async e => {
      switch(calls) {
        case 0: // On fullscreen entered.
          chrome.test.assertTrue(document.webkitIsFullScreen);
          chrome.test.assertEq(document.body, document.webkitFullscreenElement);
          break;
        case 1: // On fullscreen exited.
          chrome.test.assertFalse(document.webkitIsFullScreen);
          chrome.test.assertEq(null, document.webkitFullscreenElement);
          chrome.test.succeed();
          break;
      }
      calls++;
    });
    chrome.test.runWithUserGesture(
        () => document.body.webkitRequestFullscreen());
  },

  function testBackgroundPage() {
    checkStreamDetails('testBackgroundPage.csv', false);
    chrome.runtime.getBackgroundPage(backgroundPage => {
      backgroundPage.startBackgroundPageTest(() => {
        // Fail if the background page receives an onSuspend event.
        chrome.test.fail('Unexpected onSuspend');
      });
      // Wait for the background page to timeout. The timeouts are set to 1ms
      // for this test, but give it 100ms just in case.
      window.setTimeout(() => {
        // If the background page has shut down, its window.localStorage will be
        // null.
        chrome.test.assertNe(null, backgroundPage.window.localStorage);
        backgroundPage.endBackgroundPageTest();
        chrome.test.succeed();
      }, 100);
    });
  },

  function testTargetBlankAnchor() {
    checkStreamDetails('testTargetBlankAnchor.csv', false);
    var anchor = document.createElement('a');
    anchor.href = 'about:blank';
    anchor.target = '_blank';
    document.body.appendChild(anchor);
    anchor.click();
    chrome.test.succeed();
  },

  function testBeforeUnloadNoDialog() {
    checkStreamDetails('testBeforeUnloadNoDialog.csv', false);
    chrome.mimeHandlerPrivate.setShowBeforeUnloadDialog(
        false, chrome.test.succeed);
  },

  function testBeforeUnloadShowDialog() {
    checkStreamDetails('testBeforeUnloadShowDialog.csv', false);
    chrome.mimeHandlerPrivate.setShowBeforeUnloadDialog(
        true, chrome.test.succeed);
  },

  // TODO(mustaq): Every test above have a unique csv, which seems redundant.
  // This particular one is used in two browser tests.
  function testBeforeUnloadWithUserActivation() {
    checkStreamDetails('testBeforeUnloadWithUserActivation.csv', false);
    chrome.mimeHandlerPrivate.setShowBeforeUnloadDialog(
        true, chrome.test.succeed);
  },
];

var testsByName = {};
for (let i = 0; i < tests.length; i++) {
  testsByName[tests[i].name] = tests[i];
}

chrome.mimeHandlerPrivate.getStreamInfo(function(streamInfo) {
  if (!streamInfo)
    return;

  // If the name of the file we're handling matches the name of a test, run that
  // test.
  var urlComponents = streamInfo.originalUrl.split('/');
  var test = urlComponents[urlComponents.length - 1].split('.')[0];
  streamDetails = streamInfo;
  if (testsByName[test]) {
    window.removeEventListener('message', queueMessage);
    chrome.test.runTests([testsByName[test]]);
  }

  // Run the test for data URLs.
  if (streamInfo.originalUrl.startsWith("data:")) {
    window.removeEventListener('message', queueMessage);
    chrome.test.runTests([testsByName['testDataUrlLong']]);
  }
});
