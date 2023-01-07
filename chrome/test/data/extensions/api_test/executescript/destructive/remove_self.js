// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (top === window) {
  testTop();
} else if (!parent.location.search.includes('end') ||
    window.didRunAtDocumentEnd) {
  testChild();
}

function testTop() {
  var testMessage = {};
  function reportFrames() {
    testMessage.frameCount = frames.length;
    testMessage.frameHTML = window.frameHTML;
    chrome.runtime.sendMessage(testMessage);
  }

  if (window.frameHTML) {  // Set by child frame...
    // about:blank frames are synchronously parsed, so their document_end script
    // injection happens before the main frame's injection.
    var expectChildBeforeMain = location.search.includes('?blankend');
    if (!expectChildBeforeMain) {
      // Add a message to the test notification to cause the test to fail,
      // with some useful information for diagnostics.
      testMessage.warning = 'Content script in child frame was executed ' +
          'before the main frame\'s content script!';
    }
    reportFrames();
  } else {
    window.frameHTML = '(not set)';
    window.onmessage = function(event) {
      chrome.test.assertEq('toBeRemoved', event.data);
      reportFrames();
    };
  }
}

function testChild() {
  var TEST_HOST = parent.location.hostname;

  if (TEST_HOST === 'synchronous') {
    doRemove();
  } else if (TEST_HOST === 'microtask') {
    Promise.resolve().then(doRemove);
  } else if (TEST_HOST === 'macrotask') {
    setTimeout(doRemove, 0);
  } else if (TEST_HOST.startsWith('domnodeinserted')) {
    removeOnEvent('DOMNodeInserted');
  } else if (TEST_HOST.startsWith('domsubtreemodified')) {
    removeOnEvent('DOMSubtreeModified');
  } else {
    console.error('Unexpected test: ' + TEST_HOST);
    chrome.test.fail();
  }

  function doRemove() {
    parent.frameHTML = document.documentElement.outerHTML || '(no outerHTML)';
    parent.postMessage('toBeRemoved', '*');
    // frameElement = <iframe> element in parent document.
    frameElement.remove();
  }

  function removeOnEvent(eventName) {
    var expected = parseInt(TEST_HOST.match(/\d+/)[0]);
    document.addEventListener(eventName, function() {
      // Synchronously remove the frame in the mutation event.
      if (--expected === 0)
        doRemove();
    });

    // Fallback in case the mutation events are not triggered.
    new Promise(function(resolve) {
      // The window.onload event signals that the document and its resources
      // have finished loading, so we don't expect any other parser-initiated
      // DOM mutations after that point.
      if (document.readyState === 'complete')
        resolve();
      else
        window.addEventListener('load', resolve);
    }).then(function() {
      if (expected > 0) {
        expected = 0;
        // Print this message to make it clear that the expected condition
        // (mutation event |eventName| triggered XXX times) did not happen.
        console.log('Mutation condition not triggered: ' + TEST_HOST);
        doRemove();
      }
    });

  }
}
