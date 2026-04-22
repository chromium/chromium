// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ProxySettingsApiTest.ProxyEventsParseError

const EXPECTED_ERROR = {
  details: `line: 1: Uncaught SyntaxError: Unexpected token '!'`,
  error: 'net::ERR_PAC_SCRIPT_FAILED',
  fatal: false,
};

function test() {
  // Install error handler and get the test server config.
  chrome.proxy.onProxyError.addListener(function(error) {
    chrome.test.assertEq(EXPECTED_ERROR, error);
    chrome.test.notifyPass();
  });

  // Set an invalid PAC script. This should trigger a proxy errors.
  const config = {
    mode: 'pac_script',
    pacScript: {
      data: 'trash!-FindProxyForURL',
      mandatory: false,
    },
  };
  chrome.proxy.settings.set({value: config}, testDone);
}

function testDone() {
  // Do nothing. The test success/failure is decided in the event handler.
}

test();
