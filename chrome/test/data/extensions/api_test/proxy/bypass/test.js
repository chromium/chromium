// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ProxySettingsApiTest.ProxyBypass

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

var httpProxy = {
  host: "1.1.1.1"
};
var httpProxyExpected = {
  scheme: "http",
  host: "1.1.1.1",
  port: 80
};

var rules = {
  proxyForHttp: httpProxy,
  bypassList: ["localhost", "::1", "foo.bar", "<local>"]
};
var rulesExpected = {
  proxyForHttp: httpProxyExpected,
  bypassList: ["localhost", "::1", "foo.bar", "<local>"]
};

var config = { rules: rules, mode: "fixed_servers" };
var configExpected = { rules: rulesExpected, mode: "fixed_servers" };

chrome.test.runTests([
  // Verify that execution has started to make sure flaky timeouts are not
  // caused by us.
  function verifyTestsHaveStarted() {
    chrome.test.succeed();
  },
  function setIndividualProxies() {
    chrome.proxy.settings.set(
        {'value': config},
        chrome.test.callbackPass());
  },
  function verifyRegular() {
    chrome.proxy.settings.get(
        {'incognito': false},
        expect({ 'value': configExpected,
                 'levelOfControl': "controlled_by_this_extension" },
               "invalid proxy settings"));
  },
  function verifyIncognito() {
    chrome.proxy.settings.get(
        {'incognito': true},
        expect({ 'value': configExpected,
                 'incognitoSpecific': false,
                 'levelOfControl': "controlled_by_this_extension" },
               "invalid proxy settings"));
  }
]);
