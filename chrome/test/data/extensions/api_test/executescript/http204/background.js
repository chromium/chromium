// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var config;
var MAIN_HOST = 'b.com';
var OTHER_HOST = 'c.com';

var DOMContentLoadedEventsInFrame = [];

chrome.test.getConfig(function(testConfig) {
  config = testConfig;

  var testUrl = 'http://' + MAIN_HOST + ':' + config.testServer.port +
    '/extensions/api_test/executescript/http204/page_with_204_frame.html';
  chrome.runtime.onMessage.addListener(function listener(msg, sender) {
    // This message should be sent when the frame and all sub frames have
    // completely finished loading.
    chrome.test.assertEq('start the test', msg);
    // Should be the top-level frame with our test page.
    chrome.test.assertEq(0, sender.frameId);
    chrome.test.assertEq(testUrl, sender.url);
    chrome.test.assertTrue(sender.tab.id > 0);

    chrome.runtime.onMessage.removeListener(listener);
    chrome.webNavigation.onDOMContentLoaded.removeListener(onDOMContentLoaded);
    // Avoid flakiness by excluding all events that are not from our tab.
    DOMContentLoadedEventsInFrame =
      DOMContentLoadedEventsInFrame.filter(function(details) {
        return details.tabId === sender.tab.id;
      });

    startTest(sender.tab.id);
  });

  chrome.webNavigation.onDOMContentLoaded.addListener(onDOMContentLoaded);
  chrome.tabs.create({
    url: testUrl
  });

  function onDOMContentLoaded(details) {
    if (details.frameId > 0) {
      DOMContentLoadedEventsInFrame.push(details);
    }
  }
});

