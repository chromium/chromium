// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var EVENT_MESSAGE_EXTENSION_STRING = "Extension";

// Constants as functions, not to be called until after runTests.
function getURLHttpSimpleLoad() {
  return getServerURL('extensions/api_test/webrequest/simpleLoad/a.html');
}

function getServerURL(path) {
  return 'http://www.a.com:' + testServerPort + '/' + path;
}

function runTests(tests) {
  chrome.test.getConfig(function(config) {
    testServerPort = config.testServer.port;
    chrome.test.runTests(tests);
  });
}

function size(obj) {
  var size = 0, key;
  for (key in obj) {
    if (obj.hasOwnProperty(key)) size++;
  }
  return size;
};

runTests([
  function testSendMessage() {
    var expectedEvents = {
      "onBeforeRequest-Extension": 1,
      "onBeforeSendHeaders-Extension": 1,
      "onHeadersReceived-Extension": 1
      // "onAuthRequired-Extension" is not sent for this test case.
    };
    var done = chrome.test.listenForever(
        chrome.declarativeWebRequest.onMessage,
        function(details) {
      if (EVENT_MESSAGE_EXTENSION_STRING != details.message) {
        chrome.test.fail("Invalid message: " + details.message);
      }
      chrome.test.assertEq(getURLHttpSimpleLoad(), details.url);
      chrome.test.assertEq('outermost_frame', details.frameType);
      chrome.test.assertEq('active', details.documentLifecycle);
      chrome.test.assertFalse('parentDocumentId' in details);
      chrome.test.assertFalse('documentId' in details);
      var messageKey = details.stage + "-" + details.message;
      if (messageKey in expectedEvents) {
        delete expectedEvents[messageKey];
        if (size(expectedEvents) == 0) {
          done();
        }
      } else {
        chrome.test.fail();
      }
    });

    chrome.declarativeWebRequest.onRequest.removeRules(null, function() {
      var rule = {
        conditions: [
          new chrome.declarativeWebRequest.RequestMatcher(
              {url: {urlEquals: getURLHttpSimpleLoad()}}),
        ],
        actions: [
          new chrome.declarativeWebRequest.SendMessageToExtension(
              {message: EVENT_MESSAGE_EXTENSION_STRING}),
        ],
      };
      chrome.declarativeWebRequest.onRequest.addRules([rule], function() {
        chrome.tabs.create({"url": getURLHttpSimpleLoad()});
      });
    });
  }
]);
