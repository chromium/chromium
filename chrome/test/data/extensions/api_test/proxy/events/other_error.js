// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ProxySettingsApiTest.ProxyEventsOtherError

function test() {
  // Esure the the proxy configuration is direct.
  var config = { mode: 'direct' };
  chrome.proxy.settings.set({'value': config}, addListener);
}

function addListener() {
  // Add a listener for proxy errors. This should not be called, as we are not
  // using a proxy.
  chrome.proxy.onProxyError.addListener(function (error) {
    chrome.test.notifyFail('onProxyError unexpectedly called with: ' + error);
  });

  // Sequentially fetch two URLs. Both should generate
  // network errors (since host does not exist).
  //
  // Neither of these failed fetches should trigger
  // chrome.proxy.onProxyError, since the proxy settings are not
  // using any proxy.
  //
  // Doing two fetches serially as opposed to just one, is to
  // increase the timing window during which we wait for an
  // unexpected onProxyError callback. If the first fetch triggers
  // onProxyError, that listener should run before the second fetch
  // has failed (at which poit the test completes).
  fetchBadUrl(() => {
    fetchBadUrl(() => {
      chrome.test.notifyPass();  // Done.
    });
  });
}

function fetchBadUrl(onError) {
  // Use a unique URL so the fetch can't be served from cache.
  var url = 'http://foo.invalid/' + Math.random();

  var req = new XMLHttpRequest();
  req.open('GET', url, true);
  req.onload = function () {
    chrome.test.notifyFail('XHR is expected to fail');
  }
  req.onerror = onError;
  req.send(null);
}

test();