function startTest(tabId) {
  var kDefaultColor = 'rgb(0, 0, 0)';
  var kExpectedFontFamily = '"expected font-family"';
  var kExpectedColor = 'rgb(123, 123, 123)';

  // The page has a child frame containing a HTTP 204 page.
  // In response to HTTP 204 (No Content), the browser stops navigating away and
  // stays at the previous page. In this test, the URL leading to HTTP 204 was
  // the initial URL of the frame, so in response to HTTP 204, the frame should
  // end at about:blank.

  // Each chrome.tabs.insertCSS test is followed by a test using executeScript.
  // These executeScript tests exists for two reasons:
  // - They verify the result of insertCSS
  // - They show that executeScript is working as intended.

  chrome.test.runTests([
    function insertCssTopLevelOnly() {
      // Sanity check: insertCSS can change main frame's CSS.
      chrome.tabs.insertCSS(tabId, {
        code: 'body { font-family: ' + kExpectedFontFamily + ' !important;}',
      }, chrome.test.callbackPass());
      // The result is verified hereafter, in executeScriptTopLevelOnly.
    },

    function executeScriptTopLevelOnly() {
      // Sanity check: insertCSS should really have changed the CSS.
      // Depends on insertCssTopLevelOnly.
      chrome.tabs.executeScript(tabId, {
        code: 'getComputedStyle(document.body).fontFamily',
      }, chrome.test.callbackPass(function(results) {
        chrome.test.assertEq([kExpectedFontFamily], results);
      }));
    },

    // Now we know that executeScript works in the top-level frame, we will use
    // it to check whether executeScript can execute code in the child frame.
    function verifyManifestContentScriptInjected() {
      // Check whether the content scripts from manifest.json ran in the frame.
      chrome.tabs.executeScript(tabId, {
        code: '[' +
          '[window.documentStart,' +
          ' window.documentEnd,' +
          ' window.documentIdle],' +
          '[frames[0].documentStart,' +
          ' frames[0].documentEnd,' +
          ' frames[0].documentIdle],' +
          '[frames[0].didRunAtDocumentStartUnexpected,' +
          ' frames[0].didRunAtDocumentEndUnexpected,' +
          ' frames[0].didRunAtDocumentIdleUnexpected],' +
          ']',
      }, chrome.test.callbackPass(function(results) {
        chrome.test.assertEq([[
            // Should always run in top frame because of matching match pattern.
            [1, 1, 1],

            [
              // Before the response from the server is received, the frame
              // displays an empty document. This document has a <html> element,
              // it can be scripted by the parent frame and its URL as shown to
              // scripts is about:blank.
              // Because the content script's match_about_blank flag is set to
              // true in manifest.json, and its URL pattern matches the parent
              // frame's URL and, the document_start script should be run.
              // TODO(robwu): This should be 1 for the reason above, but it is
              // null because the script is not injected (crbug.com/511057).
              null,
              // Does not run at document_end and document_idle because the
              // DOMContentLoaded event is not triggered either.
              null,
              null,
            ],

            // Should not run scripts in child frame because the page load was
            // not committed, and the URL pattern (204 page) doesn't match.
            [null, null, null],
        ]], results);
      }));
    },

    // document_end and document_idle scripts are not run in the child frame
    // because we assume that the DOMContentLoaded event is not triggered in
    // frames after a failed provisional load. Verify that the DOMContentLoaded
    // event was indeed NOT triggered.
    function checkDOMContentLoadedEvent() {
      chrome.test.assertEq([], DOMContentLoadedEventsInFrame);
      chrome.test.succeed();
    },

    function insertCss204NoAbout() {
      // HTTP 204 = stay at previous page, which was a blank page, so insertCSS
      // without matchAboutBlank shouldn't change the frame's CSS.
      chrome.tabs.insertCSS(tabId, {
        code: 'body { color: ' + kExpectedColor + '; }',
        allFrames: true,
      }, chrome.test.callbackPass());
      // The result is verified hereafter, in verifyInsertCss204NoAbout.
    },

    function verifyInsertCss204NoAbout() {
      // Depends on insertCss204NoAbout.
      chrome.tabs.executeScript(tabId, {
        code: 'frames[0].getComputedStyle(frames[0].document.body).color',
      }, chrome.test.callbackPass(function(results) {
        // CSS should not be inserted in frame because it's about:blank.
        chrome.test.assertEq([kDefaultColor], results);
      }));
    },

    function insertCss204Blank() {
      chrome.tabs.insertCSS(tabId, {
        code: 'body { color: ' + kExpectedColor + '; }',
        allFrames: true,
        matchAboutBlank: true,
      }, chrome.test.callbackPass());
      // The result is verified hereafter, in verifyInsertCss204Blank.
    },

    function verifyInsertCss204Blank() {
      // Depends on insertCss204Blank.
      chrome.tabs.executeScript(tabId, {
        code: 'frames[0].getComputedStyle(frames[0].document.body).color',
      }, chrome.test.callbackPass(function(results) {
        // CSS should be inserted in frame because matchAboutBlank was true.
        chrome.test.assertEq([kExpectedColor], results);
      }));
    },

    function executeScript204NoAbout() {
      chrome.tabs.executeScript(tabId, {
        code: 'top === window',
        allFrames: true,
      }, chrome.test.callbackPass(function(results) {
        // Child frame should not be matched because it's about:blank.
        chrome.test.assertEq([true], results);
      }));
    },

    function executeScript204About() {
      chrome.tabs.executeScript(tabId, {
        code: 'top === window',
        allFrames: true,
        matchAboutBlank: true,
      }, chrome.test.callbackPass(function(results) {
        // Child frame should not be matched because matchAboutBlank was true.
        chrome.test.assertEq([true, false], results);
      }));
    },

    // Now we have verified that (programmatic) content script injection works
    // for a frame whose initial load resulted in a 204.
    // Continue with testing navigation from a child frame to a 204 page, with
    // a variety of origins for completeness.

    function loadSameOriginFrameAndWaitUntil204() {
      // This is not a test, just preparing for the next test.
      // All URLs are at the same origin.
      navigateToFrameAndWaitUntil204Loaded(tabId, MAIN_HOST, MAIN_HOST);
    },

    function verifySameOriginManifestAfterSameOrigin204() {
      checkManifestScriptsAfter204Navigation(tabId);
    },

    function loadSameOriginFrameAndWaitUntilCrossOrigin204() {
      // This is not a test, just preparing for the next test.
      // The frame is at the same origin as the top-level frame, but the 204
      // URL is at a different origin.
      navigateToFrameAndWaitUntil204Loaded(tabId, MAIN_HOST, OTHER_HOST);
    },

    function verifySameOriginManifestAfterCrossOrigin204() {
      checkManifestScriptsAfter204Navigation(tabId);
    },

    function loadCrossOriginFrameAndWaitUntil204() {
      // This is not a test, just preparing for the next test.
      // The frame's origin differs from the top-level frame, and the 204 URL is
      // at the same origin as the frame.
      navigateToFrameAndWaitUntil204Loaded(tabId, OTHER_HOST, OTHER_HOST);
    },

    function verifyCrossOriginManifestAfterSameOrigin204() {
      checkManifestScriptsAfter204Navigation(tabId);
    },

    function loadCrossOriginFrameAndWaitUntilCrossOrigin204() {
      // This is not a test, just preparing for the next test.
      // The frame's origin differs from the top-level frame, and the origin of
      // the 204 URL differs from the frame (it is incidentally the same as the
      // main frame's origin).
      navigateToFrameAndWaitUntil204Loaded(tabId, OTHER_HOST, MAIN_HOST);
    },

    function verifyCrossOriginManifestAfterCrossOrigin204() {
      checkManifestScriptsAfter204Navigation(tabId);
    },
  ]);
}

