// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var relativePath = 'extensions/api_test/executescript/permissions/';
var port;

function fixPort(url) {
  return url.replace(/PORT/, port);
}

chrome.test.runTests([
  function setup() {
    chrome.test.getConfig(function(config) {
      port = config.testServer.port;
      chrome.test.succeed();
    });
  },

  // Test a race that used to occur here (see bug 30937).
  // Open a tab that we're not allowed to execute in (c.com), then
  // navigate it to a tab we *are* allowed to execute in (a.com),
  // then quickly run script in the tab before it navigates. It
  // should appear to work (no error -- it could have been a developer
  // mistake), but not actually do anything.
  function testRace() {
    var testFile = relativePath + 'empty.html';
    var openUrl = fixPort('http://c.com:PORT/') + testFile;
    var executeUrl = fixPort('http://a.com:PORT/') + testFile;
    var expectedError =
        'Cannot access contents of url "' + openUrl + '". ' +
        'Extension manifest must request permission to access this host.';

    // This promise waits for the second URL to finish loading.
    let tabLoadedPromise = new Promise((resolve) => {
      chrome.tabs.onUpdated.addListener(function listener(
          tabId, changeInfo, tab) {
        if (tab.status == 'complete' && tab.url == executeUrl) {
          chrome.tabs.onUpdated.removeListener(listener);
          resolve();
        }
      });
    });

    // This promise waits for both the first URL to finish loading and
    // the subsequent script execution to finish.
    let executePromise = new Promise((resolve, reject) => {
      chrome.tabs.onUpdated.addListener(function listener(
          tabId, changeInfo, tab) {
        if (tab.status == 'complete') {
          chrome.tabs.onUpdated.removeListener(listener);
          chrome.tabs.update(tab.id, {url: executeUrl});
          chrome.tabs.executeScript(
              tab.id, {file: 'script.js'},
              function(results) {
                if (results != undefined || !chrome.runtime.lastError) {
                  reject('Unexpected success in execute callback');
                } else if (chrome.runtime.lastError.message != expectedError) {
                  reject('Unexpected error: ' +
                         chrome.runtime.lastError.message);
                } else {
                  resolve();
                }
              });
        }
      })
    });

    chrome.tabs.create({url: openUrl});

    Promise.all([tabLoadedPromise, executePromise]).then(() => {
      chrome.test.succeed();
    }).catch((message) => {
      console.log(message);
      chrome.test.fail();
    });
  },

  // Inject into all frames. This should only affect frames we have
  // access to. This page has three subframes, one each from a.com,
  // b.com, and c.com. We have access to two of those, plus the root
  // frame, so we should get three responses.
  function testAllFrames() {
    var testFileFrames = relativePath + 'frames.html';
    var tabUrl = fixPort('http://a.com:PORT/') + testFileFrames;
    // A sorted list of the expected scripts results. The script returns
    // window.location.href.
    var expectedResults = [
      tabUrl,
      fixPort('http://a.com:PORT/') + relativePath + 'empty.html',
      fixPort('http://b.com:PORT/') + relativePath + 'empty.html'].sort();

    function executeScriptCallback(results) {
      chrome.test.assertEq(expectedResults, results.sort());
      chrome.test.succeed();
    }

    function updatedListener(tabId, changeInfo, tab) {
      if (tab.status == 'complete') {
        chrome.tabs.onUpdated.removeListener(updatedListener);
        chrome.tabs.executeScript(tab.id, {file: 'script.js', allFrames: true},
                                  executeScriptCallback);
      }
    }
    chrome.tabs.onUpdated.addListener(updatedListener);

    chrome.tabs.create({url: tabUrl});
  },
]);
