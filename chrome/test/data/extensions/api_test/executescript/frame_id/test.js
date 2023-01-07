// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var relativePath = '/extensions/api_test/executescript/frame_id/frames.html';
var testOrigin = 'http://a.com:PORT';
var testUrl = 'http://a.com:PORT' + relativePath;

var tabId;

// Frame ID of every frame in this test, and the patterns of the frame URLs.
// Frame IDs are lazily initialized (and constant thereafter).
// All patterns are mutually exclusive.

// Main frame.
var ID_FRAME_TOP = 0;
var R_FRAME_TOP = /frames\.html/;
// Frame with (same-origin) about:srcdoc.
var ID_FRAME_SRCDOC;
var R_FRAME_SRCDOC = /about:srcdoc/;
// Frame with (unique-origin) sandboxed about:blank.
var ID_FRAME_SANDBOXED;
var R_FRAME_SANDBOXED = /about:blank/;
// Frame with same-origin page.
var ID_FRAME_SECOND;
var R_FRAME_SECOND = /frame\.html/;
// Same-origin child frame of |frame_second|.
var ID_FRAME_THIRD;
var R_FRAME_THIRD = /nested\.html/;
// Frame for which the extension does not have the right permissions.
var ID_FRAME_NOPERMISSION;
var R_FRAME_NOPERMISSION = /empty\.html/;

function matchesAny(urls, regex) {
  return urls.some(function(url) { return regex.test(url); });
}

var gCssCounter = 0;

// Calls chrome.tabs.insertCSS and invokes the callback with a list of affected
// URLs. This function assumes that the tab identified by |tabId| exists, and
// that |injectDetails| is a valid argument for insertCSS.
function insertCSS(tabId, injectDetails, callback) {
  var marker = (++gCssCounter) + 'px';
  injectDetails.code = 'body { min-width: ' + marker + ';}';
  chrome.tabs.insertCSS(tabId, injectDetails, function() {
    chrome.test.assertNoLastError();
    chrome.tabs.executeScript(
        tabId, {
          code: '[getComputedStyle(document.body).minWidth, document.URL];',
          allFrames: true,
          matchAboutBlank: true
        },
        function(results) {
          chrome.test.assertNoLastError();
          results = getAffectedUrls(results);
          callback(results);
        });
  });

  // Selects the results from the frames whose CSS was changed by the insertCSS
  // call, and returns the URLs of these frames.
  function getAffectedUrls(results) {
    return results.filter(function(result) {
      return result && result[0] === marker;
    }).map(function(result) {
      return result[1];  // "document.URL"
    });
  }
}

chrome.test.getConfig(function(config) {
  testOrigin = testOrigin.replace(/PORT/, config.testServer.port);
  testUrl = testUrl.replace(/PORT/, config.testServer.port);
  chrome.tabs.onUpdated.addListener(function(_, changeInfo, tab) {
    if (changeInfo.status != 'complete' || tab.id !== tabId) {
      return;
    }

    chrome.webNavigation.getAllFrames({tabId: tabId}, function(frames) {
      function getFrameId(urlRegex) {
        var filtered =
            frames.filter(function(frame) { return urlRegex.test(frame.url); });
        // Sanity check.
        chrome.test.assertEq(1, filtered.length);
        chrome.test.assertTrue(filtered[0].frameId > 0);
        return filtered[0].frameId;
      }

      ID_FRAME_SRCDOC = getFrameId(R_FRAME_SRCDOC);
      ID_FRAME_SANDBOXED = getFrameId(R_FRAME_SANDBOXED);
      ID_FRAME_SECOND = getFrameId(R_FRAME_SECOND);
      ID_FRAME_THIRD = getFrameId(R_FRAME_THIRD);
      ID_FRAME_NOPERMISSION = getFrameId(R_FRAME_NOPERMISSION);

      runTests(config);
    });
  });

  chrome.tabs.create({url: testUrl}, function(tab) { tabId = tab.id; });
});

