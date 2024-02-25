// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ProxySettingsApiTest.ProxyFixedIndividual

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
var httpsProxy = {
  host: "2.2.2.2"
};
var httpsProxyExpected = {
  scheme: "http",
  host: "2.2.2.2",
  port: 80
};
var ftpProxy = {
  host: "3.3.3.3",
  port: 9000
};
var ftpProxyExpected = {
  scheme: "http",  // this is added.
  host: "3.3.3.3",
  port: 9000
};
var fallbackProxy = {
  scheme: "socks4",
  host: "4.4.4.4",
  port: 9090
};
var fallbackProxyExpected = fallbackProxy;

var rules = {
  proxyForHttp: httpProxy,
  proxyForHttps: httpsProxy,
  proxyForFtp: ftpProxy,
  fallbackProxy: fallbackProxy,
};
var rulesExpected = {
  proxyForHttp: httpProxyExpected,
  proxyForHttps: httpsProxyExpected,
  proxyForFtp: ftpProxyExpected,
  fallbackProxy: fallbackProxyExpected,
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
