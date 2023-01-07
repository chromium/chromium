// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ProxySettingsApiTest.ProxyEventsInvalidProxy

var expected_error = {
    error: 'net::ERR_PROXY_CONNECTION_FAILED',
    details: '',
    fatal: true
};

function test() {
  // Install error handler and get the test server config.
  chrome.proxy.onProxyError.addListener(function (error) {
    chrome.test.assertEq(expected_error, error);
    chrome.test.notifyPass();
  });

  // Set an invalid proxy and fire off an XHR. This should trigger proxy errors.
  // There may be any number of proxy errors, as systems like safe browsing
  // might start network traffic as well.
  var rules = {
    singleProxy: { host: 'does.not.exist' }
  };
  var config = { rules: rules, mode: 'fixed_servers' };
  chrome.proxy.settings.set({'value': config}, sendFailingXHR);
}

function sendFailingXHR() {
  // The URL for the XHR doesn't matter, since it will be sent through an HTTP
  // proxy server (and the proxy server is unreachable).
  var url = 'http://example.test/';

  var req = new XMLHttpRequest();
  req.open('GET', url, true);
  req.onload = function () {
    chrome.test.notifyFail('proxy settings should not work');
  }
  req.onerror = testDone;
  req.send(null);
}

function testDone() {
 // Do nothing. The test success/failure is decided in the event handler.
}

test();