function runTests(config) {
  // All of the following tests set the frameId parameter in the injection
  // details.
  chrome.test.runTests([
    function executeScriptInTopFrame() {
      chrome.tabs.executeScript(
          tabId, {frameId: 0, code: 'document.URL'}, pass(function(results) {
            assertEq(1, results.length);
            assertTrue(matchesAny(results, R_FRAME_TOP));
          }));
    },

    function executeScriptInTopFrameIncludingAllFrames() {
      chrome.tabs.executeScript(
          tabId, {
            frameId: 0,
            matchAboutBlank: true,
            allFrames: true,
            code: 'document.URL'
          },
          pass(function(results) {
            assertEq(5, results.length);
            assertTrue(matchesAny(results, R_FRAME_TOP));
            assertTrue(matchesAny(results, R_FRAME_SRCDOC));
            assertTrue(matchesAny(results, R_FRAME_SANDBOXED));
            assertTrue(matchesAny(results, R_FRAME_SECOND));
            assertTrue(matchesAny(results, R_FRAME_THIRD));
          }));
    },

    function executeScriptInSrcdocFrame() {
      chrome.tabs.executeScript(
          tabId, {
            frameId: ID_FRAME_SRCDOC,
            matchAboutBlank: true,
            code: 'document.URL'
          },
          pass(function(results) {
            assertEq(1, results.length);
            assertTrue(matchesAny(results, R_FRAME_SRCDOC));
          }));
    },

    function executeScriptInSrcdocFrameWithoutMatchAboutBlank() {
      chrome.tabs.executeScript(
          tabId, {frameId: ID_FRAME_SRCDOC, code: 'document.URL'},
          fail(
              'Cannot access "about:srcdoc" at origin "' + testOrigin + '". ' +
              'Extension must have permission to access the frame\'s origin, ' +
              'and matchAboutBlank must be true.'));
    },

    function executeScriptInSrcdocFrameIncludingAllFrames() {
      chrome.tabs.executeScript(
          tabId, {
            frameId: ID_FRAME_SRCDOC,
            matchAboutBlank: true,
            allFrames: true,
            code: 'document.URL'
          },
          pass(function(results) {
            assertEq(1, results.length);
            assertTrue(matchesAny(results, R_FRAME_SRCDOC));
          }));
    },

    function executeScriptInSandboxedFrame() {
      chrome.tabs.executeScript(
          tabId, {
            frameId: ID_FRAME_SANDBOXED,
            matchAboutBlank: true,
            code: 'document.URL'
          },
          pass((results) => {
            assertEq(1, results.length);
            assertTrue(matchesAny(results, R_FRAME_SANDBOXED));
          }));
    },

    function executeScriptInSubFrame() {
      chrome.tabs.executeScript(
          tabId, {frameId: ID_FRAME_SECOND, code: 'document.URL'},
          pass(function(results) {
            assertEq(1, results.length);
            assertTrue(matchesAny(results, R_FRAME_SECOND));
          }));
    },

    function executeScriptInSubFrameIncludingAllFrames() {
      chrome.tabs.executeScript(
          tabId,
          {frameId: ID_FRAME_SECOND, allFrames: true, code: 'document.URL'},
          pass(function(results) {
            assertEq(2, results.length);
            assertTrue(matchesAny(results, R_FRAME_SECOND));
            assertTrue(matchesAny(results, R_FRAME_THIRD));
          }));
    },

    function executeScriptInNestedFrame() {
      chrome.tabs.executeScript(
          tabId, {frameId: ID_FRAME_THIRD, code: 'document.URL'},
          pass(function(results) {
            assertEq(1, results.length);
            assertTrue(matchesAny(results, R_FRAME_THIRD));
          }));
    },

    function executeScriptInNestedFrameIncludingAllFrames() {
      chrome.tabs.executeScript(
          tabId,
          {frameId: ID_FRAME_THIRD, allFrames: true, code: 'document.URL'},
          pass(function(results) {
            assertEq(1, results.length);
            assertTrue(matchesAny(results, R_FRAME_THIRD));
          }));
    },

    function executeScriptInFrameWithoutPermission() {
      chrome.tabs.executeScript(
          tabId, {frameId: ID_FRAME_NOPERMISSION, code: 'document.URL'},
          fail(
              'Cannot access contents of url "http://c.com:' +
              config.testServer.port + '/empty.html". Extension manifest ' +
              'must request permission to access this host.'));
    },

    function executeScriptWithNonExistentFrameId() {
      chrome.tabs.executeScript(
          tabId, {frameId: 999999999, code: 'document.URL'},
          fail('No frame with id 999999999 in tab ' + tabId + '.'));
    },

    function executeScriptWithNegativeFrameId() {
      try {
        chrome.tabs.executeScript(
            tabId, {frameId: -1, code: 'document.URL'}, function() {
              chrome.test.fail(
                  'executeScript should never have been executed!');
            });
      } catch (e) {
        assertTrue(
            // JS-based bindings.
            e.message == 'Invalid value for argument 2. Property \'frameId\':' +
                         ' Value must not be less than 0.' ||
            // Native bindings.
            e.message == 'Error in invocation of tabs.executeScript(' +
                         'optional integer tabId, ' +
                         'extensionTypes.InjectDetails details, ' +
                         'optional function callback): Error at parameter ' +
                         '\'details\': Error at property \'frameId\': ' +
                         'Value must be at least 0.',
            e.message);
        chrome.test.succeed();
      }
    },

    function insertCSSInTopFrame() {
      insertCSS(tabId, {frameId: 0}, pass(function(results) {
                  assertEq(1, results.length);
                  assertTrue(matchesAny(results, R_FRAME_TOP));
                }));
    },

    function insertCSSInTopFrameIncludingAllFrames() {
      insertCSS(
          tabId, {frameId: 0, matchAboutBlank: true, allFrames: true},
          pass(function(results) {
            assertEq(5, results.length);
            assertTrue(matchesAny(results, R_FRAME_TOP));
            assertTrue(matchesAny(results, R_FRAME_SRCDOC));
            assertTrue(matchesAny(results, R_FRAME_SANDBOXED));
            assertTrue(matchesAny(results, R_FRAME_SECOND));
            assertTrue(matchesAny(results, R_FRAME_THIRD));
          }));
    },

    function insertCSSInSrcdocFrame() {
      insertCSS(
          tabId, {frameId: ID_FRAME_SRCDOC, matchAboutBlank: true},
          pass(function(results) {
            assertEq(1, results.length);
            assertTrue(matchesAny(results, R_FRAME_SRCDOC));
          }));
    },

    function insertCSSInSrcdocFrameWithoutMatchAboutBlank() {
      chrome.tabs.insertCSS(
          tabId, {frameId: ID_FRAME_SRCDOC, code: 'body{color:red;}'},
          fail(
              'Cannot access "about:srcdoc" at origin "' + testOrigin + '". ' +
              'Extension must have permission to access the frame\'s origin, ' +
              'and matchAboutBlank must be true.'));
    },

    function insertCSSInSrcdocFrameIncludingAllFrames() {
      insertCSS(
          tabId,
          {frameId: ID_FRAME_SRCDOC, matchAboutBlank: true, allFrames: true},
          pass(function(results) {
            assertEq(1, results.length);
            assertTrue(matchesAny(results, R_FRAME_SRCDOC));
          }));
    },

    function insertCSSInSandboxedFrame() {
      insertCSS(
          tabId, {
            frameId: ID_FRAME_SANDBOXED,
            matchAboutBlank: true,
            allFrames: true,
            code: 'body{color:red}'
          },
          pass((results) => {
            assertEq(1, results.length);
            assertTrue(matchesAny(results, R_FRAME_SANDBOXED));
          }));
    },

    function insertCSSInSubFrame() {
      insertCSS(tabId, {frameId: ID_FRAME_SECOND}, pass(function(results) {
                  assertEq(1, results.length);
                  assertTrue(matchesAny(results, R_FRAME_SECOND));
                }));
    },

    function insertCSSInSubFrameIncludingAllFrames() {
      insertCSS(
          tabId, {frameId: ID_FRAME_SECOND, allFrames: true},
          pass(function(results) {
            assertEq(2, results.length);
            assertTrue(matchesAny(results, R_FRAME_SECOND));
            assertTrue(matchesAny(results, R_FRAME_THIRD));
          }));
    },

    function insertCSSInNestedFrame() {
      insertCSS(tabId, {frameId: ID_FRAME_THIRD}, pass(function(results) {
                  assertEq(1, results.length);
                  assertTrue(matchesAny(results, R_FRAME_THIRD));
                }));
    },

    function insertCSSInNestedFrameIncludingAllFrames() {
      insertCSS(
          tabId, {frameId: ID_FRAME_THIRD, allFrames: true},
          pass(function(results) {
            assertEq(1, results.length);
            assertTrue(matchesAny(results, R_FRAME_THIRD));
          }));
    },

    function insertCSSInFrameWithoutPermission() {
      chrome.tabs.insertCSS(
          tabId, {frameId: ID_FRAME_NOPERMISSION, code: 'body{color:red}'},
          fail(
              'Cannot access contents of url "http://c.com:' +
              config.testServer.port + '/empty.html". Extension manifest ' +
              'must request permission to access this host.'));
    },

    function insertCSSWithNonExistentFrameId() {
      chrome.tabs.insertCSS(
          tabId, {frameId: 999999999, code: 'body{color:red}'},
          fail('No frame with id 999999999 in tab ' + tabId + '.'));
    },

    function insertCSSWithNegativeFrameId() {
      try {
        chrome.tabs.insertCSS(
            tabId, {frameId: -1, code: 'body{color:red}'}, function() {
              chrome.test.fail('insertCSS should never have been executed!');
            });
      } catch (e) {
        assertTrue(
            // JS-based bindings.
            e.message == 'Invalid value for argument 2. Property \'frameId\':' +
                         ' Value must not be less than 0.' ||
            // Native bindings.
            e.message == 'Error in invocation of tabs.insertCSS(' +
                         'optional integer tabId, ' +
                         'extensionTypes.InjectDetails details, ' +
                         'optional function callback): Error at parameter ' +
                         '\'details\': Error at property \'frameId\': ' +
                         'Value must be at least 0.',
            e.message);
        chrome.test.succeed();
      }
    },

  ]);
}
