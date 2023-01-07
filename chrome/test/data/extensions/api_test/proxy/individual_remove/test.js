// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe
//     --gtest_filter=ProxySettingsApiTest.ProxyFixedIndividualRemove

var httpProxy = {
  host: "1.1.1.1"
};
var httpsProxy = {
  scheme: "socks5",
  host: "2.2.2.2"
};
var ftpProxy = {
  host: "3.3.3.3",
  port: 9000
};
var fallbackProxy = {
  scheme: "socks4",
  host: "4.4.4.4",
  port: 9090
};

var rules = {
  proxyForHttp: httpProxy,
  proxyForHttps: httpsProxy,
  proxyForFtp: ftpProxy,
  fallbackProxy: fallbackProxy,
};

var config = { rules: rules, mode: "fixed_servers" };

chrome.test.runTests([
  // Verify that execution has started to make sure flaky timeouts are not
  // caused by us.
  function verifyTestsHaveStarted() {
    chrome.test.succeed();
  },
  function setIndividualProxies() {
    chrome.proxy.settings.set(
        {'value': config, 'scope': 'regular'},
        chrome.test.callbackPass());
  },
  function clearProxies() {
    chrome.proxy.settings.clear(
        {'scope': 'regular'},
        chrome.test.callbackPass());
  }
]);
