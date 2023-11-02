// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple helper to keep track of how many responses are received, and fail
// if it receives a "fail" message. It's unfortunate that these are never
// cleaned up, but, realistically, doesn't matter.
function ResponseCounter() {
  this.responsesReceived = 0;
  this.expectedResponses = -1;
  var listenerFunction = function(request, sender, sendResponse) {
    if (request == 'fail') {
      chrome.test.fail('Received bad message');
    } else {
      chrome.test.assertEq('complete', request);
      ++this.responsesReceived;
      chrome.test.assertTrue(this.responsesReceived <= this.expectedResponses);
      if (this.responsesReceived == this.expectedResponses &&
          this.doneCallback) {
        this.removeListener();
        this.doneCallback();
      }
    }
  }.bind(this);
  this.removeListener = function() {
    chrome.runtime.onMessage.removeListener(listenerFunction);
  }
  chrome.runtime.onMessage.addListener(listenerFunction);
}

var waitForCommittedAndRun = function(functionToRun, numCommits, url) {
  var committedCount = 0;
  var counter = new ResponseCounter();  // Every test gets a counter.
  var onCommitted = function(details) {
    if (++committedCount == numCommits) {
      functionToRun(counter, details.tabId);
      chrome.webNavigation.onCommitted.removeListener(onCommitted);
    }
  };
  chrome.webNavigation.onCommitted.addListener(onCommitted);
  chrome.tabs.create({url: url});
};

chrome.test.getConfig(function(config) {
  var url = 'http://a.com:' + config.testServer.port +
            '/extensions/api_test/executescript/removed_frames/outer.html';
  // Regression tests for crbug.com/500574.
  chrome.test.runTests([
   function testInjectAndDeleteIframeFromMainFrame() {
      waitForCommittedAndRun(injectAndDeleteIframeFromMainFrame, 2, url);
    },
    function testInjectAndDeleteIframeFromIframe() {
      waitForCommittedAndRun(injectAndDeleteIframeFromIframe, 2, url);
    }
  ]);
});

function injectAndDeleteIframeFromMainFrame(counter, tabId) {
  // Inject code into each frame. If it's the parent frame, it removes the child
  // frame from the DOM (invalidating it). The child frame's code shouldn't
  // finish executing, since it's been removed.
  counter.expectedResponses = 1;
  counter.doneCallback = function() {
    chrome.test.assertEq(1, counter.responsesReceived);
    chrome.test.succeed();
  };
  var injectFrameCode = [
      'if (window === window.top) {',
      '  iframe = document.getElementsByTagName("iframe")[0];',
      '  iframe.parentElement.removeChild(iframe);',
      '  chrome.runtime.sendMessage("complete");',
      '}'
  ].join('\n');
  chrome.tabs.executeScript(
      tabId,
      {code: injectFrameCode, allFrames: true, runAt: 'document_idle'});
};

function injectAndDeleteIframeFromIframe(counter, tabId) {
  // Inject code into each frame. Have the child frame remove itself, deleting
  // the frame while it's still executing.
  counter.expectedResponses = 2;
  counter.doneCallback = function() {
    chrome.test.assertEq(2, counter.responsesReceived);
    chrome.test.succeed();
  };
  var injectFrameCode = [
      'if (window.self !== window.top) {',
      '  var iframe = window.top.document.getElementsByTagName("iframe")[0];',
      '  if (!iframe || iframe.contentWindow !== window)',
      '    chrome.runtime.sendMessage("fail");',
      '  else',
      '    window.top.document.body.removeChild(iframe);',
      '} else {',
      '  chrome.runtime.sendMessage("complete");',
      '}'
  ].join('\n');
  // We also use two "executeScript" calls here so that we have a pending script
  // execution on a frame that gets deleted.
  chrome.tabs.executeScript(
      tabId,
      {code: injectFrameCode, allFrames: true, runAt: 'document_idle'});
  chrome.tabs.executeScript(
      tabId,
      {code: injectFrameCode, allFrames: true, runAt: 'document_idle'});
}
