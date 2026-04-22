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

const HTTP_PROXY = {
  host: '1.1.1.1',
};
const HTTP_PROXY_EXPECTED = {
  scheme: 'http',
  host: '1.1.1.1',
  port: 80,
};
const HTTPS_PROXY = {
  host: '2.2.2.2',
};
const HTTPS_PROXY_EXPECTED = {
  scheme: 'http',
  host: '2.2.2.2',
  port: 80,
};
const FTP_PROXY = {
  host: '3.3.3.3',
  port: 9000,
};
const FTP_PROXY_EXPECTED = {
  scheme: 'http',  // this is added.
  host: '3.3.3.3',
  port: 9000,
};
const FALLBACK_PROXY = {
  scheme: 'socks4',
  host: '4.4.4.4',
  port: 9090,
};
const FALLBACK_PROXY_EXPECTED = FALLBACK_PROXY;

const RULES = {
  proxyForHttp: HTTP_PROXY,
  proxyForHttps: HTTPS_PROXY,
  proxyForFtp: FTP_PROXY,
  fallbackProxy: FALLBACK_PROXY,
};
const RULES_EXPECTED = {
  proxyForHttp: HTTP_PROXY_EXPECTED,
  proxyForHttps: HTTPS_PROXY_EXPECTED,
  proxyForFtp: FTP_PROXY_EXPECTED,
  fallbackProxy: FALLBACK_PROXY_EXPECTED,
};

const CONFIG = {
  rules: RULES,
  mode: 'fixed_servers'
};
const CONFIG_EXPECTED = {
  rules: RULES_EXPECTED,
  mode: 'fixed_servers'
};

chrome.test.runTests([
  // Verify that execution has started to make sure flaky timeouts are not
  // caused by us.
  function verifyTestsHaveStarted() {
    chrome.test.succeed();
  },
  function setIndividualProxies() {
    chrome.proxy.settings.set({value: CONFIG}, chrome.test.callbackPass());
  },
  function verifyRegular() {
    chrome.proxy.settings.get(
        {incognito: false},
        expect(
            {
              value: CONFIG_EXPECTED,
              levelOfControl: 'controlled_by_this_extension'
            },
            'invalid proxy settings'));
  },
  function verifyIncognito() {
    chrome.proxy.settings.get(
        {incognito: true},
        expect(
            {
              value: CONFIG_EXPECTED,
              incognitoSpecific: false,
              levelOfControl: 'controlled_by_this_extension'
            },
            'invalid proxy settings'));
  },
]);