// Navigates to a page that navigates to a 204 page via a script.
function navigateToFrameAndWaitUntil204Loaded(tabId, hostname, hostname204) {
  var doneListening = chrome.test.listenForever(
      chrome.webNavigation.onErrorOccurred,
      function(details) {
        if (details.tabId === tabId && details.frameId > 0) {
          chrome.test.assertTrue(details.url.includes('page204.html'),
              'frame URL should be page204.html, but was ' + details.url);
          doneListening();
        }
      });

  var url = 'http://' + hostname + ':' + config.testServer.port +
    '/extensions/api_test/executescript/http204/navigate_to_204.html?' +
    hostname204;

  chrome.tabs.executeScript(tabId, {
    code: 'document.body.innerHTML = \'<iframe src="' + url + '"></iframe>\';',
  });
}

// Checks whether the content scripts were run as expected in the frame that
// just received a failed provisional load (=received 204 reply).
function checkManifestScriptsAfter204Navigation(tabId) {
  chrome.tabs.executeScript(tabId, {
    allFrames: true,
    code: '[' +
      '[window.documentStart,' +
      ' window.documentEnd,' +
      ' performance.timing.domContentLoadedEventStart > 0],' +
      '[window.didRunAtDocumentStartUnexpected,' +
      ' window.didRunAtDocumentEndUnexpected],' +
      ']',
  }, chrome.test.callbackPass(function(results) {
    chrome.test.assertEq(2, results.length);
    // Main frame. Should not be affected by child frame navigations.
    chrome.test.assertEq([[1, 1, true], [null, null]], results[0]);

    // Child frame.
    if (!results[1][0][2]) {  // = if DOMContentLoaded did not run.
      // If the 204 reply was handled faster than the parsing of the frame
      // document, then the DOMContentLoaded event won't be triggered.
      chrome.test.assertEq([
          // The 204 navigation was triggered by the page, so the document_start
          // script should have run by then. But since DOMContentLoaded is not
          // triggered, the document_end script should not run either.
          [1, null, false],
          // Should not inject non-matching scripts.
          [null, null],
      ], results[1]);
      return;
    }
    chrome.test.assertEq([
        // Should run the content scripts even after a navigation to 204.
        [1, 1, true],
        // Should not inject non-matching scripts.
        [null, null],
    ], results[1]);
  }));
}
