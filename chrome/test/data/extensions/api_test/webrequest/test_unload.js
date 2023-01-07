// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file extends framework.js with functionality for testing the behavior of
// the webRequest API upon unloading a frame or tab. Loading test_unload.html?N
// causes the tests in test_unloadN.js to be run. Each file contains tests for
// a single tab (opposed to testing multiple tabs in a file to avoid unnecessary
// dependencies in the value of "tabId" between unrelated tests).

// Returns a URL of a page that generates a response after waiting for a long
// while. Callers are expected to abort the request as soon as possible.
// |hostname| can be set to make sure that the frame is created in a new process
// if site isolation is enabled.
function getSlowURL(hostname) {
  // Waiting for 10 seconds should be more than sufficient.
  return getServerURL('slow?10', hostname);
}

function getInitiatorURLForExtension() {
  var url = getURL('');
  return url.slice(0, -1);
}

function getInitiatorURLForHostname(hostname) {
  var url = getServerURL('', hostname);
  return url.slice(0, -1);
}

// Get the URL of a page that inserts a frame with the given URL upon load.
function getPageWithFrame(frameUrl, hostname) {
  return getServerURL('extensions/api_test/webrequest/unload/load_frame.html?' +
      encodeURIComponent(frameUrl), hostname);
}

// Invokes |callback| when when the onSendHeaders event occurs. When used with
// getSlowURL(), this signals when the request has been processed and that there
// won't be any webRequest events for a long while.
// This allows the test to deterministically cancel the request, which should
// trigger onErrorOccurred.
function waitUntilSendHeaders(type, url, callback) {
  chrome.test.assertTrue(/^https?:.+\/slow\?/.test(url),
      'Must be a slow URL, but was ' + url);

  chrome.webRequest.onSendHeaders.addListener(function listener() {
    chrome.webRequest.onSendHeaders.removeListener(listener);
    callback();
  }, {
    types: [type],
    urls: [url],
  });
}

(function() {
  // Load the actual test file.
  var id = location.search.slice(1);
  chrome.test.assertTrue(/^\d+$/.test(id),
      'Page URL should end with digits, but got ' + id);
  console.log('Running test_unload ' + id);

  var s = document.createElement('script');
  // test_unload1.js, test_unload2.js, ..., etc.
  // These tests are in separate files to make sure that the tests are
  // independent of each other. If they were put in one file, then the tabId
  // of one test would depend on the number of tabs from the previous tests.
  s.src = 'test_unload' + id + '.js';
  s.onerror = function() {
    chrome.test.fail('Failed to load test ' + s.src);
  };

  // At the next test, a call to RunExtensionSubtest causes the extension to
  // reload. As a result, all extension pages are closed. If the extension page
  // was the only tab in the browser, then the browser would exit and cause the
  // test to end too early. To avoid this problem, create an extra non-extension
  // tab before starting tests.
  chrome.tabs.create({
    url: 'data:,'
  }, function() {
    document.body.appendChild(s);
  });
})();
